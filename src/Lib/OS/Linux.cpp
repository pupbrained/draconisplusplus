#ifdef __linux__

  #include <algorithm>
  #include <climits>              // PATH_MAX
  #include <cpuid.h>              // __get_cpuid
  #include <cstring>              // std::strlen
  #include <expected>             // std::{unexpected, expected}
  #include <filesystem>           // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
  #include <format>               // std::{format, format_to_n}
  #include <fstream>              // std::ifstream
  #include <glaze/beve/read.hpp>  // glz::read_beve
  #include <glaze/beve/write.hpp> // glz::write_beve
  #include <matchit.hpp>          // matchit::{is, is_not, is_any, etc.}
  #include <string>               // std::{getline, string (String)}
  #include <string_view>          // std::string_view (StringView)
  #include <sys/socket.h>         // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
  #include <sys/statvfs.h>        // statvfs
  #include <sys/sysinfo.h>        // sysinfo
  #include <sys/utsname.h>        // utsname, uname
  #include <unistd.h>             // readlink
  #include <utility>              // std::move

  #include <Drac++/Core/System.hpp>
  #include <Drac++/Services/PackageCounting.hpp>

  #include <DracUtils/Definitions.hpp>
  #include <DracUtils/Env.hpp>
  #include <DracUtils/Error.hpp>
  #include <DracUtils/Types.hpp>

  #include "Utils/Caching.hpp"
  #include "Wrappers/DBus.hpp"
  #include "Wrappers/Wayland.hpp"
  #include "Wrappers/XCB.hpp"

using drac::error::DracError;
using enum drac::error::DracErrorCode;
using namespace drac::types;
namespace fs = std::filesystem;

// clang-format off
#ifdef __GLIBC__
extern "C" fn issetugid() -> usize { return 0; } // NOLINT(readability-identifier-naming) - glibc function stub
#endif
// clang-format on

namespace {
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
      conn.get(), GetProperty(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || GetPropertyValueLength(wmNameReply.get()) == 0)
      return Err(DracError(NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(GetPropertyValue(wmNameReply.get()));
    const usize length   = GetPropertyValueLength(wmNameReply.get());

    return String(nameData, length);
  }
  #else
  fn GetX11WindowManager() -> Result<String> {
    return Err(DracError(NotSupported, "XCB (X11) support not available"));
  }
  #endif

  #ifdef HAVE_WAYLAND
  fn GetWaylandCompositor() -> Result<String> {
    const Wayland::DisplayGuard display;

    if (!display)
      return Err(DracError(NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
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
  fn GetWaylandCompositor() -> Result<String> {
    return Err(DracError(NotSupported, "Wayland support not available"));
  }
  #endif
} // namespace

namespace os {
  using drac::env::GetEnv;

  fn System::getOSVersion() -> Result<String> {
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
  }

  fn System::getMemInfo() -> Result<ResourceUsage> {
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

  fn System::getNowPlaying() -> Result<MediaInfo> {
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

  fn System::getWindowManager() -> Result<String> {
  #if !defined(HAVE_WAYLAND) && !defined(HAVE_XCB)
    return Err(DracError(NotSupported, "Wayland or XCB support not available"));
  #endif

    if (GetEnv("WAYLAND_DISPLAY"))
      return GetWaylandCompositor();

    if (GetEnv("DISPLAY"))
      return GetX11WindowManager();

    return Err(DracError(NotFound, "No display server detected"));
  }

  fn System::getDesktopEnvironment() -> Result<String> {
    Result<CStr> xdgEnvResult = GetEnv("XDG_CURRENT_DESKTOP");

    if (xdgEnvResult) {
      String xdgDesktopSz = String(*xdgEnvResult);

      if (const usize colonPos = xdgDesktopSz.find(':'); colonPos != String::npos)
        xdgDesktopSz.resize(colonPos);

      return xdgDesktopSz;
    }

    Result<CStr> desktopSessionResult = GetEnv("DESKTOP_SESSION");

    if (desktopSessionResult)
      return *desktopSessionResult;

    return Err(desktopSessionResult.error());
  }

  fn System::getShell() -> Result<String> {
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
  }

  fn System::getHost() -> Result<String> {
    constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
    constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

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
  }

  fn System::getCPUModel() -> Result<String> {
    std::array<unsigned int, 4> cpuInfo;
    std::array<char, 49>        brandString = { 0 };

    __get_cpuid(0x80000000, cpuInfo.data(), &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
    const unsigned int maxFunction = cpuInfo[0];

    if (maxFunction < 0x80000004)
      return Err(DracError(NotSupported, "CPU does not support brand string"));

    for (unsigned int i = 0; i < 3; ++i) {
      __get_cpuid(0x80000002 + i, cpuInfo.data(), &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
      std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
    }

    std::string result(brandString.data());

    result.erase(result.find_last_not_of(" \t\n\r") + 1);

    if (result.empty())
      return Err(DracError(InternalError, "Failed to get CPU model string via CPUID"));

    return result;
  }

  fn System::getGPUModel() -> Result<String> {
    using drac::cache::GetValidCache, drac::cache::WriteCache;

    const String cacheKey = "gpu_model";

    if (Result<String> cachedDataResult = GetValidCache<String>(cacheKey))
      return *cachedDataResult;

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

        if (iter != fallbackVendorMap.end()) {
          if (Result writeResult = WriteCache(cacheKey, iter->second); !writeResult)
            debug_at(writeResult.error());

          return String(iter->second);
        }
      }
    }

    return Err(DracError(NotFound, "No compatible GPU found in /sys/bus/pci/devices."));
  }

  fn System::getKernelVersion() -> Result<String> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError(InternalError, "uname call failed"));

    if (std::strlen(uts.release) == 0)
      return Err(DracError(ParseError, "uname returned null kernel release"));

    return String(uts.release);
  }

  fn System::getDiskUsage() -> Result<ResourceUsage> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DracError(InternalError, "Failed to get filesystem stats for '/' (statvfs call failed)"));

    return ResourceUsage {
      .usedBytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .totalBytes = stat.f_blocks * stat.f_frsize,
    };
  }
} // namespace os

  #ifdef DRAC_ENABLE_PACKAGECOUNT
namespace package {
  fn CountApk() -> Result<u64> {
    using namespace drac::cache;

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
} // namespace package
  #endif

#endif
