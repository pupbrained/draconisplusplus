#ifdef __linux__

// clang-format off
#include <dbus-cxx.h> // needs to be at top for Success/None
#include <cstring>
#include <fstream>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xcb/xcb.h>

#include "os.h"
#include "src/os/linux/display_guards.h"
#include "src/util/macros.h"
#include "src/util/types.h"
// clang-format on

using namespace std::string_view_literals;

namespace {
  fn GetX11WindowManager() -> Result<String, OsError> {
    using os::linux::XcbReplyGuard;
    using os::linux::XorgDisplayGuard;

    const XorgDisplayGuard conn;

    if (!conn)
      if (const i32 err = xcb_connection_has_error(conn.get()); !conn || err != 0)
        return Err(OsError(OsErrorCode::ApiUnavailable, [&] -> String {
          switch (err) {
            case 0:                                return "Connection object invalid, but no specific XCB error code";
            case XCB_CONN_ERROR:                   return "Stream/Socket/Pipe Error";
            case XCB_CONN_CLOSED_EXT_NOTSUPPORTED: return "Closed: Extension Not Supported";
            case XCB_CONN_CLOSED_MEM_INSUFFICIENT: return "Closed: Insufficient Memory";
            case XCB_CONN_CLOSED_REQ_LEN_EXCEED:   return "Closed: Request Length Exceeded";
            case XCB_CONN_CLOSED_PARSE_ERR:        return "Closed: Display String Parse Error";
            case XCB_CONN_CLOSED_INVALID_SCREEN:   return "Closed: Invalid Screen";
            case XCB_CONN_CLOSED_FDPASSING_FAILED: return "Closed: FD Passing Failed";
            default:                               return std::format("Unknown Error Code ({})", err);
          }
        }()));

    fn internAtom = [&conn](const StringView name) -> Result<xcb_atom_t, OsError> {
      const XcbReplyGuard<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(
        conn.get(), xcb_intern_atom(conn.get(), 0, static_cast<uint16_t>(name.size()), name.data()), nullptr
      ));

      if (!reply)
        return Err(OsError(OsErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name)));

      return reply->atom;
    };

    const Result<xcb_atom_t, OsError> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<xcb_atom_t, OsError> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<xcb_atom_t, OsError> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        ERROR_LOG("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        ERROR_LOG("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        ERROR_LOG("Failed to get UTF8_STRING atom");

      return Err(OsError(OsErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const XcbReplyGuard<xcb_get_property_reply_t> wmWindowReply(xcb_get_property_reply(
      conn.get(),
      xcb_get_property(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, XCB_ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != XCB_ATOM_WINDOW || wmWindowReply->format != 32 ||
        xcb_get_property_value_length(wmWindowReply.get()) == 0)
      return Err(OsError(OsErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const xcb_window_t wmRootWindow = *static_cast<xcb_window_t*>(xcb_get_property_value(wmWindowReply.get()));

    const XcbReplyGuard<xcb_get_property_reply_t> wmNameReply(xcb_get_property_reply(
      conn.get(), xcb_get_property(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || xcb_get_property_value_length(wmNameReply.get()) == 0)
      return Err(OsError(OsErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(xcb_get_property_value(wmNameReply.get()));
    const usize length   = xcb_get_property_value_length(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetWaylandCompositor() -> Result<String, OsError> {
    using os::linux::WaylandDisplayGuard;

    const WaylandDisplayGuard display;

    if (!display)
      return Err(OsError(OsErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(OsError(OsErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(OsError::withErrno("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf;

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(OsError(OsErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf;

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      return Err(OsError::withErrno(std::format("Failed to read link '{}'", exeLinkPath)));

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
      return Err(OsError(OsErrorCode::NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(OsError(OsErrorCode::NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  }

  fn GetMprisPlayers(const SharedPointer<DBus::Connection>& connection) -> Result<String, OsError> {
    try {
      const SharedPointer<DBus::CallMessage> call =
        DBus::CallMessage::create("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

      const SharedPointer<DBus::Message> reply = connection->send_with_reply_blocking(call, 5);

      if (!reply || !reply->is_valid())
        return Err(OsError(OsErrorCode::Timeout, "Failed to get reply from ListNames"));

      Vec<String>           allNamesStd;
      DBus::MessageIterator reader(*reply);
      reader >> allNamesStd;

      for (const String& name : allNamesStd)
        if (StringView(name).contains("org.mpris.MediaPlayer2"sv))
          return name;

      return Err(OsError(OsErrorCode::NotFound, "No MPRIS players found"));
    } catch (const DBus::Error& e) { return Err(OsError::fromDBus(e)); } catch (const Exception& e) {
      return Err(OsError(OsErrorCode::InternalError, e.what()));
    }
  }

  fn GetMediaPlayerMetadata(const SharedPointer<DBus::Connection>& connection, const String& playerBusName)
    -> Result<MediaInfo, OsError> {
    try {
      const SharedPointer<DBus::CallMessage> metadataCall =
        DBus::CallMessage::create(playerBusName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");

      *metadataCall << "org.mpris.MediaPlayer2.Player" << "Metadata";

      const SharedPointer<DBus::Message> metadataReply = connection->send_with_reply_blocking(metadataCall, 1000);

      if (!metadataReply || !metadataReply->is_valid()) {
        return Err(OsError(OsErrorCode::Timeout, "DBus Get Metadata call timed out or received invalid reply"));
      }

      DBus::MessageIterator iter(*metadataReply);
      DBus::Variant         metadataVariant;
      iter >> metadataVariant; // Can throw

      // MPRIS metadata is variant containing a dict a{sv}
      if (metadataVariant.type() != DBus::DataType::DICT_ENTRY && metadataVariant.type() != DBus::DataType::ARRAY) {
        return Err(OsError(
          OsErrorCode::ParseError,
          std::format(
            "Inner metadata variant is not the expected type, expected dict/a{{sv}} but got '{}'",
            metadataVariant.signature().str()
          )
        ));
      }

      Map<String, DBus::Variant> metadata = metadataVariant.to_map<String, DBus::Variant>(); // Can throw

      Option<String> title   = None;
      Option<String> artist  = None;
      Option<String> album   = None;
      Option<String> appName = None; // Try to get app name too

      if (auto titleIter = metadata.find("xesam:title");
          titleIter != metadata.end() && titleIter->second.type() == DBus::DataType::STRING)
        title = titleIter->second.to_string();

      if (auto artistIter = metadata.find("xesam:artist"); artistIter != metadata.end()) {
        if (artistIter->second.type() == DBus::DataType::ARRAY) {
          if (Vec<String> artists = artistIter->second.to_vector<String>(); !artists.empty())
            artist = artists[0];
        } else if (artistIter->second.type() == DBus::DataType::STRING) {
          artist = artistIter->second.to_string();
        }
      }

      if (auto albumIter = metadata.find("xesam:album");
          albumIter != metadata.end() && albumIter->second.type() == DBus::DataType::STRING)
        album = albumIter->second.to_string();

      try {
        const SharedPointer<DBus::CallMessage> identityCall =
          DBus::CallMessage::create(playerBusName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");
        *identityCall << "org.mpris.MediaPlayer2" << "Identity";
        if (const SharedPointer<DBus::Message> identityReply = connection->send_with_reply_blocking(identityCall, 500);
            identityReply && identityReply->is_valid()) {
          DBus::MessageIterator identityIter(*identityReply);
          DBus::Variant         identityVariant;
          identityIter >> identityVariant;
          if (identityVariant.type() == DBus::DataType::STRING)
            appName = identityVariant.to_string();
        }
      } catch (const DBus::Error& e) {
        DEBUG_LOG("Failed to get player Identity property for {}: {}", playerBusName, e.what()); // Non-fatal
      }

      return MediaInfo(std::move(title), std::move(artist), std::move(album), std::move(appName));
    } catch (const DBus::Error& e) { return Err(OsError::fromDBus(e)); } catch (const Exception& e) {
      return Err(
        OsError(OsErrorCode::InternalError, std::format("Standard exception processing metadata: {}", e.what()))
      );
    }
  }
} // namespace

fn os::GetOSVersion() -> Result<String, OsError> {
  constexpr CStr path = "/etc/os-release";

  std::ifstream file(path);

  if (!file)
    return Err(OsError(OsErrorCode::NotFound, std::format("Failed to open {}", path)));

  String               line;
  constexpr StringView prefix = "PRETTY_NAME=";

  while (getline(file, line)) {
    if (StringView(line).starts_with(prefix)) {
      String value = line.substr(prefix.size());

      if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
          (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
        value = value.substr(1, value.length() - 2);

      if (value.empty())
        return Err(
          OsError(OsErrorCode::ParseError, std::format("PRETTY_NAME value is empty or only quotes in {}", path))
        );

      return value;
    }
  }

  return Err(OsError(OsErrorCode::NotFound, std::format("PRETTY_NAME line not found in {}", path)));
}

fn os::GetMemInfo() -> Result<u64, OsError> {
  struct sysinfo info;

  if (sysinfo(&info) != 0)
    return Err(OsError::fromDBus("sysinfo call failed"));

  const u64 totalRam = info.totalram;
  const u64 memUnit  = info.mem_unit;

  if (memUnit == 0)
    return Err(OsError(OsErrorCode::InternalError, "sysinfo returned mem_unit of zero"));

  if (totalRam > std::numeric_limits<u64>::max() / memUnit)
    return Err(OsError(OsErrorCode::InternalError, "Potential overflow calculating total RAM"));

  return info.totalram * info.mem_unit;
}

fn os::GetNowPlaying() -> Result<MediaInfo, NowPlayingError> {
  // Dispatcher must outlive the try-block because 'connection' depends on it later.
  // ReSharper disable once CppTooWideScope, CppJoinDeclarationAndAssignment
  SharedPointer<DBus::Dispatcher> dispatcher;
  SharedPointer<DBus::Connection> connection;

  try {
    dispatcher = DBus::StandaloneDispatcher::create();

    if (!dispatcher)
      return Err(OsError(OsErrorCode::ApiUnavailable, "Failed to create DBus dispatcher"));

    connection = dispatcher->create_connection(DBus::BusType::SESSION);

    if (!connection)
      return Err(OsError(OsErrorCode::ApiUnavailable, "Failed to connect to DBus session bus"));
  } catch (const DBus::Error& e) { return Err(OsError::fromDBus(e)); } catch (const Exception& e) {
    return Err(OsError(OsErrorCode::InternalError, e.what()));
  }

  Result<String, NowPlayingError> playerBusName = GetMprisPlayers(connection);

  if (!playerBusName)
    return Err(playerBusName.error());

  Result<MediaInfo, OsError> metadataResult = GetMediaPlayerMetadata(connection, *playerBusName);

  if (!metadataResult)
    return Err(metadataResult.error());

  return std::move(*metadataResult);
}

fn os::GetWindowManager() -> Option<String> {
  if (Result<String, OsError> waylandResult = GetWaylandCompositor())
    return *waylandResult;
  else
    DEBUG_LOG("Could not detect Wayland compositor: {}", waylandResult.error().message);

  if (Result<String, OsError> x11Result = GetX11WindowManager())
    return *x11Result;
  else
    DEBUG_LOG("Could not detect X11 window manager: {}", x11Result.error().message);

  return None;
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

fn os::GetShell() -> Option<String> {
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

  return None;
}

fn os::GetHost() -> Result<String, OsError> {
  constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
  constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

  fn readFirstLine = [&](const String& path) -> Result<String, OsError> {
    std::ifstream file(path);
    String        line;

    if (!file)
      return Err(OsError(OsErrorCode::NotFound, std::format("Failed to open DMI product identifier file '{}'", path)));

    if (!getline(file, line))
      return Err(OsError(OsErrorCode::ParseError, std::format("DMI product identifier file ('{}') is empty", path)));

    return line;
  };

  return readFirstLine(primaryPath).or_else([&](const OsError& primaryError) -> Result<String, OsError> {
    return readFirstLine(fallbackPath).or_else([&](const OsError& fallbackError) -> Result<String, OsError> {
      return Err(OsError(
        OsErrorCode::InternalError,
        std::format(
          "Failed to get host identifier. Primary ('{}'): {}. Fallback ('{}'): {}",
          primaryPath,
          primaryError.message,
          fallbackPath,
          fallbackError.message
        )
      ));
    });
  });
}

fn os::GetKernelVersion() -> Result<String, OsError> {
  utsname uts;

  if (uname(&uts) == -1)
    return Err(OsError::withErrno("uname call failed"));

  if (strlen(uts.release) == 0)
    return Err(OsError(OsErrorCode::ParseError, "uname returned null kernel release"));

  return uts.release;
}

fn os::GetDiskUsage() -> Result<DiskSpace, OsError> {
  struct statvfs stat;

  if (statvfs("/", &stat) == -1)
    return Err(OsError::withErrno(std::format("Failed to get filesystem stats for '/' (statvfs call failed)")));

  return DiskSpace {
    .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
    .total_bytes = stat.f_blocks * stat.f_frsize,
  };
}

#endif // __linux__
