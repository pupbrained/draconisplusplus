#ifdef __linux__

// clang-format off
#include <climits>              // PATH_MAX
#include <cstring>              // std::strlen
#include <expected>             // std::{unexpected, expected}
#include <filesystem>           // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
#include <format>               // std::{format, format_to_n}
#include <fstream>              // std::ifstream
#include <glaze/beve/read.hpp>  // glz::read_beve
#include <glaze/beve/write.hpp> // glz::write_beve
#include <limits>               // std::numeric_limits
#include <matchit.hpp>          // matchit::{is, is_not, is_any, etc.}
#include <string>               // std::{getline, string (String)}
#include <string_view>          // std::string_view (StringView)
#include <sys/socket.h>         // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>        // statvfs
#include <sys/sysinfo.h>        // sysinfo
#include <sys/utsname.h>        // utsname, uname
#include <unistd.h>             // readlink
#include <utility>              // std::move

#include "Services/PackageCounting.hpp"

#include "Util/Caching.hpp"
#include "Util/Definitions.hpp"
#include "Util/Env.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "Wrappers/DBus.hpp"
#include "Wrappers/Wayland.hpp"
#include "Wrappers/XCB.hpp"

#include "OperatingSystem.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::String, util::types::Result, util::types::Err, util::types::usize;

// clang-format off
#ifdef __GLIBC__
extern "C" fn issetugid() -> usize { return 0; } // NOLINT(readability-identifier-naming) - glibc function stub
#endif
// clang-format on

namespace {
  #ifdef HAVE_XCB
  fn GetX11WindowManager() -> Result<String> {
    using namespace XCB;
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

    fn internAtom = [&conn](const StringView name) -> Result<Atom> {
      using util::types::u16;

      const ReplyGuard<IntAtomReply> reply(InternAtomReply(conn.get(), InternAtom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr));

      if (!reply)
        return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name)));

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

      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<GetPropReply> wmWindowReply(GetPropertyReply(
      conn.get(),
      GetProperty(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        GetPropertyValueLength(wmWindowReply.get()) == 0)
      return Err(DracError(DracErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const Window wmRootWindow = *static_cast<Window*>(GetPropertyValue(wmWindowReply.get()));

    const ReplyGuard<GetPropReply> wmNameReply(GetPropertyReply(
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

  #ifdef HAVE_WAYLAND
  fn GetWaylandCompositor() -> Result<String> {
    using util::types::i32, util::types::Array, util::types::isize, util::types::StringView;

    const Wayland::DisplayGuard display;

    if (!display)
      return Err(DracError(DracErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(DracError("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf {};

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DracError(DracErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

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
  #else
  fn GetWaylandCompositor() -> Result<String> {
    return Err(DracError(DracErrorCode::NotSupported, "Wayland support not available"));
  }
  #endif
} // namespace

namespace os {
  using util::helpers::GetEnv;

  fn GetOSVersion() -> Result<String> {
    using util::types::StringView;

    std::ifstream file("/etc/os-release");

    if (!file)
      return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open /etc/os-release")));

    String               line;
    constexpr StringView prefix = "PRETTY_NAME=";

    while (std::getline(file, line)) {
      if (StringView(line).starts_with(prefix)) {
        String value = line.substr(prefix.size());

        if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
            (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
          value = value.substr(1, value.length() - 2);

        if (value.empty())
          return Err(DracError(DracErrorCode::ParseError, std::format("PRETTY_NAME value is empty or only quotes in /etc/os-release")));

        return value;
      }
    }

    return Err(DracError(DracErrorCode::NotFound, "PRETTY_NAME line not found in /etc/os-release"));
  }

  fn GetMemInfo() -> Result<u64> {
    struct sysinfo info;

    if (sysinfo(&info) != 0)
      return Err(DracError("sysinfo call failed"));

    const u64 totalRam = info.totalram;
    const u64 memUnit  = info.mem_unit;

    if (memUnit == 0)
      return Err(DracError(DracErrorCode::InternalError, "sysinfo returned mem_unit of zero"));

    if (totalRam > std::numeric_limits<u64>::max() / memUnit)
      return Err(DracError(DracErrorCode::InternalError, "Potential overflow calculating total RAM"));

    return info.totalram * info.mem_unit;
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
      Result<Message> listNamesResult = Message::newMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

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
        return Err(DracError(DracErrorCode::ParseError, "Invalid DBus ListNames reply format: Could not recurse into array"));

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

    Result<Message> msgResult = Message::newMethodCall(activePlayer->c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");

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
    return Err(DracError(DracErrorCode::NotSupported, "DBus support not available"));
  #endif
  }

  fn GetWindowManager() -> Result<String> {
  #if !defined(HAVE_WAYLAND) && !defined(HAVE_XCB)
    return Err(DracError(DracErrorCode::NotSupported, "Wayland or XCB support not available"));
  #else
    if (Result<String> waylandResult = GetWaylandCompositor())
      return *waylandResult;

    if (Result<String> x11Result = GetX11WindowManager())
      return *x11Result;

    return Err(DracError(DracErrorCode::NotFound, "Could not detect window manager (Wayland/X11) or both failed"));
  #endif
  }

  fn GetDesktopEnvironment() -> Result<String> {
    return GetEnv("XDG_CURRENT_DESKTOP")
      .transform([](String xdgDesktop) -> String {
        if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
          xdgDesktop.resize(colon);

        return xdgDesktop;
      })
      .or_else([](const DracError&) -> Result<String> { return GetEnv("DESKTOP_SESSION"); });
  }

  fn GetShell() -> Result<String> {
    using util::types::Pair, util::types::Array, util::types::StringView;

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
    using util::types::CStr;

    constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
    constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

    fn readFirstLine = [&](const String& path) -> Result<String> {
      std::ifstream file(path);
      String        line;

      if (!file)
        return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open DMI product identifier file '{}'", path)));

      if (!std::getline(file, line) || line.empty())
        return Err(DracError(DracErrorCode::ParseError, std::format("DMI product identifier file ('{}') is empty", path)));

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
      DracErrorCode::InternalError,
      std::format(
        "Failed to get host identifier. Primary ('{}'): {}. Fallback ('{}'): {}",
        primaryPath,
        primaryError.message,
        fallbackPath,
        fallbackError.message
      )
    ));
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
      return Err(DracError("Failed to get filesystem stats for '/' (statvfs call failed)"));

    return DiskSpace {
      .usedBytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .totalBytes = stat.f_blocks * stat.f_frsize,
    };
  }
} // namespace os

namespace package {
  using namespace std::string_literals;

  fn CountApk() -> Result<u64> {
    using namespace util::cache;

    const String   pmId      = "apk";
    const fs::path apkDbPath = "/lib/apk/db/installed";
    const String   cacheKey  = "pkg_count_" + pmId;

    std::error_code fsErrCode;

    if (!fs::exists(apkDbPath, fsErrCode)) {
      if (fsErrCode) {
        warn_log("Filesystem error checking for Apk DB at '{}': {}", apkDbPath.string(), fsErrCode.message());
        return Err(DracError(DracErrorCode::IoError, "Filesystem error checking Apk DB: " + fsErrCode.message()));
      }

      return Err(DracError(DracErrorCode::NotFound, std::format("Apk database path '{}' does not exist", apkDbPath.string())));
    }

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [cachedCount, timestamp] = *cachedDataResult;
      std::error_code          modTimeErrCode;
      const fs::file_time_type dbModTime = fs::last_write_time(apkDbPath, modTimeErrCode);

      if (modTimeErrCode) {
        warn_log(
          "Could not get modification time for '{}': {}. Invalidating {} cache.",
          apkDbPath.string(),
          modTimeErrCode.message(),
          pmId
        );
      } else {
        using std::chrono::system_clock, std::chrono::seconds, std::chrono::floor;
        const system_clock::time_point cacheTimePoint = system_clock::time_point(seconds(timestamp));

        if (cacheTimePoint.time_since_epoch() >= dbModTime.time_since_epoch()) {
          return cachedCount;
        }
      }
    } else {
      if (cachedDataResult.error().code != DracErrorCode::NotFound)
        debug_at(cachedDataResult.error());
      debug_log("{} package count cache not found or unreadable.", pmId);
    }

    std::ifstream file(apkDbPath);
    if (!file.is_open())
      return Err(DracError(DracErrorCode::IoError, std::format("Failed to open Apk database file '{}'", apkDbPath.string())));

    String line;

    u64 count = 0;

    try {
      while (std::getline(file, line))
        if (line.empty())
          count++;
    } catch (const std::ios_base::failure& e) {
      return Err(DracError(
        DracErrorCode::IoError,
        std::format("Error reading Apk database file '{}': {}", apkDbPath.string(), e.what())
      ));
    }

    if (file.bad())
      return Err(DracError(DracErrorCode::IoError, std::format("IO error while reading Apk database file '{}'", apkDbPath.string())));

    {
      using std::chrono::duration_cast, std::chrono::system_clock, std::chrono::seconds;

      const i64 timestampEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

      const PkgCountCacheData dataToCache(count, timestampEpochSeconds);

      if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
        debug_at(writeResult.error());
    }

    return count;
  }

  fn CountDpkg() -> Result<u64> {
    return GetCountFromDirectory("Dpkg", fs::current_path().root_path() / "var" / "lib" / "dpkg" / "info", ".list"s);
  }

  fn CountMoss() -> Result<u64> {
    Result<u64> countResult = GetCountFromDb("moss", "/.moss/db/install", "SELECT COUNT(*) FROM meta");

    if (countResult)
      if (*countResult > 0)
        return *countResult - 1;

    return countResult;
  }

  fn CountPacman() -> Result<u64> {
    return GetCountFromDirectory("Pacman", fs::current_path().root_path() / "var" / "lib" / "pacman" / "local", true);
  }

  fn CountRpm() -> Result<u64> {
    return GetCountFromDb("rpm", "/var/lib/rpm/rpmdb.sqlite", "SELECT COUNT(*) FROM Installtid");
  }

  fn CountXbps() -> Result<u64> {
    using util::types::CStr;

    const CStr xbpsDbPath = "/var/db/xbps";

    if (!fs::exists(xbpsDbPath))
      return Err(DracError(DracErrorCode::NotFound, std::format("Xbps database path '{}' does not exist", xbpsDbPath)));

    fs::path plistPath;
    for (const fs::directory_entry& entry : fs::directory_iterator(xbpsDbPath)) {
      const String filename = entry.path().filename().string();
      if (filename.starts_with("pkgdb-") && filename.ends_with(".plist")) {
        plistPath = entry.path();
        break;
      }
    }

    if (plistPath.empty())
      return Err(DracError(DracErrorCode::NotFound, "No Xbps database found"));

    return GetCountFromPlist("xbps", plistPath);
  }
} // namespace package

#endif // __linux__
