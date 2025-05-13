#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <dbus/dbus-protocol.h> // DBUS_TYPE_*
#include <dbus/dbus-shared.h>   // DBUS_BUS_SESSION
#include <fstream>              // ifstream
#include <sys/socket.h>         // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>        // statvfs
#include <sys/sysctl.h>         // sysctlbyname
#include <sys/un.h>             // LOCAL_PEERCRED
#include <sys/utsname.h>        // uname, utsname

#ifndef __NetBSD__
  #include <kenv.h>      // kenv
  #include <sys/ucred.h> // xucred
#endif

#include "Services/PackageCounting.hpp"
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Env.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"
#include "Wrappers/DBus.hpp"
#include "Wrappers/Wayland.hpp"
#include "Wrappers/XCB.hpp"

#include "OperatingSystem.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError, util::error::DracErrorCode;

namespace {
  #ifdef __FreeBSD__
  fn GetPathByPid(pid_t pid) -> Result<String> {
    Array<char, PATH_MAX> exePathBuf;
    usize                 size = exePathBuf.size();
    Array<i32, 4>         mib;

    mib.at(0) = CTL_KERN;
    mib.at(1) = KERN_PROC_ARGS;
    mib.at(2) = pid;
    mib.at(3) = KERN_PROC_PATHNAME;

    if (sysctl(mib.data(), 4, exePathBuf.data(), &size, nullptr, 0) == -1)
      return Err(DracError(std::format("sysctl KERN_PROC_PATHNAME failed for pid {}", pid)));

    if (size == 0 || exePathBuf[0] == '\0')
      return Err(
        DracError(DracErrorCode::NotFound, std::format("sysctl KERN_PROC_PATHNAME returned empty path for pid {}", pid))
      );

    exePathBuf.at(std::min(size, exePathBuf.size() - 1)) = '\0';

    return String(exePathBuf.data());
  }
  #endif

  #ifdef HAVE_XCB
  fn GetX11WindowManager() -> Result<String> {
    using namespace xcb;
    using namespace matchit;
    using enum ConnError;
    using util::types::StringView;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = ConnectionHasError(conn.get()))
        return Err(
          DracError(
            DracErrorCode::ApiUnavailable,
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

    fn internAtom = [&conn](const StringView name) -> Result<atom_t> {
      using util::types::u16;

      const ReplyGuard<intern_atom_reply_t> reply(InternAtomReply(conn.get(), InternAtom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr));

      if (!reply)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name)));

      return reply->atom;
    };

    const Result<atom_t> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<atom_t> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<atom_t> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<get_property_reply_t> wmWindowReply(GetPropertyReply(
      conn.get(),
      GetProperty(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        GetPropertyValueLength(wmWindowReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const window_t wmRootWindow = *static_cast<window_t*>(GetPropertyValue(wmWindowReply.get()));

    const ReplyGuard<get_property_reply_t> wmNameReply(GetPropertyReply(
      conn.get(), GetProperty(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || GetPropertyValueLength(wmNameReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(GetPropertyValue(wmNameReply.get()));
    const usize length   = GetPropertyValueLength(wmNameReply.get());

    return String(nameData, length);
  }
  #else
  fn GetX11WindowManager() -> Result<String> {
    return Err(DracError(DracErrorCode::NotSupported, "XCB (X11) support not available"));
  }
  #endif

  fn GetWaylandCompositor() -> Result<String> {
  #ifndef __FreeBSD__
    return "Wayland Compositor";
  #else
    const wl::DisplayGuard display;

    if (!display)
      return Err(DracError(DracErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    pid_t peerPid = -1; // Initialize PID

    struct xucred cred;

    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, LOCAL_PEERCRED, &cred, &len) == -1)
      return Err(DracError("Failed to get socket credentials (LOCAL_PEERCRED)"));

    peerPid = cred.cr_pid;

    if (peerPid <= 0)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to obtain a valid peer PID"));

    Result<String> exePathResult = GetPathByPid(peerPid);

    if (!exePathResult)
      return Err(std::move(exePathResult).error());

    const String& exeRealPath = *exePathResult;

    StringView compositorNameView;

    if (const usize lastSlash = exeRealPath.rfind('/'); lastSlash != String::npos)
      compositorNameView = StringView(exeRealPath).substr(lastSlash + 1);
    else
      compositorNameView = exeRealPath;

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
  #endif
  }
} // namespace

namespace os {
  using util::helpers::GetEnv;

  fn GetOSVersion() -> Result<String> {
    constexpr CStr path = "/etc/os-release";

    std::ifstream file(path);

    if (file) {
      String               line;
      constexpr StringView prefix = "NAME=";

      while (std::getline(file, line)) {
        if (StringView(line).starts_with(prefix)) {
          String value = line.substr(prefix.size());

          if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
              (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
            value = value.substr(1, value.length() - 2);

          return value;
        }
      }
    }

    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError(std::format("Failed to open {} and uname() call also failed", path)));

    String osName = uts.sysname;

    if (osName.empty())
      return Err(DracError(DracErrorCode::ParseError, "uname() returned empty sysname or release"));

    return osName;
  }

  fn GetMemInfo() -> Result<u64> {
    u64   mem  = 0;
    usize size = sizeof(mem);

  #ifdef __NetBSD__
    sysctlbyname("hw.physmem64", &mem, &size, nullptr, 0);
  #else
    sysctlbyname("hw.physmem", &mem, &size, nullptr, 0);
  #endif

    return mem;
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    using namespace dbus;

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

  fn GetWindowManager() -> Result<String> {
    if (!GetEnv("DISPLAY") && !GetEnv("WAYLAND_DISPLAY") && !GetEnv("XDG_SESSION_TYPE"))
      return Err(DracError(DracErrorCode::NotFound, "Could not find a graphical session"));

    if (Result<String> waylandResult = GetWaylandCompositor())
      return *waylandResult;

    if (Result<String> x11Result = GetX11WindowManager())
      return *x11Result;

    return Err(DracError(DracErrorCode::NotFound, "Could not detect window manager (Wayland/X11) or both failed"));
  }

  fn GetDesktopEnvironment() -> Result<String> {
    if (!GetEnv("DISPLAY") && !GetEnv("WAYLAND_DISPLAY") && !GetEnv("XDG_SESSION_TYPE"))
      return Err(DracError(DracErrorCode::NotFound, "Could not find a graphical session"));

    return GetEnv("XDG_CURRENT_DESKTOP")
      .transform([](String xdgDesktop) -> String {
        if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
          xdgDesktop.resize(colon);

        return xdgDesktop;
      })
      .or_else([](const DracError&) -> Result<String> { return GetEnv("DESKTOP_SESSION"); });
  }

  fn GetShell() -> Result<String> {
    if (const Result<String> shellPath = GetEnv("SHELL")) {
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

  fn GetHost() -> Result<String> {
    Array<char, 256> buffer {};
    usize            size = buffer.size();

  #if defined(__FreeBSD__) || defined(__DragonFly__)
    int result = kenv(KENV_GET, "smbios.system.product", buffer.data(), buffer.size() - 1); // Ensure space for null

    if (result == -1) {
      if (sysctlbyname("hw.model", buffer.data(), &size, nullptr, 0) == -1)
        return Err(DracError("kenv smbios.system.product failed and sysctl hw.model also failed"));

      buffer.at(std::min(size, buffer.size() - 1)) = '\0';
      return String(buffer.data());
    }

    if (result > 0)
      buffer.at(result) = '\0';
    else
      buffer.at(0) = '\0';

  #elifdef __NetBSD__
    if (sysctlbyname("machdep.dmi.system-product", buffer.data(), &size, nullptr, 0) == -1)
      return Err(DracError(std::format("sysctlbyname failed for")));

    buffer[std::min(size, buffer.size() - 1)] = '\0';
  #endif
    if (buffer[0] == '\0')
      return Err(DracError(DracErrorCode::NotFound, "Failed to get host product information (empty result)"));

    return String(buffer.data());
  }

  fn GetKernelVersion() -> Result<String> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError("uname call failed"));

    if (std::strlen(uts.release) == 0)
      return Err(DracError(DracErrorCode::ParseError, "uname returned null kernel release"));

    return uts.release;
  }

  fn GetDiskUsage() -> Result<DiskSpace> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DracError(std::format("Failed to get filesystem stats for '/' (statvfs call failed)")));

    return DiskSpace {
      .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .total_bytes = stat.f_blocks * stat.f_frsize,
    };
  }
} // namespace os

namespace package {
  #ifdef __NetBSD__
  fn GetPkgSrcCount() -> Result<u64> {
    return GetCountFromDirectory("pkgsrc", fs::current_path().root_path() / "usr" / "pkg" / "pkgdb", true);
  }
  #else
  fn GetPkgNgCount() -> Result<u64> {
    return GetCountFromDb("pkgng", "/var/db/pkg/local.sqlite", "SELECT COUNT(*) FROM packages");
  }
  #endif
} // namespace package

#endif // __FreeBSD__ || __DragonFly__ || __NetBSD__
