#ifdef __serenity__

// clang-format off
#include <format>                 // std::format
#include <fstream>                // std::ifstream
#include <glaze/core/common.hpp>  // glz::object
#include <glaze/core/context.hpp> // glz::{error_ctx, error_code}
#include <glaze/core/meta.hpp>    // glz::detail::Object
#include <glaze/core/read.hpp>    // glz::read
#include <glaze/core/reflect.hpp> // glz::format_error
#include <glaze/json/read.hpp>    // glz::read<glaze_opts>
#include <iterator>               // std::istreambuf_iterator
#include <pwd.h>                  // getpwuid, passwd
#include <string>                 // std::string (String)
#include <sys/statvfs.h>          // statvfs
#include <sys/types.h>            // uid_t
#include <sys/utsname.h>          // utsname, uname
#include <unistd.h>               // getuid, gethostname
#include <unordered_set>          // std::unordered_set

#include "Services/PackageCounting.hpp"
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Env.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "OperatingSystem.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError, util::error::DracErrorCode;
using util::helpers::GetEnv;

namespace {
  using glz::opts, glz::detail::Object, glz::object;

  constexpr opts glaze_opts = { .error_on_unknown_keys = false };

  struct MemStatData {
    u64 physical_allocated = 0;
    u64 physical_available = 0;

    // NOLINTBEGIN(readability-identifier-naming)
    struct glaze {
      using T = MemStatData;
      static constexpr Object value =
        object("physical_allocated", &T::physical_allocated, "physical_available", &T::physical_available);
    };
    // NOLINTEND(readability-identifier-naming)
  };

  fn CountUniquePackages(const String& dbPath) -> Result<u64> {
    std::ifstream dbFile(dbPath);

    if (!dbFile.is_open())
      return Err(DracError(DracErrorCode::NotFound, std::format("Failed to open file: {}", dbPath)));

    std::unordered_set<String> uniquePackages;
    String                     line;

    while (std::getline(dbFile, line))
      if (line.starts_with("manual ") || line.starts_with("auto "))
        uniquePackages.insert(line);

    return uniquePackages.size();
  }
} // namespace

namespace os {
  fn GetOSVersion() -> Result<String> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError("uname call failed for OS Version"));

    return uts.sysname;
  }

  fn GetMemInfo() -> Result<u64> {
    CStr          path = "/sys/kernel/memstat";
    std::ifstream file(path);

    if (!file)
      return Err(DracError(DracErrorCode::NotFound, std::format("Could not open {}", path)));

    String buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (buffer.empty())
      return Err(DracError(DracErrorCode::IoError, std::format("File is empty: {}", path)));

    MemStatData data;

    glz::error_ctx error_context = glz::read<glaze_opts>(data, buffer);

    if (error_context)
      return Err(DracError(
        DracErrorCode::ParseError,
        std::format("Failed to parse JSON from {}: {}", path, glz::format_error(error_context, buffer))
      ));

    if (data.physical_allocated > std::numeric_limits<u64>::max() - data.physical_available)
      return Err(DracError(DracErrorCode::InternalError, "Memory size overflow during calculation"));

    return (data.physical_allocated + data.physical_available) * PAGE_SIZE;
  }

  fn GetNowPlaying() -> Result<MediaInfo> {
    return Err(DracError(DracErrorCode::NotSupported, "Now playing is not supported on SerenityOS"));
  }

  fn GetWindowManager() -> Result<String> {
    return "WindowManager";
  }

  fn GetDesktopEnvironment() -> Result<String> {
    return "SerenityOS Desktop";
  }

  fn GetShell() -> Result<String> {
    uid_t   userId = getuid();
    passwd* pw     = getpwuid(userId);

    if (pw == nullptr)
      return Err(DracError(DracErrorCode::NotFound, std::format("User ID {} not found in /etc/passwd", userId)));

    if (pw->pw_shell == nullptr || *(pw->pw_shell) == '\0')
      return Err(DracError(
        DracErrorCode::NotFound, std::format("User shell entry is empty in /etc/passwd for user ID {}", userId)
      ));

    String shell = pw->pw_shell;

    if (shell.starts_with("/bin/"))
      shell = shell.substr(5);

    return shell;
  }

  fn GetHost() -> Result<String> {
    Array<char, HOST_NAME_MAX> hostname_buffer;

    if (gethostname(hostname_buffer.data(), hostname_buffer.size()) != 0)
      return Err(DracError("gethostname() failed: {}"));

    return String(hostname_buffer.data());
  }

  fn GetKernelVersion() -> Result<String> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DracError("uname call failed for Kernel Version"));

    return uts.release;
  }

  fn GetDiskUsage() -> Result<DiskSpace> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DracError("statvfs call failed for '/'"));

    const u64 totalBytes = static_cast<u64>(stat.f_blocks) * stat.f_frsize;
    const u64 freeBytes  = static_cast<u64>(stat.f_bfree) * stat.f_frsize;
    const u64 usedBytes  = totalBytes - freeBytes;

    return DiskSpace { usedBytes, totalBytes };
  }
} // namespace os

namespace package {
  fn GetSerenityCount() -> Result<u64> {
    return CountUniquePackages("/usr/Ports/installed.db");
  }
} // namespace package

#endif // __serenity__
