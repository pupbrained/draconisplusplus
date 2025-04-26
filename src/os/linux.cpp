#ifdef __linux__

// clang-format off
#include <dbus-cxx.h> // needs to be at top for Success/None
// clang-format on
#include <cstring>
#include <fstream>
#include <ranges>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <system_error>
#include <unistd.h>
#include <wayland-client.h>
#include <xcb/xcb.h>

#include "os.h"
#include "src/os/linux/display_guards.h"
#include "src/util/macros.h"

using namespace std::string_view_literals;

namespace {
  fn GetX11WindowManager() -> Option<String> {
    using os::linux::XcbReplyGuard;
    using os::linux::XorgDisplayGuard;

    const XorgDisplayGuard conn;

    if (!conn)
      RETURN_ERR("Failed to open X11 display");

    fn internAtom = [&conn](const StringView name) -> XcbReplyGuard<xcb_intern_atom_reply_t> {
      const auto cookie = xcb_intern_atom(conn.get(), 0, static_cast<uint16_t>(name.size()), name.data());
      return XcbReplyGuard(xcb_intern_atom_reply(conn.get(), cookie, nullptr));
    };

    const XcbReplyGuard<xcb_intern_atom_reply_t> supportingWmCheck = internAtom("_NET_SUPPORTING_WM_CHECK");
    const XcbReplyGuard<xcb_intern_atom_reply_t> wmName            = internAtom("_NET_WM_NAME");
    const XcbReplyGuard<xcb_intern_atom_reply_t> utf8String        = internAtom("UTF8_STRING");

    if (!supportingWmCheck || !wmName || !utf8String)
      RETURN_ERR("Failed to get X11 atoms");

    const XcbReplyGuard<xcb_get_property_reply_t> wmWindowReply(xcb_get_property_reply(
      conn.get(),
      xcb_get_property(conn.get(), 0, conn.rootScreen()->root, supportingWmCheck->atom, XCB_ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != XCB_ATOM_WINDOW || wmWindowReply->format != 32 ||
        xcb_get_property_value_length(wmWindowReply.get()) == 0)
      RETURN_ERR("Failed to get _NET_SUPPORTING_WM_CHECK property");

    const XcbReplyGuard<xcb_get_property_reply_t> wmNameReply(xcb_get_property_reply(
      conn.get(),
      xcb_get_property(
        conn.get(),
        0,
        *static_cast<xcb_window_t*>(xcb_get_property_value(wmWindowReply.get())),
        wmName->atom,
        utf8String->atom,
        0,
        1024
      ),
      nullptr
    ));

    if (!wmNameReply || wmNameReply->type != utf8String->atom || xcb_get_property_value_length(wmNameReply.get()) == 0)
      RETURN_ERR("Failed to get _NET_WM_NAME property");

    const char* nameData = static_cast<const char*>(xcb_get_property_value(wmNameReply.get()));
    const usize length   = xcb_get_property_value_length(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetWaylandCompositor() -> Option<String> {
    using os::linux::WaylandDisplayGuard;

    const WaylandDisplayGuard display;

    if (!display)
      RETURN_ERR("Failed to open Wayland display");

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      RETURN_ERR("Failed to get Wayland file descriptor");

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      RETURN_ERR("Failed to get socket credentials: {}", std::error_code(errno, std::generic_category()).message());

    Array<char, 128> exeLinkPathBuf;

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      RETURN_ERR("Failed to format /proc path (PID too large?)");

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf;

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      RETURN_ERR("Failed to read link {}: {}", exeLinkPath, std::error_code(errno, std::generic_category()).message());

    exeRealPathBuf.at(bytesRead) = '\0';

    String compositorName;

    try {
      namespace fs = std::filesystem;

      const fs::path exePath(exeRealPathBuf.data());

      compositorName = exePath.filename().string();
    } catch (const std::filesystem::filesystem_error& e) {
      RETURN_ERR("Error getting compositor name from path '{}': {}", exeRealPathBuf.data(), e.what());
    } catch (...) { RETURN_ERR("Unknown error getting compositor name"); }

    if (compositorName.empty() || compositorName == "." || compositorName == "/")
      RETURN_ERR("Empty or invalid compositor name {}", compositorName);

    const StringView compositorNameView = compositorName;

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        RETURN_ERR("Compositor name invalid after heuristic: original='%s'\n", compositorName.c_str());

      return String(cleanedView);
    }

    return compositorName;
  }

  fn GetMprisPlayers(const SharedPointer<DBus::Connection>& connection) -> Result<Vec<String>, NowPlayingError> {
    try {
      const SharedPointer<DBus::CallMessage> call =
        DBus::CallMessage::create("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

      const SharedPointer<DBus::Message> reply = connection->send_with_reply_blocking(call, 500);

      if (!reply) {
        ERROR_LOG("DBus timeout or null reply in ListNames");
        return Err("DBus timeout in ListNames");
      }

      Vec<String>           allNamesStd;
      DBus::MessageIterator reader(*reply);
      reader >> allNamesStd;

      Vec<String> mprisPlayers;
      for (const String& name : allNamesStd)
        if (StringView(name).contains("org.mpris.MediaPlayer2"sv))
          mprisPlayers.emplace_back(name);

      return mprisPlayers;
    } catch (const DBus::Error& e) {
      ERROR_LOG("DBus::Error exception in ListNames: {}", e.what());
      return Err(e.what());
    } catch (const Exception& e) {
      ERROR_LOG("Standard exception getting MPRIS players: {}", e.what());
      return Err(e.what());
    }
  }
}

fn os::GetOSVersion() -> Result<String, String> {
  constexpr CStr path = "/etc/os-release";

  std::ifstream file(path);

  if (!file)
    return Err(std::format("Failed to open {}", path));

  String       line;
  const String prefix = "PRETTY_NAME=";

  while (getline(file, line))
    if (line.starts_with(prefix)) {
      StringView valueView = StringView(line).substr(prefix.size());

      if (!valueView.empty() && valueView.front() == '"' && valueView.back() == '"') {
        valueView.remove_prefix(1);
        valueView.remove_suffix(1);
      }

      return String(valueView);
    }

  return Err(std::format("PRETTY_NAME line not found in {}", path));
}

fn os::GetMemInfo() -> Result<u64, String> {
  struct sysinfo info;

  if (sysinfo(&info) != 0)
    return Err(std::format("sysinfo failed: {}", std::error_code(errno, std::generic_category()).message()));

  return info.totalram * info.mem_unit;
}

fn os::GetNowPlaying() -> Result<String, NowPlayingError> {
  try {
    const SharedPointer<DBus::Dispatcher> dispatcher = DBus::StandaloneDispatcher::create();
    if (!dispatcher)
      return Err("Failed to create DBus dispatcher");

    const SharedPointer<DBus::Connection> connection = dispatcher->create_connection(DBus::BusType::SESSION);

    if (!connection)
      return Err("Failed to connect to session bus");

    Result<Vec<String>, NowPlayingError> mprisPlayersResult = GetMprisPlayers(connection);
    if (!mprisPlayersResult)
      return Err(mprisPlayersResult.error());

    const Vec<String>& mprisPlayers = *mprisPlayersResult;

    if (mprisPlayers.empty())
      return Err(NowPlayingCode::NoPlayers);

    const String activePlayer = mprisPlayers.front();

    const SharedPointer<DBus::CallMessage> metadataCall =
      DBus::CallMessage::create(activePlayer, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");

    *metadataCall << "org.mpris.MediaPlayer2.Player" << "Metadata";

    const SharedPointer<DBus::Message> metadataReply = connection->send_with_reply_blocking(metadataCall, 5000);

    String title;
    String artist;

    if (metadataReply && metadataReply->is_valid()) {
      try {
        DBus::MessageIterator iter(*metadataReply);
        DBus::Variant         metadataVariant;
        iter >> metadataVariant;

        if (metadataVariant.type() == DBus::DataType::ARRAY) {
          Map<String, DBus::Variant> metadata = metadataVariant.to_map<String, DBus::Variant>();

          if (auto titleIter = metadata.find("xesam:title");
              titleIter != metadata.end() && titleIter->second.type() == DBus::DataType::STRING)
            title = titleIter->second.to_string();

          if (auto artistIter = metadata.find("xesam:artist"); artistIter != metadata.end()) {
            if (artistIter->second.type() == DBus::DataType::ARRAY) {
              if (Vec<String> artists = artistIter->second.to_vector<String>(); !artists.empty())
                artist = artists[0];
            } else if (artistIter->second.type() == DBus::DataType::STRING)
              artist = artistIter->second.to_string();
          }
        } else {
          ERROR_LOG(
            "Metadata variant is not the expected type, expected a{{sv}} but got {}", metadataVariant.signature().str()
          );
        }
      } catch (const DBus::Error& e) {
        ERROR_LOG("DBus error processing metadata reply: {}", e.what());
      } catch (const Exception& e) { ERROR_LOG("Error processing metadata reply: {}", e.what()); }
    }

    return std::format("{}{}{}", artist, !artist.empty() && !title.empty() ? " - " : "", title);
  } catch (const DBus::Error& e) { return Err(std::format("DBus error: {}", e.what())); } catch (const Exception& e) {
    return Err(std::format("General error: {}", e.what()));
  }
}

fn os::GetWindowManager() -> Option<String> {
  // clang-format off
  return GetWaylandCompositor()
    .or_else([] { return GetX11WindowManager(); })
    .and_then([](const String& windowManager) -> Option<String> {
      DEBUG_LOG("Found window manager: {}", windowManager);
      return windowManager;
    });
  // clang-format on
}

fn os::GetDesktopEnvironment() -> Option<String> {
  return GetEnv("XDG_CURRENT_DESKTOP")
    .transform([](const String& xdgDesktop) -> String {
      if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
        return xdgDesktop.substr(0, colon);

      return xdgDesktop;
    })
    .or_else([](const EnvError&) -> Result<String, EnvError> { return GetEnv("DESKTOP_SESSION"); })
    .transform([](const String& finalValue) -> Option<String> {
      DEBUG_LOG("Found desktop environment: {}", finalValue);
      return finalValue;
    })
    .value_or(None);
}

fn os::GetShell() -> String {
  if (const Result<String, EnvError> shellPath = GetEnv("SHELL")) {
    // clang-format off
    constexpr Array<Pair<StringView, StringView>, 5> shellMap {{
      { "bash",    "Bash" },
      {  "zsh",     "Zsh" },
      { "fish",    "Fish" },
      {   "nu", "Nushell" },
      {   "sh",      "SH" }, // sh last because other shells contain "sh"
    }};
    // clang-format on

    for (const auto& [exe, name] : shellMap)
      if (shellPath->contains(exe))
        return String(name);

    return *shellPath; // fallback to the raw shell path
  }

  return "";
}

fn os::GetHost() -> String {
  constexpr CStr path = "/sys/class/dmi/id/product_family";

  std::ifstream file(path);

  if (!file) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  String productFamily;

  if (!getline(file, productFamily)) {
    ERROR_LOG("Failed to read from {} (is it empty?)", path);
    return "";
  }

  return productFamily;
}

fn os::GetKernelVersion() -> String {
  utsname uts;

  if (uname(&uts) == -1) {
    ERROR_LOG("uname() failed: {}", std::error_code(errno, std::generic_category()).message());
    return "";
  }

  return uts.release;
}

fn os::GetDiskUsage() -> Pair<u64, u64> {
  struct statvfs stat;

  if (statvfs("/", &stat) == -1) {
    ERROR_LOG("statvfs() failed: {}", std::error_code(errno, std::generic_category()).message());
    return { 0, 0 };
  }

  // ReSharper disable CppRedundantParentheses
  return { (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize), stat.f_blocks * stat.f_frsize };
  // ReSharper restore CppRedundantParentheses
}

#endif
