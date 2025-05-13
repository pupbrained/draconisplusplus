#ifdef __HAIKU__

// clang-format off
#include <AppFileInfo.h>               // For BAppFileInfo and version_info
#include <Errors.h>                    // B_OK, strerror, status_t
#include <File.h>                      // For BFile
#include <OS.h>                        // get_system_info, system_info
#include <climits>                     // PATH_MAX
#include <cstring>                     // std::strlen
#include <os/package/PackageDefs.h>    // BPackageKit::BPackageInfoSet
#include <os/package/PackageInfoSet.h> // BPackageKit::BPackageInfo
#include <os/package/PackageRoster.h>  // BPackageKit::BPackageRoster
#include <sys/socket.h>                // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>               // statvfs
#include <utility>                     // std::move

#include "Services/PackageCounting.hpp"
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Env.hpp"
#include "Util/Loggin.hpp"
#include "Util/Types.hpp"

#include "OperatingSystem.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError, util::error::DracErrorCode;
using util::helpers::GetEnv;

namespace os {
  fn GetOSVersion() -> Result<String> {
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

  fn GetMemInfo() -> Result<u64> {
    system_info    sysinfo;
    const status_t status = get_system_info(&sysinfo);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, std::format("get_system_info failed: {}", strerror(status))));

    return static_cast<u64>(sysinfo.max_pages) * B_PAGE_SIZE;
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    return Err(DracError(DracErrorCode::NotSupported, "Now playing is not supported on Haiku"));
  }

  fn GetWindowManager() -> Result<String> {
    return "app_server";
  }

  fn GetDesktopEnvironment() -> Result<String> {
    return "Haiku Desktop Environment";
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
    Array<char, HOST_NAME_MAX + 1> hostnameBuffer {};

    if (gethostname(hostnameBuffer.data(), hostnameBuffer.size()) != 0)
      return Err(DracError(
        DracErrorCode::ApiUnavailable, std::format("gethostname() failed: {} (errno {})", strerror(errno), errno)
      ));

    hostnameBuffer.at(HOST_NAME_MAX) = '\0';

    return String(hostnameBuffer.data(), hostnameBuffer.size());
  }

  fn GetKernelVersion() -> Result<String> {
    system_info    sysinfo;
    const status_t status = get_system_info(&sysinfo);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::InternalError, std::format("get_system_info failed: {}", strerror(status))));

    return std::to_string(sysinfo.kernel_version);
  }

  fn GetDiskUsage() -> Result<DiskSpace> {
    struct statvfs stat;

    if (statvfs("/boot", &stat) == -1)
      return Err(DracError::withErrno(std::format("Failed to get filesystem stats for '/boot' (statvfs call failed)")));

    return DiskSpace {
      .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .total_bytes = stat.f_blocks * stat.f_frsize,
    };
  }
} // namespace os

namespace package {
  fn GetHaikuCount() -> Result<u64> {
    BPackageKit::BPackageRoster  roster;
    BPackageKit::BPackageInfoSet packageList;

    const status_t status = roster.GetActivePackages(BPackageKit::B_PACKAGE_INSTALLATION_LOCATION_SYSTEM, packageList);

    if (status != B_OK)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get active package list"));

    return static_cast<u64>(packageList.CountInfos());
  }
} // namespace package

#endif // __HAIKU__
