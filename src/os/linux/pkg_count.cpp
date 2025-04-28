#include "src/os/linux/pkg_count.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <fstream>
#include <glaze/core/common.hpp>
#include <glaze/core/read.hpp>
#include <glaze/core/reflect.hpp>
#include <glaze/json/write.hpp>

#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

using util::error::DraconisError, util::error::DraconisErrorCode;
using util::types::u64, util::types::i64, util::types::Result, util::types::Err, util::types::String,
  util::types::Exception;

namespace {
  namespace fs = std::filesystem;
  using namespace std::chrono;

  struct NixPkgCacheData {
    u64                      count {};
    system_clock::time_point timestamp;

    // NOLINTBEGIN(readability-identifier-naming) - Needs to specifically use `glaze`
    struct [[maybe_unused]] glaze {
      using T                     = NixPkgCacheData;
      static constexpr auto value = glz::object("count", &T::count, "timestamp", [](auto& self) -> auto& {
        thread_local auto epoch_seconds = duration_cast<seconds>(self.timestamp.time_since_epoch()).count();
        return epoch_seconds;
      });
    };
    // NOLINTEND(readability-identifier-naming)
  };

  fn GetPkgCountCachePath() -> Result<fs::path, DraconisError> {
    std::error_code errc;
    const fs::path  cacheDir = fs::temp_directory_path(errc);
    if (errc) {
      return Err(DraconisError(DraconisErrorCode::IoError, "Failed to get temp directory: " + errc.message()));
    }
    return cacheDir / "nix_pkg_count_cache.json";
  }

  fn ReadPkgCountCache() -> Result<NixPkgCacheData, DraconisError> {
    auto cachePathResult = GetPkgCountCachePath();
    if (!cachePathResult) {
      return Err(cachePathResult.error());
    }
    const fs::path& cachePath = *cachePathResult;

    if (!fs::exists(cachePath)) {
      return Err(DraconisError(DraconisErrorCode::NotFound, "Cache file not found: " + cachePath.string()));
    }

    std::ifstream ifs(cachePath, std::ios::binary);
    if (!ifs.is_open()) {
      return Err(
        DraconisError(DraconisErrorCode::IoError, "Failed to open cache file for reading: " + cachePath.string())
      );
    }

    debug_log("Reading Nix package count from cache file: {}", cachePath.string());

    try {
      const String    content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      NixPkgCacheData result;
      glz::context    ctx {};

      if (auto glazeResult = glz::read<glz::opts { .error_on_unknown_keys = false }>(result, content, ctx);
          glazeResult.ec != glz::error_code::none) {
        return Err(DraconisError(
          DraconisErrorCode::ParseError,
          std::format("JSON parse error reading cache: {}", glz::format_error(glazeResult, content))
        ));
      }

      if (size_t tsPos = content.find("\"timestamp\""); tsPos != String::npos) {
        size_t colonPos = content.find(':', tsPos);
        if (size_t valueStart = content.find_first_of("0123456789", colonPos); valueStart != String::npos) {
          long long timestampSeconds = 0;
          char*     endPtr           = nullptr;
          timestampSeconds           = std::strtoll(content.c_str() + valueStart, &endPtr, 10);
          result.timestamp           = system_clock::time_point(seconds(timestampSeconds));
        } else {
          return Err(DraconisError(DraconisErrorCode::ParseError, "Could not parse timestamp value from cache JSON."));
        }
      } else {
        return Err(DraconisError(DraconisErrorCode::ParseError, "Timestamp field not found in cache JSON."));
      }

      debug_log("Successfully read package count from cache file.");
      return result;
    } catch (const Exception& e) {
      return Err(
        DraconisError(DraconisErrorCode::InternalError, std::format("Error reading package count cache: {}", e.what()))
      );
    }
  }

  fn WritePkgCountCache(const NixPkgCacheData& data) -> Result<void, DraconisError> {
    auto cachePathResult = GetPkgCountCachePath();
    if (!cachePathResult) {
      return Err(cachePathResult.error());
    }
    const fs::path& cachePath = *cachePathResult;
    fs::path        tempPath  = cachePath;
    tempPath += ".tmp";

    debug_log("Writing Nix package count to cache file: {}", cachePath.string());

    try {
      {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
          return Err(DraconisError(DraconisErrorCode::IoError, "Failed to open temp cache file: " + tempPath.string()));
        }

        String jsonStr;

        NixPkgCacheData mutableData = data;

        if (auto glazeErr = glz::write_json(mutableData, jsonStr); glazeErr.ec != glz::error_code::none) {
          return Err(DraconisError(
            DraconisErrorCode::ParseError,
            std::format("JSON serialization error writing cache: {}", glz::format_error(glazeErr, jsonStr))
          ));
        }

        ofs << jsonStr;
        if (!ofs) {
          return Err(DraconisError(DraconisErrorCode::IoError, "Failed to write to temp cache file"));
        }
      }

      std::error_code errc;
      fs::rename(tempPath, cachePath, errc);
      if (errc) {
        fs::remove(tempPath);
        return Err(DraconisError(
          DraconisErrorCode::IoError,
          std::format("Failed to replace cache file '{}': {}", cachePath.string(), errc.message())
        ));
      }

      debug_log("Successfully wrote package count to cache file.");
      return {};
    } catch (const Exception& e) {
      fs::remove(tempPath);
      return Err(DraconisError(
        DraconisErrorCode::InternalError, std::format("File operation error writing package count cache: {}", e.what())
      ));
    }
  }

} // namespace

fn os::linux::GetNixPackageCount() -> Result<u64, DraconisError> {
  const fs::path nixDbPath = "/nix/var/nix/db/db.sqlite";

  if (Result<NixPkgCacheData, DraconisError> cachedDataResult = ReadPkgCountCache()) {
    const auto& [count, timestamp] = *cachedDataResult;

    std::error_code                 errc;
    std::filesystem::file_time_type dbModTime = fs::last_write_time(nixDbPath, errc);

    if (errc) {
      warn_log("Could not get modification time for '{}': {}. Invalidating cache.", nixDbPath.string(), errc.message());
    } else {
      if (timestamp.time_since_epoch() >= dbModTime.time_since_epoch()) {
        debug_log(
          "Using valid Nix package count cache (DB file unchanged since {}). Count: {}",
          std::format("{:%F %T %Z}", floor<seconds>(timestamp)),
          count
        );
        return count;
      }
      debug_log("Nix package count cache stale (DB file modified).");
    }
  } else {
    if (cachedDataResult.error().code != DraconisErrorCode::NotFound)
      debug_at(cachedDataResult.error());
    debug_log("Nix package count cache not found or unreadable.");
  }

  debug_log("Fetching fresh Nix package count from database: {}", nixDbPath.string());
  u64 count = 0;

  try {
    const SQLite::Database database("/nix/var/nix/db/db.sqlite", SQLite::OPEN_READONLY);

    if (SQLite::Statement query(database, "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL");
        query.executeStep()) {
      const i64 countInt64 = query.getColumn(0).getInt64();
      if (countInt64 < 0)
        return Err(DraconisError(DraconisErrorCode::ParseError, "Negative count returned by Nix DB COUNT(*) query."));
      count = static_cast<u64>(countInt64);
    } else {
      return Err(DraconisError(DraconisErrorCode::ParseError, "No rows returned by Nix DB COUNT(*) query."));
    }
  } catch (const SQLite::Exception& e) {
    return Err(DraconisError(
      DraconisErrorCode::ApiUnavailable, std::format("SQLite error occurred accessing Nix DB: {}", e.what())
    ));
  } catch (const Exception& e) { return Err(DraconisError(DraconisErrorCode::InternalError, e.what())); } catch (...) {
    return Err(DraconisError(DraconisErrorCode::Other, "Unknown error occurred accessing Nix DB"));
  }

  const NixPkgCacheData dataToCache = { .count = count, .timestamp = system_clock::now() };
  if (Result<void, DraconisError> writeResult = WritePkgCountCache(dataToCache); !writeResult) {
    warn_at(writeResult.error());
    warn_log("Failed to write Nix package count to cache.");
  }

  debug_log("Fetched fresh Nix package count: {}", count);
  return count;
}
