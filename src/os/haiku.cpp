#ifdef __HAIKU__

// clang-format off
#include <File.h>                      // For BFile
#include <AppFileInfo.h>               // For BAppFileInfo and version_info
#include <Errors.h>                    // B_OK, strerror, status_t
#include <OS.h>                        // get_system_info, system_info
#include <climits>                     // PATH_MAX
#include <cstring>                     // std::strlen
#include <dbus/dbus-protocol.h>        // DBUS_TYPE_*
#include <dbus/dbus-shared.h>          // DBUS_BUS_SESSION
#include <os/package/PackageDefs.h>    // BPackageKit::BPackageInfoSet
#include <os/package/PackageInfoSet.h> // BPackageKit::BPackageInfo
#include <os/package/PackageRoster.h>  // BPackageKit::BPackageRoster
#include <sys/socket.h>                // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>               // statvfs
#include <utility>                     // std::move

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/helpers.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"
#include "src/wrappers/dbus.hpp"

#include "os.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError, util::error::DracErrorCode;
using util::helpers::GetEnv;

namespace os {
  fn GetOSVersion() -> Result<String, DracError> {
    BFile    file;
    status_t status = file.SetTo("/boot/system/lib/libbe.so", B_READ_ONLY);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, "Error opening /boot/system/lib/libbe.so"));

    BAppFileInfo appInfo;
    status = appInfo.SetTo(&file);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, "Error initializing BAppFileInfo"));

    version_info versionInfo;
    status = appInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, "Error reading version info attribute"));

    String versionShortString = versionInfo.short_info;

    if (versionShortString.empty())
      return Err(DracError(DracErrorCode::InternalError, "Version info short_info is empty"));

    return std::format("Haiku {}", versionShortString);
  }

  fn GetMemInfo() -> Result<u64, DracError> {
    system_info    sysinfo;
    const status_t status = get_system_info(&sysinfo);

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

  fn GetWindowManager() -> Result<String, DracError> { return "app_server"; }

  fn GetDesktopEnvironment() -> Result<String, DracError> { return "Haiku Desktop Environment"; }

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
        DracErrorCode::ApiUnavailable, std::format("gethostname() failed: {} (errno {})", strerror(errno), errno)
      ));

    hostnameBuffer.at(HOST_NAME_MAX) = '\0';

    return String(hostnameBuffer.data(), hostnameBuffer.size());
  }

  fn GetKernelVersion() -> Result<String, DracError> {
    system_info    sysinfo;
    const status_t status = get_system_info(&sysinfo);

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
    BPackageKit::BPackageRoster  roster;
    BPackageKit::BPackageInfoSet packageList;

    const status_t status = roster.GetActivePackages(BPackageKit::B_PACKAGE_INSTALLATION_LOCATION_SYSTEM, packageList);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get active package list"));

    return static_cast<u64>(packageList.CountInfos());
  }
} // namespace os

#endif // __HAIKU__
