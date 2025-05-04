#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h> // SQLite::Exception
#include <SQLiteCpp/Statement.h> // SQLite::Statement

#include <chrono>                // std::chrono
#include <filesystem>            // std::filesystem
#include <format>                // std::format
#include <glaze/beve/write.hpp>  // glz::write_beve
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object
#include <system_error>          // std::error_code

#include "src/os/bsd/pkg_count.hpp"
#include "src/os/os.hpp"
#include "src/util/cache.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::u64, util::types::i64, util::types::Result, util::types::Err, util::types::String,
  util::types::StringView, util::types::Exception;

namespace {
  using namespace std::chrono;
  using namespace util::cache;

  using os::bsd::PackageManagerInfo, os::bsd::PkgCountCacheData;

  #ifdef __NetBSD__
  fn GetPackageCountInternalDir(const String& pmId, const fs::path& dirPath) -> Result<u64, DracError> {
    debug_log("Attempting to get {} package count.", pmId);

    std::error_code errc;
    if (!fs::exists(dirPath, errc)) {
      if (errc)
        return Err(DracError(
          DracErrorCode::IoError, std::format("Filesystem error checking {} directory: {}", pmId, errc.message())
        ));

      return Err(
        DracError(DracErrorCode::ApiUnavailable, std::format("{} directory not found: {}", pmId, dirPath.string()))
      );
    }

    if (!fs::is_directory(dirPath, errc)) {
      if (errc)
        return Err(DracError(
          DracErrorCode::IoError, std::format("Filesystem error checking {} path type: {}", pmId, errc.message())
        ));

      warn_log("Expected {} directory at '{}', but it's not a directory.", pmId, dirPath.string());
      return Err(
        DracError(DracErrorCode::IoError, std::format("{} path is not a directory: {}", pmId, dirPath.string()))
      );
    }

    u64 count = 0;

    try {
      const fs::directory_iterator dirIter(dirPath, fs::directory_options::skip_permission_denied, errc);

      if (errc)
        return Err(
          DracError(DracErrorCode::IoError, std::format("Failed to iterate {} directory: {}", pmId, errc.message()))
        );

      for (const fs::directory_entry& entry : dirIter) { count++; }
    } catch (const fs::filesystem_error& e) {
      return Err(DracError(
        DracErrorCode::IoError,
        std::format("Filesystem error iterating {} directory '{}': {}", pmId, dirPath.string(), e.what())
      ));
    } catch (...) {
      return Err(DracError(
        DracErrorCode::Other, std::format("Unknown error iterating {} directory '{}'", pmId, dirPath.string())
      ));
    }

    if (count > 0)
      count--;

    return count;
  }
  #else
  fn GetPackageCountInternalDb(const PackageManagerInfo& pmInfo) -> Result<u64, DracError> {
    const auto& [pmId, dbPath, countQuery] = pmInfo;

    if (Result<PkgCountCacheData, DracError> cachedDataResult = ReadCache<PkgCountCacheData>(pmId)) {
      const auto& [count, timestamp] = *cachedDataResult;
      std::error_code                       errc;
      const std::filesystem::file_time_type dbModTime = fs::last_write_time(dbPath, errc);

      if (errc) {
        warn_log(
          "Could not get modification time for '{}': {}. Invalidating {} cache.", dbPath.string(), errc.message(), pmId
        );
      } else {
        if (const system_clock::time_point cacheTimePoint = system_clock::time_point(seconds(timestamp));
            cacheTimePoint.time_since_epoch() >= dbModTime.time_since_epoch()) {
          debug_log(
            "Using valid {} package count cache (DB file unchanged since {}).",
            pmId,
            std::format("{:%F %T %Z}", floor<seconds>(cacheTimePoint))
          );
          return count;
        }
        debug_log("{} package count cache stale (DB file modified).", pmId);
      }
    } else {
      if (cachedDataResult.error().code != DracErrorCode::NotFound)
        debug_at(cachedDataResult.error());
      debug_log("{} package count cache not found or unreadable.", pmId);
    }

    debug_log("Fetching fresh {} package count from database: {}", pmId, dbPath.string());
    u64 count = 0;

    try {
      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);
      if (SQLite::Statement query(database, countQuery); query.executeStep()) {
        const i64 countInt64 = query.getColumn(0).getInt64();
        if (countInt64 < 0)
          return Err(
            DracError(DracErrorCode::ParseError, std::format("Negative count returned by {} DB COUNT query.", pmId))
          );
        count = static_cast<u64>(countInt64);
      } else {
        return Err(DracError(DracErrorCode::ParseError, std::format("No rows returned by {} DB COUNT query.", pmId)));
      }
    } catch (const SQLite::Exception& e) {
      return Err(DracError(
        DracErrorCode::ApiUnavailable, std::format("SQLite error occurred accessing {} DB: {}", pmId, e.what())
      ));
    } catch (const Exception& e) { return Err(DracError(DracErrorCode::InternalError, e.what())); } catch (...) {
      return Err(DracError(DracErrorCode::Other, std::format("Unknown error occurred accessing {} DB", pmId)));
    }

    const i64               nowEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    const PkgCountCacheData dataToCache     = { .count = count, .timestampEpochSeconds = nowEpochSeconds };

    if (Result<void, DracError> writeResult = WriteCache(pmId, dataToCache); !writeResult)
      error_at(writeResult.error());

    return count;
  }
  #endif
} // namespace

namespace os {
  fn GetPackageCount() -> Result<u64, DracError> {
  #ifdef __NetBSD__
    return GetPackageCountInternalDir("PkgSrc", fs::current_path().root_path() / "usr" / "pkg" / "pkgdb");
  #else
    const PackageManagerInfo pkgInfo = {
      .id         = "pkg_count",
      .dbPath     = "/var/db/pkg/local.sqlite",
      .countQuery = "SELECT COUNT(*) FROM packages",
    };

    if (std::error_code errc; !fs::exists(pkgInfo.dbPath, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for pkg DB at '{}': {}", pkgInfo.dbPath.string(), errc.message());
        return Err(DracError(DracErrorCode::IoError, "Filesystem error checking Nix DB: " + errc.message()));
      }

      return Err(DracError(DracErrorCode::ApiUnavailable, "pkg db not found: " + pkgInfo.dbPath.string()));
    }

    return GetPackageCountInternalDb(pkgInfo);
  #endif
  }
} // namespace os

#endif // __FreeBSD__ || __DragonFly__ || __NetBSD__
