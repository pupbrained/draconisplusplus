#ifdef __linux__

// clang-format off
#include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h> // SQLite::Exception
#include <SQLiteCpp/Statement.h> // SQLite::Statement
#include <climits>               // PATH_MAX
#include <cstring>               // std::strlen
#include <dbus/dbus-protocol.h>  // DBUS_TYPE_*
#include <dbus/dbus-shared.h>    // DBUS_BUS_SESSION
#include <expected>              // std::{unexpected, expected}
#include <filesystem>            // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
#include <format>                // std::{format, format_to_n}
#include <fstream>               // std::ifstream
#include <glaze/beve/read.hpp>   // glz::read_beve
#include <glaze/beve/write.hpp>  // glz::write_beve
#include <limits>                // std::numeric_limits
#include <string>                // std::{getline, string (String)}
#include <string_view>           // std::string_view (StringView)
#include <sys/socket.h>          // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>         // statvfs
#include <sys/sysinfo.h>         // sysinfo
#include <sys/utsname.h>         // utsname, uname
#include <unistd.h>              // readlink
#include <utility>               // std::move

#include "src/core/package.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/helpers.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"
#include "src/wrappers/dbus.hpp"
#include "src/wrappers/wayland.hpp"
#include "src/wrappers/xcb.hpp"

#include "os.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError, util::error::DracErrorCode;
using util::helpers::GetEnv;

namespace {
  fn GetX11WindowManager() -> Result<String, DracError> {
    using namespace xcb;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = connection_has_error(conn.get()))
        return Err(DracError(DracErrorCode::ApiUnavailable, [&] -> String {
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

    fn internAtom = [&conn](const StringView name) -> Result<atom_t, DracError> {
      const ReplyGuard<intern_atom_reply_t> reply(
        intern_atom_reply(conn.get(), intern_atom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr)
      );

      if (!reply)
        return Err(
          DracError(DracErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name))
        );

      return reply->atom;
    };

    const Result<atom_t, DracError> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<atom_t, DracError> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<atom_t, DracError> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<get_property_reply_t> wmWindowReply(get_property_reply(
      conn.get(),
      get_property(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        get_property_value_length(wmWindowReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const window_t wmRootWindow = *static_cast<window_t*>(get_property_value(wmWindowReply.get()));

    const ReplyGuard<get_property_reply_t> wmNameReply(get_property_reply(
      conn.get(), get_property(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || get_property_value_length(wmNameReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(get_property_value(wmNameReply.get()));
    const usize length   = get_property_value_length(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetWaylandCompositor() -> Result<String, DracError> {
    const wl::DisplayGuard display;

    if (!display)
      return Err(DracError(DracErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(DracError::withErrno("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf;

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DracError(DracErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf; // NOLINT(misc-include-cleaner) - PATH_MAX is in <climits>

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      return Err(DracError::withErrno(std::format("Failed to read link '{}'", exeLinkPath)));

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
      return Err(DracError(DracErrorCode::NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(DracError(DracErrorCode::NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  }
} // namespace

namespace os {
  fn GetOSVersion() -> Result<String, DracError> {
    constexpr CStr path = "/etc/os-release";

    std::ifstream file(path);

    if (!file)
      return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open {}", path)));

    String               line;
    constexpr StringView prefix = "PRETTY_NAME=";

    while (std::getline(file, line)) {
      if (StringView(line).starts_with(prefix)) {
        String value = line.substr(prefix.size());

        if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
            (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
          value = value.substr(1, value.length() - 2);

        if (value.empty())
          return Err(
            DracError(DracErrorCode::ParseError, std::format("PRETTY_NAME value is empty or only quotes in {}", path))
          );

        return value;
      }
    }

    return Err(DracError(DracErrorCode::NotFound, std::format("PRETTY_NAME line not found in {}", path)));
  }

  fn GetMemInfo() -> Result<u64, DracError> {
    struct sysinfo info;

    if (sysinfo(&info) != 0)
      return Err(DracError::withErrno("sysinfo call failed"));

    const u64 totalRam = info.totalram;
    const u64 memUnit  = info.mem_unit;

    if (memUnit == 0)
      return Err(DracError(DracErrorCode::InternalError, "sysinfo returned mem_unit of zero"));

    if (totalRam > std::numeric_limits<u64>::max() / memUnit)
      return Err(DracError(DracErrorCode::InternalError, "Potential overflow calculating total RAM"));

    return info.totalram * info.mem_unit;
  }

  fn GetNowPlaying() -> Result<MediaInfo, DracError> {
    using namespace dbus;

    Result<Connection, DracError> connectionResult = Connection::busGet(DBUS_BUS_SESSION);
    if (!connectionResult)
      return Err(connectionResult.error());

    const Connection& connection = *connectionResult;

    Option<String> activePlayer = None;

    {
      Result<Message, DracError> listNamesResult =
        Message::newMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
      if (!listNamesResult)
        return Err(listNamesResult.error());

      Result<Message, DracError> listNamesReplyResult = connection.sendWithReplyAndBlock(*listNamesResult, 100);
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

    Result<Message, DracError> msgResult = Message::newMethodCall(
      activePlayer->c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get"
    );

    if (!msgResult)
      return Err(msgResult.error());

    Message& msg = *msgResult;

    if (!msg.appendArgs("org.mpris.MediaPlayer2.Player", "Metadata"))
      return Err(DracError(DracErrorCode::InternalError, "Failed to append arguments to Properties.Get message"));

    Result<Message, DracError> replyResult = connection.sendWithReplyAndBlock(msg, 100);

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
        debug_log("Warning: Could not recurse into dict entry, skipping.");
        if (!dictIter.next())
          break;
        continue;
      }

      Option<String> key = entryIter.getString();
      if (!key) {
        debug_log("Warning: Could not get key string from dict entry, skipping.");
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
        } else {
          debug_log("Warning: Artist value was not an array of strings as expected.");
        }
      }

      if (!dictIter.next())
        break;
    }

    return MediaInfo(std::move(title), std::move(artist));
  }

  fn GetWindowManager() -> Result<String, DracError> {
    if (Result<String, DracError> waylandResult = GetWaylandCompositor())
      return *waylandResult;

    if (Result<String, DracError> x11Result = GetX11WindowManager())
      return *x11Result;

    return Err(DracError(DracErrorCode::NotFound, "Could not detect window manager (Wayland/X11) or both failed"));
  }

  fn GetDesktopEnvironment() -> Result<String, DracError> {
    return GetEnv("XDG_CURRENT_DESKTOP")
      .transform([](String xdgDesktop) -> String {
        if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
          xdgDesktop.resize(colon);

        return xdgDesktop;
      })
      .or_else([](const DracError&) -> Result<String, DracError> { return GetEnv("DESKTOP_SESSION"); });
  }

  fn GetShell() -> Result<String, DracError> {
    if (const Result<String, DracError> shellPath = GetEnv("SHELL")) {
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

    return Err(DracError(DracErrorCode::NotFound, "Could not find SHELL environment variable"));
  }

  fn GetHost() -> Result<String, DracError> {
    constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
    constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

    fn readFirstLine = [&](const String& path) -> Result<String, DracError> {
      std::ifstream file(path);
      String        line;

      if (!file)
        return Err(
          DracError(DracErrorCode::NotFound, std::format("Failed to open DMI product identifier file '{}'", path))
        );

      if (!std::getline(file, line) || line.empty())
        return Err(
          DracError(DracErrorCode::ParseError, std::format("DMI product identifier file ('{}') is empty", path))
        );

      return line;
    };

    return readFirstLine(primaryPath).or_else([&](const DracError& primaryError) -> Result<String, DracError> {
      return readFirstLine(fallbackPath).or_else([&](const DracError& fallbackError) -> Result<String, DracError> {
        return Err(DracError(
          DracErrorCode::InternalError,
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

  fn GetKernelVersion() -> Result<String, DracError> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError::withErrno("uname call failed"));

    if (std::strlen(uts.release) == 0)
      return Err(DracError(DracErrorCode::ParseError, "uname returned null kernel release"));

    return uts.release;
  }

  fn GetDiskUsage() -> Result<DiskSpace, DracError> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DracError::withErrno(std::format("Failed to get filesystem stats for '/' (statvfs call failed)")));

    return DiskSpace {
      .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .total_bytes = stat.f_blocks * stat.f_frsize,
    };
  }
} // namespace os

namespace package {
  using namespace std::string_literals;

  fn GetDpkgCount() -> Result<u64, DracError> {
    return GetCountFromDirectory("Dpkg", fs::current_path().root_path() / "var" / "lib" / "dpkg" / "info", ".list"s);
  }

  fn GetMossCount() -> Result<u64, DracError> {
    const PackageManagerInfo mossInfo = {
      .id         = "moss",
      .dbPath     = "/.moss/db/install",
      .countQuery = "SELECT COUNT(*) FROM meta",
    };

    Result<u64, DracError> countResult = GetCountFromDb(mossInfo);

    if (countResult)
      if (*countResult > 0)
        return *countResult - 1;

    return countResult;
  }

  fn GetPacmanCount() -> Result<u64, DracError> {
    return GetCountFromDirectory("Pacman", fs::current_path().root_path() / "var" / "lib" / "pacman" / "local", true);
  }
} // namespace package

#endif // __linux__
