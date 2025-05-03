#ifdef __HAIKU__

// clang-format off
#include <File.h>        // For BFile
#include <AppFileInfo.h> // For BAppFileInfo and version_info
#include <String.h>      // For BString (optional, can use std::string
#include <Errors.h>                    // Haiku specific: Defines B_OK and strerror function
#include <OS.h>                        // Haiku specific: Defines get_system_info, system_info, status_t
#include <climits>                     // PATH_MAX
#include <cstring>                     // std::strlen
#include <dbus/dbus-protocol.h>        // DBUS_TYPE_*
#include <dbus/dbus-shared.h>          // DBUS_BUS_SESSION
#include <os/package/PackageDefs.h>    // Should define typedef ... BPackageKit::BPackageInfoSet;
#include <os/package/PackageInfoSet.h> // Defines BPackageKit::BPackageInfo (template argument)
#include <os/package/PackageRoster.h>  // Defines BPackageKit::BPackageRoster
#include <support/Errors.h>            // For B_OK, status_t, strerror
#include <sys/socket.h>                // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>               // statvfs
#include <utility>                     // std::move

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

    Array<char, 128> exeLinkPathBuf {};

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DracError(DracErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf {};

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
    BFile file;
    status_t status = file.SetTo("/boot/system/lib/libbe.so", B_READ_ONLY);

    if (status != B_OK) {
        return Err(DracError(DracErrorCode::InternalError, "Error opening /boot/system/lib/libbe.so"));
    }

    BAppFileInfo appInfo;
    status = appInfo.SetTo(&file);

    if (status != B_OK) {
        return Err(DracError(DracErrorCode::InternalError, "Error initializing BAppFileInfo"));
    }

    version_info versionInfo;
    status = appInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND);

    if (status != B_OK) {
        return Err(DracError(DracErrorCode::InternalError, "Error reading version info attribute"));
    }

    std::string versionShortString = versionInfo.short_info;

    if (versionShortString.empty()) {
        return Err(DracError(DracErrorCode::InternalError, "Version info short_info is empty"));
    }

    return std::format("Haiku {}", versionShortString);
  }

  fn GetMemInfo() -> Result<u64, DracError> {
    system_info sysinfo;
    const status_t    status = get_system_info(&sysinfo);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, std::format("get_system_info failed: {}", strerror(status))));

    return static_cast<u64>(sysinfo.max_pages) * B_PAGE_SIZE;
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
    Array<char, HOST_NAME_MAX + 1> hostnameBuffer {};

    if (gethostname(hostnameBuffer.data(), hostnameBuffer.size()) != 0) 
      return Err(DracError(
        DracErrorCode::ApiUnavailable,
        std::format("gethostname() failed: {} (errno {})", strerror(errno), errno)
      ));

    hostnameBuffer.at(HOST_NAME_MAX) = '\0';

    return String(hostnameBuffer.data(), hostnameBuffer.size());
  }

  fn GetKernelVersion() -> Result<String, DracError> {
    system_info sysinfo;
    const status_t    status = get_system_info(&sysinfo);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, std::format("get_system_info failed: {}", strerror(status))));

    return std::to_string(sysinfo.kernel_version);
  }

  fn GetDiskUsage() -> Result<DiskSpace, DracError> {
    struct statvfs stat;

    if (statvfs("/boot", &stat) == -1)
      return Err(DracError::withErrno(std::format("Failed to get filesystem stats for '/boot' (statvfs call failed)")));

    return DiskSpace {
      .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .total_bytes = stat.f_blocks * stat.f_frsize,
    };
  }

  fn GetPackageCount() -> Result<u64, DracError> {
    u64 count = 0;

    BPackageKit::BPackageRoster  roster;
    BPackageKit::BPackageInfoSet packageList;

    const status_t status = roster.GetActivePackages(BPackageKit::B_PACKAGE_INSTALLATION_LOCATION_SYSTEM, packageList);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get active package list"));

    count += static_cast<u64>(packageList.CountInfos());

    if (Result<u64, DracError> sharedCount = shared::GetPackageCount())
      count += *sharedCount;
    else
      debug_at(sharedCount.error());

    return count;
  }
} // namespace os

#endif // __HAIKU__
