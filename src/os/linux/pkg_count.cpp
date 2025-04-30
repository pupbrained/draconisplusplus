#include "src/os/linux/pkg_count.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <fstream>
#include <glaze/beve/read.hpp>
#include <glaze/beve/write.hpp>
#include <glaze/core/common.hpp>
#include <glaze/core/read.hpp>

#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

using util::error::DraconisError, util::error::DraconisErrorCode;
using util::types::u64, util::types::i64, util::types::Result, util::types::Err, util::types::String,
  util::types::Exception;

namespace {
  namespace fs = std::filesystem;
  using namespace std::chrono;

  struct PkgCountCacheData {
    u64 count {};
    i64 timestamp_epoch_seconds {};

    // NOLINTBEGIN(readability-identifier-naming)
    struct [[maybe_unused]] glaze {
      using T                     = PkgCountCacheData;
      static constexpr auto value = glz::object("count", &T::count, "timestamp", &T::timestamp_epoch_seconds);
    };
    // NOLINTEND(readability-identifier-naming)
  };

  fn GetPkgCountCachePath(const String& pm_id) -> Result<fs::path, DraconisError> {
    std::error_code errc;
    const fs::path  cacheDir = fs::temp_directory_path(errc);

    if (errc)
      return Err(DraconisError(DraconisErrorCode::IoError, "Failed to get temp directory: " + errc.message()));

    if (pm_id.empty() ||
        pm_id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != String::npos)
      return Err(DraconisError(DraconisErrorCode::ParseError, "Invalid package manager ID for cache path: " + pm_id));

    return cacheDir / (pm_id + "_pkg_count_cache.beve");
  }

  fn ReadPkgCountCache(const String& pm_id) -> Result<PkgCountCacheData, DraconisError> {
    Result<fs::path, DraconisError> cachePathResult = GetPkgCountCachePath(pm_id);

    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;

    if (!fs::exists(cachePath))
      return Err(DraconisError(DraconisErrorCode::NotFound, "Cache file not found: " + cachePath.string()));

    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open())
      return Err(
        DraconisError(DraconisErrorCode::IoError, "Failed to open cache file for reading: " + cachePath.string())
      );

    // Update log message
    debug_log("Reading {} package count from cache file: {}", pm_id, cachePath.string());

    try {
      // Read the entire binary content
      // Using std::string buffer is fine, it can hold arbitrary binary data
      const String content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      ifs.close(); // Close the file stream after reading

      if (content.empty()) {
        return Err(DraconisError(DraconisErrorCode::ParseError, "BEVE cache file is empty: " + cachePath.string()));
      }

      PkgCountCacheData  result;
      const glz::context ctx {};

      if (auto glazeResult = glz::read_beve(result, content); glazeResult.ec != glz::error_code::none)
        return Err(DraconisError(
          DraconisErrorCode::ParseError,
          std::format(
            "BEVE parse error reading cache (code {}): {}", static_cast<int>(glazeResult.ec), cachePath.string()
          )
        ));

      debug_log("Successfully read {} package count from BEVE cache file.", pm_id);
      return result;
    } catch (const std::ios_base::failure& e) {
      return Err(DraconisError(
        DraconisErrorCode::IoError,
        std::format("Filesystem error reading cache file {}: {}", cachePath.string(), e.what())
      ));
    } catch (const Exception& e) {
      return Err(
        DraconisError(DraconisErrorCode::InternalError, std::format("Error reading package count cache: {}", e.what()))
      );
    }
  }

  // Modified to take pm_id and PkgCountCacheData
  fn WritePkgCountCache(const String& pm_id, const PkgCountCacheData& data) -> Result<void, DraconisError> {
    using util::types::isize;

    Result<fs::path, DraconisError> cachePathResult = GetPkgCountCachePath(pm_id);

    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;
    fs::path        tempPath  = cachePath;
    tempPath += ".tmp";

    debug_log("Writing {} package count to BEVE cache file: {}", pm_id, cachePath.string());

    try {
      String binaryBuffer;

      PkgCountCacheData mutableData = data;

      if (auto glazeErr = glz::write_beve(mutableData, binaryBuffer); glazeErr) {
        return Err(DraconisError(
          DraconisErrorCode::ParseError,
          std::format("BEVE serialization error writing cache (code {})", static_cast<int>(glazeErr.ec))
        ));
      }

      {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
          return Err(DraconisError(DraconisErrorCode::IoError, "Failed to open temp cache file: " + tempPath.string()));
        }

        ofs.write(binaryBuffer.data(), static_cast<isize>(binaryBuffer.size()));

        if (!ofs) {
          std::error_code removeEc;
          fs::remove(tempPath, removeEc);
          return Err(
            DraconisError(DraconisErrorCode::IoError, "Failed to write to temp cache file: " + tempPath.string())
          );
        }
      }

      // Atomically replace the old cache file with the new one
      std::error_code errc;
      fs::rename(tempPath, cachePath, errc);
      if (errc) {
        fs::remove(tempPath, errc); // Clean up temp file on failure (ignore error)
        return Err(DraconisError(
          DraconisErrorCode::IoError,
          std::format("Failed to replace cache file '{}': {}", cachePath.string(), errc.message())
        ));
      }

      debug_log("Successfully wrote {} package count to BEVE cache file.", pm_id);
      return {};
    } catch (const std::ios_base::failure& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DraconisError(
        DraconisErrorCode::IoError,
        std::format("Filesystem error writing cache file {}: {}", tempPath.string(), e.what())
      ));
    } catch (const Exception& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(
        DraconisError(DraconisErrorCode::InternalError, std::format("Error writing package count cache: {}", e.what()))
      );
    } catch (...) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(
        DraconisError(DraconisErrorCode::Other, std::format("Unknown error writing cache file: {}", tempPath.string()))
      );
    }
  }

  fn GetPackageCountInternal(const os::linux::PackageManagerInfo& pmInfo) -> Result<u64, DraconisError> {
    // Use info from the struct
    const fs::path& dbPath   = pmInfo.db_path;
    const String&   pmId     = pmInfo.id;
    const String&   queryStr = pmInfo.count_query;

    // Try reading from cache using pm_id
    if (Result<PkgCountCacheData, DraconisError> cachedDataResult = ReadPkgCountCache(pmId)) {
      const auto& [count, timestamp] = *cachedDataResult;
      std::error_code                       errc;
      const std::filesystem::file_time_type dbModTime = fs::last_write_time(dbPath, errc);

      if (errc) {
        warn_log(
          "Could not get modification time for '{}': {}. Invalidating {} cache.", dbPath.string(), errc.message(), pmId
        );
      } else {
        if (const auto cacheTimePoint = system_clock::time_point(seconds(timestamp));
            cacheTimePoint.time_since_epoch() >= dbModTime.time_since_epoch()) {
          // Use cacheTimePoint for logging as well
          debug_log(
            "Using valid {} package count cache (DB file unchanged since {}). Count: {}",
            pmId,
            std::format("{:%F %T %Z}", floor<seconds>(cacheTimePoint)), // Format the time_point
            count
          );
          return count;
        }
        debug_log("{} package count cache stale (DB file modified).", pmId);
      }
    } else {
      if (cachedDataResult.error().code != DraconisErrorCode::NotFound)
        debug_at(cachedDataResult.error());
      debug_log("{} package count cache not found or unreadable.", pmId);
    }

    debug_log("Fetching fresh {} package count from database: {}", pmId, dbPath.string());
    u64 count = 0;

    try {
      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);
      if (SQLite::Statement query(database, queryStr); query.executeStep()) {
        const i64 countInt64 = query.getColumn(0).getInt64();
        if (countInt64 < 0)
          return Err(DraconisError(
            DraconisErrorCode::ParseError, std::format("Negative count returned by {} DB COUNT query.", pmId)
          ));
        count = static_cast<u64>(countInt64);
      } else {
        return Err(
          DraconisError(DraconisErrorCode::ParseError, std::format("No rows returned by {} DB COUNT query.", pmId))
        );
      }
    } catch (const SQLite::Exception& e) {
      return Err(DraconisError(
        DraconisErrorCode::ApiUnavailable, std::format("SQLite error occurred accessing {} DB: {}", pmId, e.what())
      ));
    } catch (const Exception& e) {
      return Err(DraconisError(DraconisErrorCode::InternalError, e.what()));
    } catch (...) {
      return Err(DraconisError(DraconisErrorCode::Other, std::format("Unknown error occurred accessing {} DB", pmId)));
    }

    const i64               nowEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    const PkgCountCacheData dataToCache     = { .count = count, .timestamp_epoch_seconds = nowEpochSeconds };

    if (Result<void, DraconisError> writeResult = WritePkgCountCache(pmId, dataToCache); !writeResult) {
      warn_at(writeResult.error());
      warn_log("Failed to write {} package count to cache.", pmId);
    }

    debug_log("Fetched fresh {} package count: {}", pmId, count);
    return count;
  }
} // namespace

namespace os::linux {
  fn GetMossPackageCount() -> Result<u64, DraconisError> {
    debug_log("Attempting to get Moss package count.");

    const PackageManagerInfo mossInfo = {
      .id          = "moss",
      .db_path     = "/.moss/db/install",
      .count_query = "SELECT COUNT(*) FROM meta",
    };

    if (std::error_code errc; !fs::exists(mossInfo.db_path, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for Moss DB at '{}': {}", mossInfo.db_path.string(), errc.message());
        return Err(DraconisError(DraconisErrorCode::IoError, "Filesystem error checking Moss DB: " + errc.message()));
      }

      debug_log("Moss database not found at '{}'. Assuming 0 Moss packages.", mossInfo.db_path.string());

      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Moss db not found: " + mossInfo.db_path.string()));
    }

    debug_log("Moss database found at '{}'. Proceeding with count.", mossInfo.db_path.string());

    return GetPackageCountInternal(mossInfo);
  }

  fn GetNixPackageCount() -> Result<u64, DraconisError> {
    debug_log("Attempting to get Nix package count.");
    const PackageManagerInfo nixInfo = {
      .id          = "nix",
      .db_path     = "/nix/var/nix/db/db.sqlite",
      .count_query = "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL",
    };

    if (std::error_code errc; !fs::exists(nixInfo.db_path, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for Nix DB at '{}': {}", nixInfo.db_path.string(), errc.message());
        return Err(DraconisError(DraconisErrorCode::IoError, "Filesystem error checking Nix DB: " + errc.message()));
      }

      debug_log("Nix database not found at '{}'. Assuming 0 Nix packages.", nixInfo.db_path.string());

      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Nix db not found: " + nixInfo.db_path.string()));
    }

    debug_log("Nix database found at '{}'. Proceeding with count.", nixInfo.db_path.string());

    return GetPackageCountInternal(nixInfo);
  }

  fn GetTotalPackageCount() -> Result<u64, DraconisError> {
    debug_log("Attempting to get total package count from all package managers.");

    const PackageManagerInfo mossInfo = {
      .id          = "moss",
      .db_path     = "/.moss/db/install",
      .count_query = "SELECT COUNT(*) FROM meta",
    };

    const PackageManagerInfo nixInfo = {
      .id          = "nix",
      .db_path     = "/nix/var/nix/db/db.sqlite",
      .count_query = "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL",
    };

    u64 totalCount = 0;

    if (Result<u64, DraconisError> mossCountResult = GetMossPackageCount(); mossCountResult) {
      // `moss list installed` returns 1 less than the db count,
      // so we subtract 1 for consistency.
      totalCount += (*mossCountResult - 1);
    } else {
      debug_at(mossCountResult.error());
    }

    if (Result<u64, DraconisError> nixCountResult = GetNixPackageCount(); nixCountResult) {
      totalCount += *nixCountResult;
    } else {
      debug_at(nixCountResult.error());
    }

    return totalCount;
  }
} // namespace os::linux