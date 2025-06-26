#ifdef __linux__

  #include <algorithm>
  #include <arpa/inet.h>          // inet_ntop
  #include <chrono>               // std::chrono::minutes
  #include <climits>              // PATH_MAX
  #include <cpuid.h>              // __get_cpuid
  #include <cstring>              // std::strlen
  #include <expected>             // std::{unexpected, expected}
  #include <filesystem>           // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
  #include <format>               // std::{format, format_to_n}
  #include <fstream>              // std::ifstream
  #include <glaze/beve/read.hpp>  // glz::read_beve
  #include <glaze/beve/write.hpp> // glz::write_beve
  #include <ifaddrs.h>            // getifaddrs, freeifaddrs, ifaddrs
  #include <matchit.hpp>          // matchit::{is, is_not, is_any, etc.}
  #include <net/if.h>             // IFF_UP, IFF_LOOPBACK
  #include <netdb.h>              // getnameinfo, NI_NUMERICHOST
  #include <netinet/in.h>         // sockaddr_in
  #include <string>               // std::{getline, string (String)}
  #include <string_view>          // std::string_view (StringView)
  #include <sys/socket.h>         // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
  #include <sys/statvfs.h>        // statvfs
  #include <sys/sysinfo.h>        // sysinfo
  #include <sys/utsname.h>        // utsname, uname
  #include <unistd.h>             // readlink
  #include <utility>              // std::move
  #ifdef HAVE_XCB
    #include <xcb/randr.h>
  #endif
  #include <linux/if_packet.h> // sockaddr_ll
  #include <map>               // std::map
  #include <sstream>           // std::istringstream

  #include <Drac++/Core/System.hpp>
  #include <Drac++/Services/Packages.hpp>

  #include <Drac++/Utils/Caching.hpp>
  #include <Drac++/Utils/Definitions.hpp>
  #include <Drac++/Utils/Env.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Logging.hpp>
  #include <Drac++/Utils/Types.hpp>

  #include "Wrappers/DBus.hpp"
  #include "Wrappers/Wayland.hpp"
  #include "Wrappers/XCB.hpp"

using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
using namespace draconis::utils::types;
namespace fs = std::filesystem;

// clang-format off
#ifdef __GLIBC__
extern "C" fn issetugid() -> usize { return 0; } // NOLINT(readability-identifier-naming) - glibc function stub
#endif
// clang-format on

namespace {
  template <std::integral T>
  fn try_parse(std::string_view sview) -> Option<T> {
    T value;

    auto [ptr, ec] = std::from_chars(sview.data(), sview.data() + sview.size(), value);

    if (ec == std::errc() && ptr == sview.data() + sview.size())
      return value;

    return None;
  }

  fn LookupPciNamesFromStream(std::istream& pciStream, const StringView vendor_id_in, const StringView device_id_in) -> Pair<String, String> {
    const String vendorId = vendor_id_in.starts_with("0x") ? String(vendor_id_in.substr(2)) : String(vendor_id_in);
    const String deviceId = device_id_in.starts_with("0x") ? String(device_id_in.substr(2)) : String(device_id_in);

    String line;
    String currentVendorName;
    bool   vendorFound = false;

    while (std::getline(pciStream, line)) {
      if (line.empty() || line[0] == '#')
        continue;

      if (line[0] != '\t') {
        vendorFound = false;
        if (line.starts_with(vendorId)) {
          vendorFound = true;
          if (const usize namePos = line.find("  "); namePos != String::npos)
            currentVendorName = line.substr(namePos + 2);
        }
      } else if (vendorFound && line.starts_with('\t') && line[1] != '\t')
        if (line.starts_with(std::format("\t{}", deviceId)))
          if (const usize namePos = line.find("  "); namePos != String::npos)
            return { currentVendorName, line.substr(namePos + 2) };
    }

    return { "", "" };
  }

  fn ReadSysFile(const std::filesystem::path& path) -> Result<String> {
    std::ifstream file(path);
    if (!file.is_open())
      return Err(DracError(NotFound, std::format("Failed to open sysfs file: {}", path.string())));

    String line;

    if (std::getline(file, line)) {
      if (const usize pos = line.find_last_not_of(" \t\n\r"); pos != String::npos)
        line.erase(pos + 1);

      return line;
    }

    return Err(DracError(ParseError, std::format("Failed to read from sysfs file: {}", path.string())));
  }

  #ifdef USE_LINKED_PCI_IDS
  extern "C" {
    extern const char _binary_pci_ids_start[];
    extern const char _binary_pci_ids_end[];
  }

  fn LookupPciNamesFromMemory(const StringView vendor_id_in, const StringView device_id_in) -> Pair<String, String> {
    const size_t       pciIdsLen = _binary_pci_ids_end - _binary_pci_ids_start;
    std::istringstream pciStream(String(_binary_pci_ids_start, pciIdsLen));
    return LookupPciNamesFromStream(pciStream, vendor_id_in, device_id_in);
  }
  #else
  fn FindPciIDsPath() -> const fs::path& {
    const Array<fs::path, 3> known_paths = {
      "/usr/share/hwdata/pci.ids",
      "/usr/share/misc/pci.ids",
      "/usr/share/pci.ids"
    };

    for (const auto& path : known_paths)
      if (fs::exists(path)) {
        return path;
        break;
      }

    return {};
  }

  fn LookupPciNamesFromFile(const StringView VendorIDIn, const StringView DeviceIDIn) -> Pair<String, String> {
    const fs::path& pci_ids_path = FindPciIDsPath();

    if (pci_ids_path.empty())
      return { "", "" };

    std::ifstream file(pci_ids_path);

    if (!file)
      return { "", "" };

    return LookupPciNamesFromStream(file, VendorIDIn, DeviceIDIn);
  }
  #endif

  fn LookupPciNames(const StringView vendorId, const StringView deviceId) -> Pair<String, String> {
  #ifdef USE_LINKED_PCI_IDS
    return LookupPciNamesFromMemory(vendorId, deviceId);
  #else
    return LookupPciNamesFromFile(vendorId, deviceId);
  #endif
  }

  fn CleanGpuModelName(String vendor, String device) -> String {
    if (vendor.find("[AMD/ATI]") != String::npos)
      vendor = "AMD";
    else if (const usize pos = vendor.find(' '); pos != String::npos)
      vendor = vendor.substr(0, pos);

    if (const usize openPos = device.find('['); openPos != String::npos)
      if (const usize closePos = device.find(']', openPos); closePos != String::npos)
        device = device.substr(openPos + 1, closePos - openPos - 1);

    fn trim = [](String& str) {
      if (const usize pos = str.find_last_not_of(" \t\n\r"); pos != String::npos)
        str.erase(pos + 1);
      if (const usize pos = str.find_first_not_of(" \t\n\r"); pos != String::npos)
        str.erase(0, pos);
    };

    trim(vendor);
    trim(device);

    return std::format("{} {}", vendor, device);
  }

  #ifdef HAVE_XCB
  fn GetX11WindowManager() -> Result<String> {
    using namespace XCB;
    using namespace matchit;
    using enum ConnError;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = ConnectionHasError(conn.get()))
        return Err(
          DracError(
            ApiUnavailable,
            match(err)(
              is | Generic         = "Stream/Socket/Pipe Error",
              is | ExtNotSupported = "Extension Not Supported",
              is | MemInsufficient = "Insufficient Memory",
              is | ReqLenExceed    = "Request Length Exceeded",
              is | ParseErr        = "Display String Parse Error",
              is | InvalidScreen   = "Invalid Screen",
              is | FdPassingFailed = "FD Passing Failed",
              is | _               = std::format("Unknown Error Code ({})", err)
            )
          )
        );

    fn internAtom = [&conn](const StringView name) -> Result<Atom> {
      const ReplyGuard<IntAtomReply> reply(InternAtomReply(conn.get(), InternAtom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr));

      if (!reply)
        return Err(DracError(PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name)));

      return reply->atom;
    };

    const Result<Atom> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<Atom> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<Atom> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DracError(PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<GetPropReply> wmWindowReply(GetPropertyReply(
      conn.get(),
      GetProperty(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        GetPropertyValueLength(wmWindowReply.get()) == 0)
      return Err(DracError(NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const Window wmRootWindow = *static_cast<Window*>(GetPropertyValue(wmWindowReply.get()));

    const ReplyGuard<GetPropReply> wmNameReply(GetPropertyReply(
      conn.get(),
      GetProperty(
        conn.get(),
        0,
        wmRootWindow,
        *wmNameAtom,
        *utf8StringAtom,
        0,
        1024
      ),
      nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || GetPropertyValueLength(wmNameReply.get()) == 0)
      return Err(DracError(NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(GetPropertyValue(wmNameReply.get()));
    const usize length   = GetPropertyValueLength(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetX11Displays() -> Result<Vec<Display>> {
    using namespace XCB;

    DisplayGuard conn;
    if (!conn)
      return Err(DracError(ApiUnavailable, "Failed to connect to X server"));

    const auto* setup = conn.setup();
    if (!setup)
      return Err(DracError(ApiUnavailable, "Failed to get X server setup"));

    const ReplyGuard<xcb_query_extension_reply_t> randrQueryReply(
      xcb_query_extension_reply(conn.get(), xcb_query_extension(conn.get(), std::strlen("RANDR"), "RANDR"), nullptr)
    );

    if (!randrQueryReply || !randrQueryReply->present)
      return Err(DracError(NotSupported, "X server does not support RANDR extension"));

    Screen* screen = conn.rootScreen();
    if (!screen)
      return Err(DracError(NotFound, "Failed to get X root screen"));

    const ReplyGuard<xcb_randr_get_screen_resources_current_reply_t> screenResourcesReply(
      xcb_randr_get_screen_resources_current_reply(
        conn.get(), xcb_randr_get_screen_resources_current(conn.get(), screen->root), nullptr
      )
    );

    if (!screenResourcesReply)
      return Err(DracError(ApiUnavailable, "Failed to get screen resources"));

    xcb_randr_output_t* outputs     = xcb_randr_get_screen_resources_current_outputs(screenResourcesReply.get());
    const int           outputCount = xcb_randr_get_screen_resources_current_outputs_length(screenResourcesReply.get());

    if (outputCount == 0)
      return Err(DracError(NotFound, "No outputs found"));

    Vec<Display> displays;
    int          primaryIndex = -1;

    const ReplyGuard<xcb_randr_get_output_primary_reply_t> primaryOutputReply(
      xcb_randr_get_output_primary_reply(conn.get(), xcb_randr_get_output_primary(conn.get(), screen->root), nullptr)
    );
    const xcb_randr_output_t primaryOutput = primaryOutputReply ? primaryOutputReply->output : XCB_NONE;

    for (int i = 0; i < outputCount; ++i) {
      const ReplyGuard<xcb_randr_get_output_info_reply_t> outputInfoReply(
        xcb_randr_get_output_info_reply(conn.get(), xcb_randr_get_output_info(conn.get(), *std::next(outputs, i), XCB_CURRENT_TIME), nullptr)
      );

      if (!outputInfoReply || outputInfoReply->crtc == XCB_NONE)
        continue;

      const ReplyGuard<xcb_randr_get_crtc_info_reply_t> crtcInfoReply(
        xcb_randr_get_crtc_info_reply(conn.get(), xcb_randr_get_crtc_info(conn.get(), outputInfoReply->crtc, XCB_CURRENT_TIME), nullptr)
      );

      if (!crtcInfoReply)
        continue;

      u16 refreshRate = 0;

      if (crtcInfoReply->mode != XCB_NONE) {
        xcb_randr_mode_info_t*         modeInfo  = nullptr;
        xcb_randr_mode_info_iterator_t modesIter = xcb_randr_get_screen_resources_current_modes_iterator(screenResourcesReply.get());
        for (; modesIter.rem; xcb_randr_mode_info_next(&modesIter)) {
          if (modesIter.data->id == crtcInfoReply->mode) {
            modeInfo = modesIter.data;
            break;
          }
        }

        if (modeInfo && modeInfo->htotal > 0 && modeInfo->vtotal > 0)
          refreshRate = static_cast<u16>(round(static_cast<f64>(modeInfo->dot_clock) / (static_cast<f64>(modeInfo->htotal) * static_cast<f64>(modeInfo->vtotal))));
      }

      bool isPrimary = (*std::next(outputs, i) == primaryOutput);
      if (isPrimary)
        primaryIndex = static_cast<int>(displays.size());

      displays.emplace_back(
        *std::next(outputs, i),
        Display::Resolution { .width = crtcInfoReply->width, .height = crtcInfoReply->height },
        refreshRate,
        isPrimary
      );
    }

    // If no display was marked as primary, set the first one as primary
    if (primaryIndex == -1 && !displays.empty()) {
      displays[0].isPrimary = true;
    } else if (primaryIndex > 0) {
      // Ensure only one display is marked as primary
      for (int i = 0; i < static_cast<int>(displays.size()); ++i) {
        if (i != primaryIndex)
          displays[i].isPrimary = false;
      }
    }

    return displays;
  }

  fn GetX11PrimaryDisplay() -> Result<Display> {
    using namespace XCB;

    DisplayGuard conn;
    if (!conn)
      return Err(DracError(ApiUnavailable, "Failed to connect to X server"));

    Screen* screen = conn.rootScreen();
    if (!screen)
      return Err(DracError(ApiUnavailable, "Failed to get X root screen"));

    const ReplyGuard<xcb_randr_get_output_primary_reply_t> primaryOutputReply(
      xcb_randr_get_output_primary_reply(conn.get(), xcb_randr_get_output_primary(conn.get(), screen->root), nullptr)
    );

    const xcb_randr_output_t primaryOutput = primaryOutputReply ? primaryOutputReply->output : XCB_NONE;

    if (primaryOutput == XCB_NONE)
      return Err(DracError(NotFound, "No primary output found"));

    const ReplyGuard<xcb_randr_get_output_info_reply_t> outputInfoReply(
      xcb_randr_get_output_info_reply(conn.get(), xcb_randr_get_output_info(conn.get(), primaryOutput, XCB_CURRENT_TIME), nullptr)
    );
    if (!outputInfoReply || outputInfoReply->crtc == XCB_NONE)
      return Err(DracError(NotFound, "Failed to get output info for primary display"));

    const ReplyGuard<xcb_randr_get_crtc_info_reply_t> crtcInfoReply(
      xcb_randr_get_crtc_info_reply(conn.get(), xcb_randr_get_crtc_info(conn.get(), outputInfoReply->crtc, XCB_CURRENT_TIME), nullptr)
    );
    if (!crtcInfoReply)
      return Err(DracError(NotFound, "Failed to get CRTC info for primary display"));

    u16 refreshRate = 0;
    if (crtcInfoReply->mode != XCB_NONE) {
      const ReplyGuard<xcb_randr_get_screen_resources_current_reply_t> screenResourcesReply(
        xcb_randr_get_screen_resources_current_reply(
          conn.get(), xcb_randr_get_screen_resources_current(conn.get(), screen->root), nullptr
        )
      );

      if (screenResourcesReply) {
        xcb_randr_mode_info_t*         modeInfo  = nullptr;
        xcb_randr_mode_info_iterator_t modesIter = xcb_randr_get_screen_resources_current_modes_iterator(screenResourcesReply.get());
        for (; modesIter.rem; xcb_randr_mode_info_next(&modesIter)) {
          if (modesIter.data->id == crtcInfoReply->mode) {
            modeInfo = modesIter.data;
            break;
          }
        }
        if (modeInfo && modeInfo->htotal > 0 && modeInfo->vtotal > 0)
          refreshRate = static_cast<u16>(round(static_cast<f64>(modeInfo->dot_clock) / (static_cast<f64>(modeInfo->htotal) * static_cast<f64>(modeInfo->vtotal))));
      }
    }

    return Display(
      primaryOutput,
      Display::Resolution { .width = crtcInfoReply->width, .height = crtcInfoReply->height },
      refreshRate,
      true
    );
  }
  #else
  fn GetX11WindowManager() -> Result<String> {
    return Err(DracError(NotSupported, "XCB (X11) support not available"));
  }

  fn GetX11Displays() -> Result<Vec<Display>> {
    return Err(DracError(NotSupported, "XCB (X11) display support not available"));
  }
  #endif

  #ifdef HAVE_WAYLAND
  struct WaylandCallbackData {
    struct Inner {
      u32 id;
      u32 width;
      u32 height;
      u32 refreshRate;
    };

    Vec<Inner> outputs;
  };

  void wayland_output_mode(void* data, wl_output* /*output*/, u32 flags, i32 width, i32 height, i32 refresh) {
    if (!(flags & WL_OUTPUT_MODE_CURRENT))
      return;

    auto* callbackData = static_cast<WaylandCallbackData*>(data);

    if (!callbackData->outputs.empty()) {
      WaylandCallbackData::Inner& currentOutput = callbackData->outputs.back();

      currentOutput.width       = width > 0 ? width : 0;
      currentOutput.height      = height > 0 ? height : 0;
      currentOutput.refreshRate = refresh > 0 ? refresh : 0;
    }
  }

  void wayland_output_geometry(void* /*data*/, wl_output* /*output*/, i32 /*x*/, i32 /*y*/, i32 /*physical_width*/, i32 /*physical_height*/, i32 /*subpixel*/, const char* /*make*/, const char* /*model*/, i32 /*transform*/) {}
  void wayland_output_done(void* /*data*/, wl_output* /*output*/) {}
  void wayland_output_scale(void* /*data*/, wl_output* /*output*/, i32 /*factor*/) {}

  void wayland_registry_handler(void* data, wl_registry* registry, u32 objectId, const char* interface, u32 version) {
    if (std::strcmp(interface, "wl_output") != 0)
      return;

    auto* callbackData = static_cast<WaylandCallbackData*>(data);

    wl_output* output = static_cast<wl_output*>(wl_registry_bind(registry, objectId, &wl_output_interface, std::min(version, 2U)));

    if (!output)
      return;

    const static wl_output_listener OUTPUT_LISTENER = {
      .geometry    = wayland_output_geometry,
      .mode        = wayland_output_mode,
      .done        = wayland_output_done,
      .scale       = wayland_output_scale,
      .name        = nullptr,
      .description = nullptr
    };

    callbackData->outputs.push_back({ objectId, 0, 0, 0 });
    wl_output_add_listener(output, &OUTPUT_LISTENER, data);
  }

  void wayland_registry_removal(void* /*data*/, wl_registry* /*registry*/, u32 /*id*/) {}

  struct WaylandPrimaryDisplayData {
    wl_output* output = nullptr;
    Display    display;
    bool       done = false;
  };

  fn wayland_primary_mode(void* data, wl_output* /*output*/, u32 flags, i32 width, i32 height, i32 refresh) -> void {
    if (!(flags & WL_OUTPUT_MODE_CURRENT))
      return;

    auto* displayData = static_cast<WaylandPrimaryDisplayData*>(data);

    if (displayData->done)
      return;

    displayData->display.resolution  = { .width = static_cast<u16>(width), .height = static_cast<u16>(height) };
    displayData->display.refreshRate = refresh > 0 ? refresh / 1000 : 0;
  }

  void wayland_primary_done(void* data, wl_output* /*wl_output*/) {
    auto* displayData = static_cast<WaylandPrimaryDisplayData*>(data);
    if (displayData->display.resolution.width > 0)
      displayData->done = true;
  }

  fn wayland_primary_geometry(void* /*data*/, wl_output* /*output*/, i32 /*x*/, i32 /*y*/, i32 /*physical_width*/, i32 /*physical_height*/, i32 /*subpixel*/, const char* /*make*/, const char* /*model*/, i32 /*transform*/) -> void {}
  fn wayland_primary_scale(void* /*data*/, wl_output* /*output*/, i32 /*factor*/) -> void {}

  void wayland_primary_registry(void* data, wl_registry* registry, u32 name, const char* interface, u32 version) {
    auto* displayData = static_cast<WaylandPrimaryDisplayData*>(data);
    if (displayData->output != nullptr || strcmp(interface, "wl_output") != 0)
      return;

    displayData->display.id        = name;
    displayData->display.isPrimary = true;
    displayData->output            = static_cast<wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2U)));
    if (!displayData->output)
      return;

    const static wl_output_listener LISTENER = {
      .geometry    = wayland_primary_geometry,
      .mode        = wayland_primary_mode,
      .done        = wayland_primary_done,
      .scale       = wayland_primary_scale,
      .name        = nullptr,
      .description = nullptr
    };

    wl_output_add_listener(displayData->output, &LISTENER, data);
  }

  void wayland_primary_registry_remover(void* /*data*/, wl_registry* /*registry*/, u32 /*id*/) {}

  fn GetWaylandDisplays() -> Result<Vec<Display>> {
    const Wayland::DisplayGuard display;
    if (!display)
      return Err(DracError(ApiUnavailable, "Failed to connect to Wayland display"));

    wl_registry* registry = wl_display_get_registry(display.get());

    if (!registry)
      return Err(DracError(ApiUnavailable, "Failed to get Wayland registry"));

    WaylandCallbackData callbackData;

    const static wl_registry_listener REGISTRY_LISTENER = {
      .global        = wayland_registry_handler,
      .global_remove = wayland_registry_removal,
    };

    if (wl_registry_add_listener(registry, &REGISTRY_LISTENER, &callbackData) < 0)
      return Err(DracError(ApiUnavailable, "Failed to add registry listener"));

    wl_display_roundtrip(display.get());
    wl_display_roundtrip(display.get());

    Vec<Display> displays;

    for (const WaylandCallbackData::Inner& output : callbackData.outputs)
      if (output.width > 0 && output.height > 0)
        displays.emplace_back(
          output.id,
          Display::Resolution { .width = static_cast<u16>(output.width), .height = static_cast<u16>(output.height) },
          output.refreshRate / 1000,
          displays.empty()
        );

    if (displays.empty())
      return Err(DracError(NotFound, "No Wayland displays detected"));

    return displays;
  }

  fn GetWaylandPrimaryDisplay() -> Result<Display> {
    const Wayland::DisplayGuard display;
    if (!display)
      return Err(DracError(ApiUnavailable, "Failed to connect to Wayland display"));

    wl_registry* registry = wl_display_get_registry(display.get());
    if (!registry)
      return Err(DracError(ApiUnavailable, "Failed to get Wayland registry"));

    WaylandPrimaryDisplayData         data {};
    const static wl_registry_listener LISTENER = { .global = wayland_primary_registry, .global_remove = wayland_primary_registry_remover };

    wl_registry_add_listener(registry, &LISTENER, &data);

    wl_display_roundtrip(display.get());
    wl_display_roundtrip(display.get());

    if (data.output)
      wl_output_destroy(data.output);
    wl_registry_destroy(registry);

    if (data.done)
      return data.display;

    return Err(DracError(NotFound, "No primary Wayland display found"));
  }

  fn GetWaylandCompositor() -> Result<String> {
    const Wayland::DisplayGuard display;

    if (!display)
      return Err(DracError(NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred {};
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(DracError("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf {};

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DracError(InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf {}; // NOLINT(misc-include-cleaner) - PATH_MAX is in <climits>

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      return Err(DracError(std::format("Failed to read link '{}'", exeLinkPath)));

    exeRealPathBuf.at(bytesRead) = '\0';

    StringView compositorNameView;

    const StringView pathView(exeRealPathBuf.data(), bytesRead);

    StringView filenameView;

    if (const usize lastCharPos = pathView.find_last_not_of('/'); lastCharPos != StringView::npos) {
      const StringView relevantPart = pathView.substr(0, lastCharPos + 1);

      if (const usize separatorPos = relevantPart.find_last_of('/'); separatorPos == StringView::npos)
        filenameView = relevantPart;
      else
        filenameView = relevantPart.substr(separatorPos + 1);
    }

    if (!filenameView.empty())
      compositorNameView = filenameView;

    if (compositorNameView.empty() || compositorNameView == "." || compositorNameView == "/")
      return Err(DracError(NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(DracError(NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  }
  #else
  fn GetWaylandDisplays() -> Result<Vec<Display>> {
    return Err(DracError(NotSupported, "Wayland display support not available"));
  }

  fn GetWaylandPrimaryDisplay() -> Result<Display> {
    return Err(DracError(NotSupported, "Wayland display support not available"));
  }

  fn GetWaylandCompositor() -> Result<String> {
    return Err(DracError(NotSupported, "Wayland support not available"));
  }
  #endif
} // namespace

namespace draconis::core::system {
  using draconis::utils::env::GetEnv;

  fn GetOSVersion(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_os_version", []() -> Result<String> {
        std::ifstream file("/etc/os-release");

        if (!file)
            return Err(DracError(NotFound, std::format("Failed to open /etc/os-release")));

        String               line;
        constexpr StringView prefix = "PRETTY_NAME=";

        while (std::getline(file, line)) {
            if (StringView(line).starts_with(prefix)) {
                String value = line.substr(prefix.size());

                if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
                    (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
                    value = value.substr(1, value.length() - 2);

                if (value.empty())
                    return Err(DracError(ParseError, std::format("PRETTY_NAME value is empty or only quotes in /etc/os-release")));

                return String(value);
            }
        }

        return Err(DracError(NotFound, "PRETTY_NAME line not found in /etc/os-release"));
    });
  }

  fn GetMemInfo() -> Result<ResourceUsage> {
    struct sysinfo info;

    if (sysinfo(&info) != 0)
      return Err(DracError("sysinfo call failed"));

    if (info.mem_unit == 0)
      return Err(DracError(InternalError, "sysinfo.mem_unit is 0, cannot calculate memory"));

    return ResourceUsage {
      .usedBytes  = (info.totalram - info.freeram) * info.mem_unit,
      .totalBytes = info.totalram * info.mem_unit,
    };
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
  #ifdef HAVE_DBUS
    using namespace DBus;

    Result<Connection> connectionResult = Connection::busGet(DBUS_BUS_SESSION);
    if (!connectionResult)
      return Err(connectionResult.error());

    const Connection& connection = *connectionResult;

    Option<String> activePlayer = None;

    {
      Result<Message> listNamesResult =
        Message::newMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
      if (!listNamesResult)
        return Err(listNamesResult.error());

      Result<Message> listNamesReplyResult = connection.sendWithReplyAndBlock(*listNamesResult, 100);
      if (!listNamesReplyResult)
        return Err(listNamesReplyResult.error());

      MessageIter iter = listNamesReplyResult->iterInit();
      if (!iter.isValid() || iter.getArgType() != DBUS_TYPE_ARRAY)
        return Err(DracError(DracErrorCode::ParseError, "Invalid DBus ListNames reply format: Expected array"));

      MessageIter subIter = iter.recurse();
      if (!subIter.isValid())
        return Err(
          DracError(DracErrorCode::ParseError, "Invalid DBus ListNames reply format: Could not recurse into array")
        );

      while (subIter.getArgType() != DBUS_TYPE_INVALID) {
        if (Option<String> name = subIter.getString())
          if (name->starts_with("org.mpris.MediaPlayer2.")) {
            activePlayer = std::move(*name);
            break;
          }
        if (!subIter.next())
          break;
      }
    }

    if (!activePlayer)
      return Err(DracError(DracErrorCode::NotFound, "No active MPRIS players found"));

    Result<Message> msgResult = Message::newMethodCall(
      activePlayer->c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get"
    );

    if (!msgResult)
      return Err(msgResult.error());

    Message& msg = *msgResult;

    if (!msg.appendArgs("org.mpris.MediaPlayer2.Player", "Metadata"))
      return Err(DracError(DracErrorCode::InternalError, "Failed to append arguments to Properties.Get message"));

    Result<Message> replyResult = connection.sendWithReplyAndBlock(msg, 100);

    if (!replyResult)
      return Err(replyResult.error());

    Option<String> title  = None;
    Option<String> artist = None;

    MessageIter propIter = replyResult->iterInit();
    if (!propIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Properties.Get reply has no arguments or invalid iterator"));

    if (propIter.getArgType() != DBUS_TYPE_VARIANT)
      return Err(DracError(DracErrorCode::ParseError, "Properties.Get reply argument is not a variant"));

    MessageIter variantIter = propIter.recurse();
    if (!variantIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Could not recurse into variant"));

    if (variantIter.getArgType() != DBUS_TYPE_ARRAY || variantIter.getElementType() != DBUS_TYPE_DICT_ENTRY)
      return Err(DracError(DracErrorCode::ParseError, "Metadata variant content is not a dictionary array (a{sv})"));

    MessageIter dictIter = variantIter.recurse();
    if (!dictIter.isValid())
      return Err(DracError(DracErrorCode::ParseError, "Could not recurse into metadata dictionary array"));

    while (dictIter.getArgType() == DBUS_TYPE_DICT_ENTRY) {
      MessageIter entryIter = dictIter.recurse();
      if (!entryIter.isValid()) {
        if (!dictIter.next())
          break;
        continue;
      }

      Option<String> key = entryIter.getString();
      if (!key) {
        if (!dictIter.next())
          break;
        continue;
      }

      if (!entryIter.next() || entryIter.getArgType() != DBUS_TYPE_VARIANT) {
        if (!dictIter.next())
          break;
        continue;
      }

      MessageIter valueVariantIter = entryIter.recurse();
      if (!valueVariantIter.isValid()) {
        if (!dictIter.next())
          break;
        continue;
      }

      if (*key == "xesam:title") {
        title = valueVariantIter.getString();
      } else if (*key == "xesam:artist") {
        if (valueVariantIter.getArgType() == DBUS_TYPE_ARRAY && valueVariantIter.getElementType() == DBUS_TYPE_STRING) {
          if (MessageIter artistArrayIter = valueVariantIter.recurse(); artistArrayIter.isValid())
            artist = artistArrayIter.getString();
        }
      }

      if (!dictIter.next())
        break;
    }

    return MediaInfo(std::move(title), std::move(artist));
  #else
    return Err(DracError(NotSupported, "DBus support not available"));
  #endif

    return Err(DracError(NotFound, "No media player found or an unknown error occurred"));
  }

  fn GetWindowManager(draconis::utils::cache::CacheManager& cache) -> Result<String> {
  #if !defined(HAVE_WAYLAND) && !defined(HAVE_XCB)
    return Err(DracError(NotSupported, "Wayland or XCB support not available"));
  #endif

    return cache.getOrSet<String>("linux_wm", [&]() -> Result<String> {
        if (GetEnv("WAYLAND_DISPLAY"))
            return GetWaylandCompositor();

        if (GetEnv("DISPLAY"))
            return GetX11WindowManager();

        return Err(DracError(NotFound, "No display server detected"));
    });
  }

  fn GetDesktopEnvironment(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_desktop_environment", []() -> Result<String> {
        Result<PCStr> xdgEnvResult = GetEnv("XDG_CURRENT_DESKTOP");

        if (xdgEnvResult) {
            String xdgDesktopSz = String(*xdgEnvResult);

            if (const usize colonPos = xdgDesktopSz.find(':'); colonPos != String::npos)
                xdgDesktopSz.resize(colonPos);

            return xdgDesktopSz;
        }

        Result<PCStr> desktopSessionResult = GetEnv("DESKTOP_SESSION");

        if (desktopSessionResult)
            return *desktopSessionResult;

        return Err(desktopSessionResult.error());
    });
  }

  fn GetShell(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_shell", []() -> Result<String> {
        // clang-format off
        return GetEnv("SHELL")
            .transform([](String shellPath) -> String {
                constexpr Array<Pair<StringView, StringView>, 5> shellMap {{
                    { "/usr/bin/bash", "Bash" },
                    { "/usr/bin/zsh", "Zsh" },
                    { "/usr/bin/fish", "Fish" },
                    { "/usr/bin/nu", "Nushell" },
                    { "/usr/bin/sh", "SH" },
                }};

                for (const auto& [exe, name] : shellMap)
                    if (shellPath == exe)
                        return String(name);

                if (const usize lastSlash = shellPath.find_last_of('/'); lastSlash != String::npos)
                    return shellPath.substr(lastSlash + 1);
                return shellPath;
            })
            .transform([](const String& shellPath) -> String {
                return shellPath;
            });
        // clang-format on
    });
  }

  fn GetHost(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_host", []() -> Result<String> {
        constexpr PCStr primaryPath  = "/sys/class/dmi/id/product_family";
        constexpr PCStr fallbackPath = "/sys/class/dmi/id/product_name";

        fn readFirstLine = [&](const String& path) -> Result<String> {
            std::ifstream file(path);
            String        line;

            if (!file)
                return Err(DracError(NotFound, std::format("Failed to open DMI product identifier file '{}'", path)));

            if (!std::getline(file, line) || line.empty())
                return Err(DracError(ParseError, std::format("DMI product identifier file ('{}') is empty", path)));

            return line;
        };

        Result<String> primaryResult = readFirstLine(primaryPath);

        if (primaryResult)
            return primaryResult;

        DracError primaryError = primaryResult.error();

        Result<String> fallbackResult = readFirstLine(fallbackPath);

        if (fallbackResult)
            return fallbackResult;

        DracError fallbackError = fallbackResult.error();

        return Err(DracError(
            InternalError,
            std::format(
                "Failed to get host identifier. Primary ('{}'): {}. Fallback ('{}'): {}",
                primaryPath,
                primaryError.message,
                fallbackPath,
                fallbackError.message
            )
        ));
    });
  }

  fn GetCPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_cpu_model", []() -> Result<String> {
        Array<u32, 4>   cpuInfo;
        Array<char, 49> brandString = { 0 };

        __get_cpuid(0x80000000, cpuInfo.data(), &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
        const u32 maxFunction = cpuInfo[0];

        if (maxFunction < 0x80000004)
            return Err(DracError(NotSupported, "CPU does not support brand string"));

        for (u32 i = 0; i < 3; ++i) {
            __get_cpuid(0x80000002 + i, cpuInfo.data(), &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
            std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
        }

        std::string result(brandString.data());

        result.erase(result.find_last_not_of(" \t\n\r") + 1);

        if (result.empty())
            return Err(DracError(InternalError, "Failed to get CPU model string via CPUID"));

        return result;
    });
  }

  fn GetCPUCores(draconis::utils::cache::CacheManager& cache) -> Result<CPUCores> {
    return cache.getOrSet<CPUCores>("linux_cpu_cores", []() -> Result<CPUCores> {
        u32 eax = 0, ebx = 0, ecx = 0, edx = 0;

        __get_cpuid(0x0, &eax, &ebx, &ecx, &edx);
        const u32 maxLeaf   = eax;
        const u32 vendorEbx = ebx;

        u32 logicalCores  = 0;
        u32 physicalCores = 0;

        if (maxLeaf >= 0xB) {
            u32 threadsPerCore = 0;
            for (u32 subleaf = 0;; ++subleaf) {
                __get_cpuid_count(0xB, subleaf, &eax, &ebx, &ecx, &edx);
                if (ebx == 0)
                    break;

                const u32 levelType         = (ecx >> 8) & 0xFF;
                const u32 processorsAtLevel = ebx & 0xFFFF;

                if (levelType == 1) // SMT (Hyper-Threading) level
                    threadsPerCore = processorsAtLevel;

                if (levelType == 2) // Core level
                    logicalCores = processorsAtLevel;
            }

            if (logicalCores > 0 && threadsPerCore > 0)
                physicalCores = logicalCores / threadsPerCore;
        }

        if (physicalCores == 0 || logicalCores == 0) {
            __get_cpuid(0x1, &eax, &ebx, &ecx, &edx);
            logicalCores                 = (ebx >> 16) & 0xFF;
            const bool hasHyperthreading = (edx & (1 << 28)) != 0;

            if (hasHyperthreading) {
                constexpr u32 vendorIntel = 0x756e6547; // "Genu"ine"Intel"
                constexpr u32 vendorAmd   = 0x68747541; // "Auth"entic"AMD"

                if (vendorEbx == vendorIntel && maxLeaf >= 0x4) {
                    __get_cpuid_count(0x4, 0, &eax, &ebx, &ecx, &edx);
                    physicalCores = ((eax >> 26) & 0x3F) + 1;
                } else if (vendorEbx == vendorAmd) {
                    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx); // Get max extended leaf
                    if (eax >= 0x80000008) {
                        __get_cpuid(0x80000008, &eax, &ebx, &ecx, &edx);
                        physicalCores = (ecx & 0xFF) + 1;
                    }
                }
            } else {
                physicalCores = logicalCores;
            }
        }

        if (physicalCores == 0 && logicalCores > 0)
            physicalCores = logicalCores;

        if (physicalCores == 0 || logicalCores == 0)
            return Err(DracError(InternalError, "Failed to determine core counts via CPUID"));

        return CPUCores(static_cast<u16>(physicalCores), static_cast<u16>(logicalCores));
    });
  }

  fn GetGPUModel(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_gpu_model", []() -> Result<String> {
        const fs::path pciPath = "/sys/bus/pci/devices";

        if (!fs::exists(pciPath))
            return Err(DracError(NotFound, "PCI device path '/sys/bus/pci/devices' not found."));

        // clang-format off
        const Array<Pair<StringView, StringView>, 3> fallbackVendorMap = {{
            { "0x1002",    "AMD" },
            { "0x10de", "NVIDIA" },
            { "0x8086",  "Intel" }
        }};
        // clang-format on

        for (const fs::directory_entry& entry : fs::directory_iterator(pciPath)) {
            if (Result<String> classIdRes = ReadSysFile(entry.path() / "class"); !classIdRes || !classIdRes->starts_with("0x03"))
                continue;

            Result<String> vendorIdRes = ReadSysFile(entry.path() / "vendor");
            Result<String> deviceIdRes = ReadSysFile(entry.path() / "device");

            if (vendorIdRes && deviceIdRes) {
                auto [vendor, device] = LookupPciNames(*vendorIdRes, *deviceIdRes);

                if (!vendor.empty() && !device.empty())
                    return CleanGpuModelName(std::move(vendor), std::move(device));
            }

            if (vendorIdRes) {
                const auto* iter = std::ranges::find_if(fallbackVendorMap, [&](const auto& pair) {
                    return pair.first == *vendorIdRes;
                });

                if (iter != fallbackVendorMap.end())
                    return String(iter->second);
            }
        }

        return Err(DracError(NotFound, "No compatible GPU found in /sys/bus/pci/devices."));
    }, draconis::utils::cache::CachePolicy::NeverExpire());
  }

  fn GetUptime() -> Result<std::chrono::seconds> {
    struct sysinfo info;

    if (sysinfo(&info) != 0)
      return Err(DracError(InternalError, "sysinfo call failed"));

    return std::chrono::seconds(info.uptime);
  }

  fn GetKernelVersion(draconis::utils::cache::CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("linux_kernel_version", []() -> Result<String> {
        utsname uts;

        if (uname(&uts) == -1)
            return Err(DracError(InternalError, "uname call failed"));

        if (std::strlen(uts.release) == 0)
            return Err(DracError(ParseError, "uname returned null kernel release"));

        return String(uts.release);
    });
  }

  fn GetDiskUsage() -> Result<ResourceUsage> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DracError(InternalError, "Failed to get filesystem stats for '/' (statvfs call failed)"));

    return ResourceUsage {
      .usedBytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .totalBytes = stat.f_blocks * stat.f_frsize,
    };
  }

  fn GetDisplays() -> Result<Vec<Display>> {
    if (GetEnv("WAYLAND_DISPLAY")) {
      Result<Vec<Display>> displays = GetWaylandDisplays();

      if (displays)
        return displays;

      debug_at(displays.error());
    }

    if (GetEnv("DISPLAY")) {
      Result<Vec<Display>> displays = GetX11Displays();

      if (displays)
        return displays;

      debug_at(displays.error());
    }

    return Err(DracError(NotFound, "No display server detected"));
  }

  fn GetPrimaryDisplay() -> Result<Display> {
    if (GetEnv("WAYLAND_DISPLAY")) {
      Result<Display> display = GetWaylandPrimaryDisplay();

      if (display)
        return display;

      debug_at(display.error());
    }

    if (GetEnv("DISPLAY")) {
      Result<Display> display = GetX11PrimaryDisplay();

      if (display)
        return display;

      debug_at(display.error());
    }

    return Err(DracError(NotFound, "No display server detected"));
  }

  fn GetNetworkInterfaces() -> Result<Vec<NetworkInterface>> {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
    ifaddrs* ifaddrList = nullptr;
    if (getifaddrs(&ifaddrList) == -1)
      return Err(DracError(InternalError, "getifaddrs failed"));

    UniquePointer<ifaddrs, decltype(&freeifaddrs)> ifaddrsDeleter(ifaddrList, &freeifaddrs);

    // Use a map to collect interface information since getifaddrs returns multiple entries per interface
    std::map<String, NetworkInterface> interfaceMap;

    for (ifaddrs* ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == nullptr)
        continue;

      const String interfaceName = String(ifa->ifa_name);

      // Get or create the interface entry
      auto& interface = interfaceMap[interfaceName];
      interface.name  = interfaceName;

      // Set flags
      interface.isUp       = ifa->ifa_flags & IFF_UP;
      interface.isLoopback = ifa->ifa_flags & IFF_LOOPBACK;

      // Get IPv4 details
      if (ifa->ifa_addr->sa_family == AF_INET) {
        Array<char, NI_MAXHOST> host = {};
        if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
          interface.ipv4Address = String(host.data());
      }
      // Get IPv6 details
      else if (ifa->ifa_addr->sa_family == AF_INET6) {
        Array<char, NI_MAXHOST> host = {};
        if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in6), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
          interface.ipv6Address = String(host.data());
      }
      // Get MAC address (AF_PACKET on Linux)
      else if (ifa->ifa_addr->sa_family == AF_PACKET) {
        auto* sll = reinterpret_cast<sockaddr_ll*>(ifa->ifa_addr);

        if (sll && sll->sll_halen == 6) {
          interface.macAddress = std::format(
            "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            sll->sll_addr[0],
            sll->sll_addr[1],
            sll->sll_addr[2],
            sll->sll_addr[3],
            sll->sll_addr[4],
            sll->sll_addr[5]
          );
        }
      }
    }

    // Convert the map to a vector
    Vec<NetworkInterface> interfaces;
    interfaces.reserve(interfaceMap.size());

    for (const auto& pair : interfaceMap)
      interfaces.push_back(pair.second);

    if (interfaces.empty())
      return Err(DracError(NotFound, "No network interfaces found"));

    return interfaces;
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
  }

  fn GetPrimaryNetworkInterface(draconis::utils::cache::CacheManager& cache) -> Result<NetworkInterface> {
    return cache.getOrSet<NetworkInterface>("linux_primary_network_interface", []() -> Result<NetworkInterface> {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) - This requires a lot of casts and there's no good way to avoid them.
        // First, try to find the default route to determine the primary interface
        String primaryInterfaceName;

        // Read the routing table from /proc/net/route
        std::ifstream routeFile("/proc/net/route");
        if (routeFile.is_open()) {
            String line;
            // Skip header line
            std::getline(routeFile, line);

            while (std::getline(routeFile, line)) {
                std::istringstream iss(line);
                String             iface, dest, gateway, flags, refcnt, use, metric, mask, mtu, window, irtt;

                if (iss >> iface >> dest >> gateway >> flags >> refcnt >> use >> metric >> mask >> mtu >> window >> irtt) {
                    // Check if this is the default route (destination is 00000000)
                    if (dest == "00000000") {
                        primaryInterfaceName = iface;
                        break;
                    }
                }
            }
        }

        // If we couldn't find the primary interface from routing table, try to find the first non-loopback interface
        if (primaryInterfaceName.empty()) {
            ifaddrs* ifaddrList = nullptr;
            if (getifaddrs(&ifaddrList) == -1)
                return Err(DracError(InternalError, "getifaddrs failed"));

            UniquePointer<ifaddrs, decltype(&freeifaddrs)> ifaddrsDeleter(ifaddrList, &freeifaddrs);

            for (ifaddrs* ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == nullptr)
                    continue;

                const String interfaceName = String(ifa->ifa_name);
                const bool   isUp          = ifa->ifa_flags & IFF_UP;
                const bool   isLoopback    = ifa->ifa_flags & IFF_LOOPBACK;

                // Find the first non-loopback interface that is up
                if (isUp && !isLoopback) {
                    primaryInterfaceName = interfaceName;
                    break;
                }
            }
        }

        if (primaryInterfaceName.empty())
            return Err(DracError(NotFound, "Could not determine primary interface name"));

        // Now get the detailed information for the primary interface
        ifaddrs* ifaddrList = nullptr;
        if (getifaddrs(&ifaddrList) == -1)
            return Err(DracError(InternalError, "getifaddrs failed"));

        UniquePointer<ifaddrs, decltype(&freeifaddrs)> ifaddrsDeleter(ifaddrList, &freeifaddrs);

        NetworkInterface primaryInterface;
        primaryInterface.name = primaryInterfaceName;
        bool foundDetails     = false;

        for (ifaddrs* ifa = ifaddrList; ifa != nullptr; ifa = ifa->ifa_next) {
            // Skip any entries that don't match our primary interface name
            if (ifa->ifa_addr == nullptr || primaryInterfaceName != ifa->ifa_name)
                continue;

            foundDetails = true;

            // Set flags
            primaryInterface.isUp       = ifa->ifa_flags & IFF_UP;
            primaryInterface.isLoopback = ifa->ifa_flags & IFF_LOOPBACK;

            // Get IPv4 details
            if (ifa->ifa_addr->sa_family == AF_INET) {
                Array<char, NI_MAXHOST> host = {};
                if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
                    primaryInterface.ipv4Address = String(host.data());
            }
            // Get IPv6 details
            else if (ifa->ifa_addr->sa_family == AF_INET6) {
                Array<char, NI_MAXHOST> host = {};
                if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in6), host.data(), host.size(), nullptr, 0, NI_NUMERICHOST) == 0)
                    primaryInterface.ipv6Address = String(host.data());
            }
            // Get MAC address (AF_PACKET on Linux)
            else if (ifa->ifa_addr->sa_family == AF_PACKET) {
                auto* sll = reinterpret_cast<sockaddr_ll*>(ifa->ifa_addr);

                if (sll && sll->sll_halen == 6) {
                    primaryInterface.macAddress = std::format(
                        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                        sll->sll_addr[0],
                        sll->sll_addr[1],
                        sll->sll_addr[2],
                        sll->sll_addr[3],
                        sll->sll_addr[4],
                        sll->sll_addr[5]
                    );
                }
            }
        }

        if (!foundDetails)
            return Err(DracError(NotFound, "Found primary interface name, but could not find its details via getifaddrs"));

        return primaryInterface;
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    });
  }

  fn GetBatteryInfo() -> Result<Battery> {
    using matchit::match, matchit::is, matchit::_;
    using enum Battery::Status;

    PCStr powerSupplyPath = "/sys/class/power_supply";

    if (!fs::exists(powerSupplyPath))
      return Err(DracError(NotFound, "Power supply directory not found"));

    // Find the first battery device
    fs::path batteryPath;
    for (const fs::directory_entry& entry : fs::directory_iterator(powerSupplyPath))
      if (Result<String> typeResult = ReadSysFile(entry.path() / "type");
          typeResult && *typeResult == "Battery") {
        batteryPath = entry.path();
        break;
      }

    if (batteryPath.empty())
      return Err(DracError(NotFound, "No battery found in power supply directory"));

    // Read battery percentage
    Option<u8> percentage =
      ReadSysFile(batteryPath / "capacity")
        .transform([](const String& capacityStr) -> Option<u8> {
          return try_parse<u8>(capacityStr);
        })
        .value_or(None);

    // Read battery status
    Battery::Status status =
      ReadSysFile(batteryPath / "status")
        .transform([percentage](const String& statusStr) -> Battery::Status {
          return match(statusStr)(
            is | "Charging"     = Charging,
            is | "Discharging"  = Discharging,
            is | "Full"         = Full,
            is | "Not charging" = (percentage && *percentage == 100 ? Full : Discharging),
            is | _              = Unknown
          );
        })
        .value_or(Unknown);

    if (status != Charging && status != Discharging)
      return Battery(status, percentage, None);

    return Battery(
      status,
      percentage,
      ReadSysFile(
        batteryPath / std::format("/time_to_{}now", status == Discharging ? "empty" : "full")
      )
        .transform([](const String& timeStr) -> Option<std::chrono::seconds> {
          if (Option<i32> timeMinutes = try_parse<i32>(timeStr); timeMinutes && *timeMinutes > 0)
            return std::chrono::minutes(*timeMinutes);

          return None;
        })
        .value_or(None)
    );
  }
} // namespace draconis::core::system

  #ifdef DRAC_ENABLE_PACKAGECOUNT
namespace draconis::services::packages {
  fn CountApk() -> Result<u64> {
    

    const String   pmID      = "apk";
    const fs::path apkDbPath = "/lib/apk/db/installed";
    const String   cacheKey  = "pkg_count_" + pmID;

    if (Result<u64> cachedCountResult = GetValidCache<u64>(cacheKey))
      return *cachedCountResult;
    else
      debug_at(cachedCountResult.error());

    if (std::error_code fsErrCode; !fs::exists(apkDbPath, fsErrCode)) {
      if (fsErrCode) {
        warn_log("Filesystem error checking for Apk DB at '{}': {}", apkDbPath.string(), fsErrCode.message());
        return Err(DracError(IoError, "Filesystem error checking Apk DB: " + fsErrCode.message()));
      }

      return Err(DracError(NotFound, std::format("Apk database path '{}' does not exist", apkDbPath.string())));
    }

    std::ifstream file(apkDbPath);
    if (!file.is_open())
      return Err(DracError(IoError, std::format("Failed to open Apk database file '{}'", apkDbPath.string())));

    u64 count = 0;

    try {
      String line;

      while (std::getline(file, line))
        if (line.empty())
          count++;
    } catch (const std::ios_base::failure& e) {
      return Err(DracError(
        IoError,
        std::format("Error reading Apk database file '{}': {}", apkDbPath.string(), e.what())
      ));
    }

    if (file.bad())
      return Err(DracError(IoError, std::format("IO error while reading Apk database file '{}'", apkDbPath.string())));

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }

  fn CountDpkg() -> Result<u64> {
    return GetCountFromDirectory("dpkg", fs::current_path().root_path() / "var" / "lib" / "dpkg" / "info", String(".list"));
  }

  fn CountMoss() -> Result<u64> {
    Result<u64> countResult = GetCountFromDb("moss", "/.moss/db/install", "SELECT COUNT(*) FROM meta");

    if (countResult)
      if (*countResult > 0)
        return *countResult - 1;

    return countResult;
  }

  fn CountPacman() -> Result<u64> {
    return GetCountFromDirectory("pacman", fs::current_path().root_path() / "var" / "lib" / "pacman" / "local", true);
  }

  fn CountRpm() -> Result<u64> {
    return GetCountFromDb("rpm", "/var/lib/rpm/rpmdb.sqlite", "SELECT COUNT(*) FROM Installtid");
  }

    #ifdef HAVE_PUGIXML
  fn CountXbps() -> Result<u64> {
    const CStr xbpsDbPath = "/var/db/xbps";

    if (!fs::exists(xbpsDbPath))
      return Err(DracError(NotFound, std::format("Xbps database path '{}' does not exist", xbpsDbPath)));

    fs::path plistPath;

    for (const fs::directory_entry& entry : fs::directory_iterator(xbpsDbPath))
      if (const String filename = entry.path().filename().string(); filename.starts_with("pkgdb-") && filename.ends_with(".plist")) {
        plistPath = entry.path();
        break;
      }

    if (plistPath.empty())
      return Err(DracError(NotFound, "No Xbps database found"));

    return GetCountFromPlist("xbps", plistPath);
  }
    #endif
} // namespace draconis::services::packages
  #endif

#endif
