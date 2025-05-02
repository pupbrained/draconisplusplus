#ifndef _WIN32
  #include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
  #include <SQLiteCpp/Exception.h> // SQLite::Exception
  #include <SQLiteCpp/Statement.h> // SQLite::Statement
#endif

#include <chrono>                // std::chrono
#include <filesystem>            // std::filesystem
#include <format>                // std::format
#include <fstream>               // std::{ifstream, ofstream}
#include <glaze/beve/write.hpp>  // glz::write_beve
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object
#include <iterator>              // std::istreambuf_iterator
#include <system_error>          // std::error_code

#include "src/util/cache.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/helpers.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

#include "os.hpp"

using util::error::DracError, util::error::DracErrorCode;
using util::types::u64, util::types::i64, util::types::String, util::types::StringView, util::types::Result,
  util::types::Err, util::types::Exception;

namespace fs = std::filesystem;

namespace {
  using namespace std::chrono;
  using namespace util::cache;

#ifndef _WIN32
  struct PackageManagerInfo {
    String   id;
    fs::path dbPath;
    String   countQuery;
  };
#endif

  struct PkgCountCacheData {
    u64 count {};
    i64 timestampEpochSeconds {};

    // NOLINTBEGIN(readability-identifier-naming)
    struct [[maybe_unused]] glaze {
      using T = PkgCountCacheData;

      static constexpr glz::detail::Object value =
        glz::object("count", &T::count, "timestamp", &T::timestampEpochSeconds);
    };
    // NOLINTEND(readability-identifier-naming)
  };

#ifndef _WIN32
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

#ifndef _WIN32
  fn GetNixPackageCount() -> Result<u64, DracError> {
    debug_log("Attempting to get Nix package count.");

    const PackageManagerInfo nixInfo = {
      .id         = "nix",
      .dbPath     = "/nix/var/nix/db/db.sqlite",
      .countQuery = "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL",
    };

    if (std::error_code errc; !fs::exists(nixInfo.dbPath, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for Nix DB at '{}': {}", nixInfo.dbPath.string(), errc.message());
        return Err(DracError(DracErrorCode::IoError, "Filesystem error checking Nix DB: " + errc.message()));
      }

      return Err(DracError(DracErrorCode::ApiUnavailable, "Nix db not found: " + nixInfo.dbPath.string()));
    }

    return GetPackageCountInternalDb(nixInfo);
  }
#endif

  fn GetCargoPackageCount() -> Result<u64, DracError> {
    using util::helpers::GetEnv;

    fs::path cargoPath {};

    if (const Result<String, DracError> cargoHome = GetEnv("CARGO_HOME"))
      cargoPath = fs::path(*cargoHome) / "bin";
    else if (const Result<String, DracError> homeDir = GetEnv("HOME"))
      cargoPath = fs::path(*homeDir) / ".cargo" / "bin";

    if (cargoPath.empty() || !fs::exists(cargoPath))
      return Err(DracError(DracErrorCode::NotFound, "Could not find cargo directory"));

    u64 count = 0;

    for (const fs::directory_entry& entry : fs::directory_iterator(cargoPath))
      if (entry.is_regular_file())
        ++count;

    debug_log("Found {} packages in cargo directory: {}", count, cargoPath.string());

    return count;
  }
} // namespace

namespace os::shared {
  fn GetPackageCount() -> Result<u64, DracError> {
    u64 count = 0;

#ifndef _WIN32
    if (const Result<u64, DracError> pkgCount = GetNixPackageCount())
      count += *pkgCount;
    else
      debug_at(pkgCount.error());
#endif

    if (const Result<u64, DracError> pkgCount = GetCargoPackageCount())
      count += *pkgCount;
    else
      debug_at(pkgCount.error());

    return count;
  }
} // namespace os::shared
