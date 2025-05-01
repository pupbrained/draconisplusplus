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
