#ifdef __linux__

// clang-format off
#include <cstring>                          // std::strlen
#include <dbus-cxx/callmessage.h>           // DBus::CallMessage
#include <dbus-cxx/connection.h>            // DBus::Connection
#include <dbus-cxx/dispatcher.h>            // DBus::Dispatcher
#include <dbus-cxx/enums.h>                 // DBus::{DataType, BusType}
#include <dbus-cxx/error.h>                 // DBus::Error
#include <dbus-cxx/messageappenditerator.h> // DBus::MessageAppendIterator
#include <dbus-cxx/signature.h>             // DBus::Signature
#include <dbus-cxx/standalonedispatcher.h>  // DBus::StandaloneDispatcher
#include <dbus-cxx/variant.h>               // DBus::Variant
#include <expected>                         // std::{unexpected, expected}
#include <format>                           // std::{format, format_to_n}
#include <fstream>                          // std::ifstream
#include <climits>                          // PATH_MAX
#include <limits>                           // std::numeric_limits
#include <map>                              // std::map (Map)
#include <memory>                           // std::shared_ptr (SharedPointer)
#include <string>                           // std::{getline, string (String)}
#include <string_view>                      // std::string_view (StringView)
#include <sys/socket.h>                     // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>                    // statvfs
#include <sys/sysinfo.h>                    // sysinfo
#include <sys/utsname.h>                    // utsname, uname
#include <unistd.h>                         // readlink
#include <utility>                          // std::move

#include "src/core/util/logging.hpp"
#include "src/core/util/helpers.hpp"

#include "src/wrappers/wayland.hpp"
#include "src/wrappers/xcb.hpp"

#include "os.hpp"
#include "linux/pkg_count.hpp"
// clang-format on

using namespace util::types;
using util::error::DraconisError, util::error::DraconisErrorCode;

namespace {
  fn GetX11WindowManager() -> Result<String, DraconisError> {
    using namespace xcb;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = connection_has_error(conn.get()))
        return Err(DraconisError(DraconisErrorCode::ApiUnavailable, [&] -> String {
          if (const Option<ConnError> connErr = getConnError(err)) {
            switch (*connErr) {
              case Generic:         return "Stream/Socket/Pipe Error";
              case ExtNotSupported: return "Extension Not Supported";
              case MemInsufficient: return "Insufficient Memory";
              case ReqLenExceed:    return "Request Length Exceeded";
              case ParseErr:        return "Display String Parse Error";
              case InvalidScreen:   return "Invalid Screen";
              case FdPassingFailed: return "FD Passing Failed";
              default:              return std::format("Unknown Error Code ({})", err);
            }
          }

          return std::format("Unknown Error Code ({})", err);
        }()));

    fn internAtom = [&conn](const StringView name) -> Result<atom_t, DraconisError> {
      const ReplyGuard<intern_atom_reply_t> reply(
        intern_atom_reply(conn.get(), intern_atom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr)
      );

      if (!reply)
        return Err(
          DraconisError(DraconisErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name))
        );

      return reply->atom;
    };

    const Result<atom_t, DraconisError> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<atom_t, DraconisError> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<atom_t, DraconisError> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DraconisError(DraconisErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<get_property_reply_t> wmWindowReply(get_property_reply(
      conn.get(),
      get_property(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        get_property_value_length(wmWindowReply.get()) == 0)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const window_t wmRootWindow = *static_cast<window_t*>(get_property_value(wmWindowReply.get()));

    const ReplyGuard<get_property_reply_t> wmNameReply(get_property_reply(
      conn.get(), get_property(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || get_property_value_length(wmNameReply.get()) == 0)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(get_property_value(wmNameReply.get()));
    const usize length   = get_property_value_length(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetWaylandCompositor() -> Result<String, DraconisError> {
    const wl::DisplayGuard display;

    if (!display)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(DraconisError::withErrno("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf;

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DraconisError(DraconisErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf;

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      return Err(DraconisError::withErrno(std::format("Failed to read link '{}'", exeLinkPath)));

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
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(DraconisError(DraconisErrorCode::NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  }

  fn GetMprisPlayers(const SharedPointer<DBus::Connection>& connection) -> Result<String, DraconisError> {
    using namespace std::string_view_literals;

    try {
      const SharedPointer<DBus::CallMessage> call =
        DBus::CallMessage::create("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

      const SharedPointer<DBus::Message> reply = connection->send_with_reply_blocking(call, 5);

      if (!reply || !reply->is_valid())
        return Err(DraconisError(DraconisErrorCode::Timeout, "Failed to get reply from ListNames"));

      Vec<String>           allNamesStd;
      DBus::MessageIterator reader(*reply);
      reader >> allNamesStd;

      for (const String& name : allNamesStd)
        if (StringView(name).contains("org.mpris.MediaPlayer2"sv))
          return name;

      return Err(DraconisError(DraconisErrorCode::NotFound, "No MPRIS players found"));
    } catch (const DBus::Error& e) { return Err(DraconisError::fromDBus(e)); } catch (const Exception& e) {
      return Err(DraconisError(DraconisErrorCode::InternalError, e.what()));
    }
  }

  fn GetMediaPlayerMetadata(const SharedPointer<DBus::Connection>& connection, const String& playerBusName)
    -> Result<MediaInfo, DraconisError> {
    try {
      const SharedPointer<DBus::CallMessage> metadataCall =
        DBus::CallMessage::create(playerBusName, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");

      *metadataCall << "org.mpris.MediaPlayer2.Player" << "Metadata";

      const SharedPointer<DBus::Message> metadataReply = connection->send_with_reply_blocking(metadataCall, 1000);

      if (!metadataReply || !metadataReply->is_valid()) {
        return Err(
          DraconisError(DraconisErrorCode::Timeout, "DBus Get Metadata call timed out or received invalid reply")
        );
      }

      DBus::MessageIterator iter(*metadataReply);
      DBus::Variant         metadataVariant;
      iter >> metadataVariant; // Can throw

      // MPRIS metadata is variant containing a dict a{sv}
      if (metadataVariant.type() != DBus::DataType::DICT_ENTRY && metadataVariant.type() != DBus::DataType::ARRAY) {
        return Err(DraconisError(
          DraconisErrorCode::ParseError,
          std::format(
            "Inner metadata variant is not the expected type, expected dict/a{{sv}} but got '{}'",
            metadataVariant.signature().str()
          )
        ));
      }

      Map<String, DBus::Variant> metadata = metadataVariant.to_map<String, DBus::Variant>(); // Can throw

      Option<String> title  = None;
      Option<String> artist = None;

      if (const auto titleIter = metadata.find("xesam:title");
          titleIter != metadata.end() && titleIter->second.type() == DBus::DataType::STRING)
        title = titleIter->second.to_string();

      if (const auto artistIter = metadata.find("xesam:artist"); artistIter != metadata.end()) {
        if (artistIter->second.type() == DBus::DataType::ARRAY) {
          if (Vec<String> artists = artistIter->second.to_vector<String>(); !artists.empty())
            artist = artists[0];
        } else if (artistIter->second.type() == DBus::DataType::STRING) {
          artist = artistIter->second.to_string();
        }
      }

      return MediaInfo(std::move(title), std::move(artist));
    } catch (const DBus::Error& e) { return Err(DraconisError::fromDBus(e)); } catch (const Exception& e) {
      return Err(DraconisError(
        DraconisErrorCode::InternalError, std::format("Standard exception processing metadata: {}", e.what())
      ));
    }
  }
} // namespace

fn os::GetOSVersion() -> Result<String, DraconisError> {
  constexpr CStr path = "/etc/os-release";

  std::ifstream file(path);

  if (!file)
    return Err(DraconisError(DraconisErrorCode::NotFound, std::format("Failed to open {}", path)));

  String               line;
  constexpr StringView prefix = "PRETTY_NAME=";

  while (std::getline(file, line)) {
    if (StringView(line).starts_with(prefix)) {
      String value = line.substr(prefix.size());

      if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
          (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
        value = value.substr(1, value.length() - 2);

      if (value.empty())
        return Err(DraconisError(
          DraconisErrorCode::ParseError, std::format("PRETTY_NAME value is empty or only quotes in {}", path)
        ));

      return value;
    }
  }

  return Err(DraconisError(DraconisErrorCode::NotFound, std::format("PRETTY_NAME line not found in {}", path)));
}

fn os::GetMemInfo() -> Result<u64, DraconisError> {
  struct sysinfo info;

  if (sysinfo(&info) != 0)
    return Err(DraconisError::fromDBus("sysinfo call failed"));

  const u64 totalRam = info.totalram;
  const u64 memUnit  = info.mem_unit;

  if (memUnit == 0)
    return Err(DraconisError(DraconisErrorCode::InternalError, "sysinfo returned mem_unit of zero"));

  if (totalRam > std::numeric_limits<u64>::max() / memUnit)
    return Err(DraconisError(DraconisErrorCode::InternalError, "Potential overflow calculating total RAM"));

  return info.totalram * info.mem_unit;
}

fn os::GetNowPlaying() -> Result<MediaInfo, DraconisError> {
  // Dispatcher must outlive the try-block because 'connection' depends on it later.
  // ReSharper disable once CppTooWideScope, CppJoinDeclarationAndAssignment
  SharedPointer<DBus::Dispatcher> dispatcher;
  SharedPointer<DBus::Connection> connection;

  try {
    dispatcher = DBus::StandaloneDispatcher::create();

    if (!dispatcher)
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Failed to create DBus dispatcher"));

    connection = dispatcher->create_connection(DBus::BusType::SESSION);

    if (!connection)
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Failed to connect to DBus session bus"));
  } catch (const DBus::Error& e) { return Err(DraconisError::fromDBus(e)); } catch (const Exception& e) {
    return Err(DraconisError(DraconisErrorCode::InternalError, e.what()));
  }

  Result<String, DraconisError> playerBusName = GetMprisPlayers(connection);

  if (!playerBusName)
    return Err(playerBusName.error());

  Result<MediaInfo, DraconisError> metadataResult = GetMediaPlayerMetadata(connection, *playerBusName);

  if (!metadataResult)
    return Err(metadataResult.error());

  return std::move(*metadataResult);
}

fn os::GetWindowManager() -> Option<String> {
  if (Result<String, DraconisError> waylandResult = GetWaylandCompositor())
    return *waylandResult;
  else
    debug_log("Could not detect Wayland compositor: {}", waylandResult.error().message);

  if (Result<String, DraconisError> x11Result = GetX11WindowManager())
    return *x11Result;
  else
    debug_log("Could not detect X11 window manager: {}", x11Result.error().message);

  return None;
}

fn os::GetDesktopEnvironment() -> Option<String> {
  return util::helpers::GetEnv("XDG_CURRENT_DESKTOP")
    .transform([](const String& xdgDesktop) -> String {
      if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
        return xdgDesktop.substr(0, colon);

      return xdgDesktop;
    })
    .or_else([](const DraconisError&) -> Result<String, DraconisError> {
      return util::helpers::GetEnv("DESKTOP_SESSION");
    })
    .transform([](const String& finalValue) -> Option<String> {
      debug_log("Found desktop environment: {}", finalValue);
      return finalValue;
    })
    .value_or(None);
}

fn os::GetShell() -> Option<String> {
  if (const Result<String, DraconisError> shellPath = util::helpers::GetEnv("SHELL")) {
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

fn os::GetHost() -> Result<String, DraconisError> {
  constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
  constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

  fn readFirstLine = [&](const String& path) -> Result<String, DraconisError> {
    std::ifstream file(path);
    String        line;

    if (!file)
      return Err(
        DraconisError(DraconisErrorCode::NotFound, std::format("Failed to open DMI product identifier file '{}'", path))
      );

    if (!std::getline(file, line))
      return Err(
        DraconisError(DraconisErrorCode::ParseError, std::format("DMI product identifier file ('{}') is empty", path))
      );

    return line;
  };

  return readFirstLine(primaryPath).or_else([&](const DraconisError& primaryError) -> Result<String, DraconisError> {
    return readFirstLine(fallbackPath)
      .or_else([&](const DraconisError& fallbackError) -> Result<String, DraconisError> {
        return Err(DraconisError(
          DraconisErrorCode::InternalError,
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

fn os::GetKernelVersion() -> Result<String, DraconisError> {
  utsname uts;

  if (uname(&uts) == -1)
    return Err(DraconisError::withErrno("uname call failed"));

  if (std::strlen(uts.release) == 0)
    return Err(DraconisError(DraconisErrorCode::ParseError, "uname returned null kernel release"));

  return uts.release;
}

fn os::GetDiskUsage() -> Result<DiskSpace, DraconisError> {
  struct statvfs stat;

  if (statvfs("/", &stat) == -1)
    return Err(DraconisError::withErrno(std::format("Failed to get filesystem stats for '/' (statvfs call failed)")));

  return DiskSpace {
    .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
    .total_bytes = stat.f_blocks * stat.f_frsize,
  };
}

fn os::GetPackageCount() -> Result<u64, DraconisError> { return linux::GetTotalPackageCount(); }

#endif // __linux__
