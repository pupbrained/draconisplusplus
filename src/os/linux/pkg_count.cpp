#ifdef __linux__

// clang-format off
#include "src/os/linux/pkg_count.hpp"

#include <SQLiteCpp/Database.h>   // SQLite::{Database, OPEN_READONLY}
#include <SQLiteCpp/Exception.h>  // SQLite::Exception
#include <SQLiteCpp/Statement.h>  // SQLite::Statement
#include <chrono>                 // std::chrono::{duration_cast, seconds, system_clock}
#include <filesystem>             // std::filesystem::{current_path, directory_entry, directory_iterator, etc.}
#include <format>                 // std::format
#include <fstream>                // std::{ifstream, ofstream}
#include <future>                 // std::{async, launch}
#include <ios>                    // std::ios::{binary, trunc}, std::ios_base
#include <iterator>               // std::istreambuf_iterator
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve
#include <glaze/core/context.hpp> // glz::{context, error_code, error_ctx}
#include <system_error>           // std::error_code

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"
// clang-format on

using util::error::DracError, util::error::DracErrorCode;
using util::types::u64, util::types::i64, util::types::Result, util::types::Err, util::types::String,
  util::types::StringView, util::types::Exception;

namespace {
  namespace fs = std::filesystem;
  using namespace std::chrono;
  using os::linux::PkgCountCacheData, os::linux::PackageManagerInfo;

  constexpr StringView ALLOWED_PMID_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

  fn GetPkgCountCachePath(const String& pmId) -> Result<fs::path, DracError> {
    std::error_code errc;
    const fs::path  cacheDir = fs::temp_directory_path(errc);

    if (errc)
      return Err(DracError(DracErrorCode::IoError, "Failed to get temp directory: " + errc.message()));

    if (pmId.empty() || pmId.find_first_not_of(ALLOWED_PMID_CHARS) != String::npos)
      return Err(DracError(DracErrorCode::ParseError, "Invalid package manager ID for cache path: " + pmId));

    return cacheDir / (pmId + "_pkg_count_cache.beve");
  }

  fn ReadPkgCountCache(const String& pmId) -> Result<PkgCountCacheData, DracError> {
    Result<fs::path, DracError> cachePathResult = GetPkgCountCachePath(pmId);

    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;

    if (!fs::exists(cachePath))
      return Err(DracError(DracErrorCode::NotFound, "Cache file not found: " + cachePath.string()));

    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open())
      return Err(DracError(DracErrorCode::IoError, "Failed to open cache file for reading: " + cachePath.string()));

    try {
      const String content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      ifs.close();

      if (content.empty())
        return Err(DracError(DracErrorCode::ParseError, "BEVE cache file is empty: " + cachePath.string()));

      PkgCountCacheData  result;
      const glz::context ctx {};

      if (glz::error_ctx glazeResult = glz::read_beve(result, content); glazeResult.ec != glz::error_code::none)
        return Err(DracError(
          DracErrorCode::ParseError,
          std::format(
            "BEVE parse error reading cache (code {}): {}", static_cast<int>(glazeResult.ec), cachePath.string()
          )
        ));

      return result;
    } catch (const std::ios_base::failure& e) {
      return Err(DracError(
        DracErrorCode::IoError, std::format("Filesystem error reading cache file {}: {}", cachePath.string(), e.what())
      ));
    } catch (const Exception& e) {
      return Err(DracError(DracErrorCode::InternalError, std::format("Error reading package count cache: {}", e.what()))
      );
    }
  }

  fn WritePkgCountCache(const String& pmId, const PkgCountCacheData& data) -> Result<void, DracError> {
    using util::types::isize;

    Result<fs::path, DracError> cachePathResult = GetPkgCountCachePath(pmId);

    if (!cachePathResult)
      return Err(cachePathResult.error());

    const fs::path& cachePath = *cachePathResult;
    fs::path        tempPath  = cachePath;
    tempPath += ".tmp";

    try {
      String binaryBuffer;

      PkgCountCacheData mutableData = data;

      if (glz::error_ctx glazeErr = glz::write_beve(mutableData, binaryBuffer); glazeErr)
        return Err(DracError(
          DracErrorCode::ParseError,
          std::format("BEVE serialization error writing cache (code {})", static_cast<int>(glazeErr.ec))
        ));

      {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
          return Err(DracError(DracErrorCode::IoError, "Failed to open temp cache file: " + tempPath.string()));

        ofs.write(binaryBuffer.data(), static_cast<isize>(binaryBuffer.size()));

        if (!ofs) {
          std::error_code removeEc;
          fs::remove(tempPath, removeEc);
          return Err(DracError(DracErrorCode::IoError, "Failed to write to temp cache file: " + tempPath.string()));
        }
      }

      std::error_code errc;
      fs::rename(tempPath, cachePath, errc);
      if (errc) {
        fs::remove(tempPath, errc);
        return Err(DracError(
          DracErrorCode::IoError,
          std::format("Failed to replace cache file '{}': {}", cachePath.string(), errc.message())
        ));
      }

      return {};
    } catch (const std::ios_base::failure& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(
        DracErrorCode::IoError, std::format("Filesystem error writing cache file {}: {}", tempPath.string(), e.what())
      ));
    } catch (const Exception& e) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(DracErrorCode::InternalError, std::format("Error writing package count cache: {}", e.what()))
      );
    } catch (...) {
      std::error_code removeEc;
      fs::remove(tempPath, removeEc);
      return Err(DracError(DracErrorCode::Other, std::format("Unknown error writing cache file: {}", tempPath.string()))
      );
    }
  }

  fn GetPackageCountInternalDb(const PackageManagerInfo& pmInfo) -> Result<u64, DracError> {
    const auto& [pmId, dbPath, countQuery] = pmInfo;

    if (Result<PkgCountCacheData, DracError> cachedDataResult = ReadPkgCountCache(pmId)) {
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

    if (Result<void, DracError> writeResult = WritePkgCountCache(pmId, dataToCache); !writeResult)
      error_at(writeResult.error());

    return count;
  }

  fn GetPackageCountInternalDir(
    const String&   pmId,
    const fs::path& dirPath,
    const String&   file_extension_filter = "",
    const bool      subtract_one          = false
  ) -> Result<u64, DracError> {
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

      for (const fs::directory_entry& entry : dirIter) {
        if (!file_extension_filter.empty()) {
          if (std::error_code fileErrc; !entry.is_regular_file(fileErrc) || fileErrc) {
            if (fileErrc)
              warn_log(
                "Error checking file status in {} directory for '{}': {}",
                pmId,
                entry.path().string(),
                fileErrc.message()
              );

            continue;
          }

          if (entry.path().extension().string() == file_extension_filter)
            count++;
        } else {
          count++;
        }
      }
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

    if (subtract_one && count > 0)
      count--;

    return count;
  }
} // namespace

namespace os::linux {
  fn GetDpkgPackageCount() -> Result<u64, DracError> {
    return GetPackageCountInternalDir(
      "Dpkg", fs::current_path().root_path() / "var" / "lib" / "dpkg" / "info", String(".list")
    );
  }

  fn GetMossPackageCount() -> Result<u64, DracError> {
    debug_log("Attempting to get Moss package count.");

    const PackageManagerInfo mossInfo = {
      .id         = "moss",
      .dbPath     = "/.moss/db/install",
      .countQuery = "SELECT COUNT(*) FROM meta",
    };

    if (std::error_code errc; !fs::exists(mossInfo.dbPath, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for Moss DB at '{}': {}", mossInfo.dbPath.string(), errc.message());
        return Err(DracError(DracErrorCode::IoError, "Filesystem error checking Moss DB: " + errc.message()));
      }

      return Err(DracError(DracErrorCode::ApiUnavailable, "Moss db not found: " + mossInfo.dbPath.string()));
    }

    Result<u64, DracError> countResult = GetPackageCountInternalDb(mossInfo);

    if (!countResult) {
      if (countResult.error().code != DracErrorCode::ParseError)
        debug_at(countResult.error());

      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to get package count from Moss DB"));
    }

    return *countResult - 1;
  }

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

  fn GetPacmanPackageCount() -> Result<u64, DracError> {
    return GetPackageCountInternalDir(
      "Pacman", fs::current_path().root_path() / "var" / "lib" / "pacman" / "local", "", true
    );
  }

  fn GetTotalPackageCount() -> Result<u64, DracError> {
    using util::types::Array, util::types::Future;

    Array<Future<Result<u64, DracError>>, 4> futures = {
      std::async(std::launch::async, GetDpkgPackageCount),
      std::async(std::launch::async, GetMossPackageCount),
      std::async(std::launch::async, GetNixPackageCount),
      std::async(std::launch::async, GetPacmanPackageCount),
    };

    u64 totalCount = 0;

    for (Future<Result<u64, DracError>>& fut : futures) try {
        if (Result<u64, DracError> result = fut.get()) {
          totalCount += *result;
        } else {
          if (result.error().code != DracErrorCode::ApiUnavailable) {
            error_at(result.error());
          } else {
            debug_at(result.error());
          }
        }
      } catch (const Exception& e) {
        error_log("Caught exception while getting package count future: {}", e.what());
      } catch (...) { error_log("Caught unknown exception while getting package count future."); }

    return totalCount;
  }
} // namespace os::linux

#endif // __linux__
