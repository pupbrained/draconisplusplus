#pragma once

#include <filesystem>            // std::filesystem::path
#include <future>                // std::future
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"

namespace packages {
  namespace fs = std::filesystem;
  using util::error::DracError;
  using util::types::Result, util::types::u64, util::types::i64, util::types::String, util::types::Vec,
    util::types::Future;

  /**
   * @struct PkgCountCacheData
   * @brief Structure for caching package count results along with a timestamp.
   *
   * Used to avoid redundant lookups in package manager databases or directories
   * if the underlying data source hasn't changed recently.
   */
  struct PkgCountCacheData {
    u64 count {};                 ///< The cached package count.
    i64 timestampEpochSeconds {}; ///< The UNIX timestamp (seconds since epoch) when the count was cached.

    // NOLINTBEGIN(readability-identifier-naming)
    struct [[maybe_unused]] glaze {
      using T = PkgCountCacheData;
      static constexpr glz::detail::Object value =
        glz::object("count", &T::count, "timestamp", &T::timestampEpochSeconds);
    };
    // NOLINTEND(readability-identifier-naming)
  };

  /**
   * @struct PackageManagerInfo
   * @brief Holds information needed to query a database-backed package manager.
   */
  struct PackageManagerInfo {
    String   id;         ///< Unique identifier for the package manager (used for cache key).
    fs::path dbPath;     ///< Filesystem path to the package manager's database.
    String   countQuery; ///< SQL query string to count the packages.
  };

  /**
   * @brief Gets the total package count by querying all relevant package managers for the current OS.
   * @details This function orchestrates the process:
   * 1. Determines the set of relevant package managers (platform-specific + shared).
   * 2. Launches asynchronous tasks to query each manager.
   * 3. Aggregates the results, summing counts and logging errors.
   * @return Result containing the total package count (u64) on success,
   * or a DracError if the aggregation fails (though individual manager errors are logged).
   */
  fn GetTotalCount() -> Result<u64, DracError>;

  fn GetCountFromDb(const PackageManagerInfo& pmInfo) -> Result<u64, DracError>;

  fn GetCountFromDirectory(
    const String&   pmId,
    const fs::path& dirPath,
    const String&   fileExtensionFilter = "",
    bool            subtractOne         = false
  ) -> Result<u64, DracError>;

#ifdef __linux__
  fn GetDpkgCount() -> Result<u64, DracError>;
  fn GetPacmanCount() -> Result<u64, DracError>;
  fn GetMossCount() -> Result<u64, DracError>;
  fn GetRpmCount() -> Result<u64, DracError>;
  fn GetZypperCount() -> Result<u64, DracError>;
  fn GetPortageCount() -> Result<u64, DracError>;
  fn GetApkCount() -> Result<u64, DracError>;
#elif defined(__APPLE__)
  fn GetHomebrewCount() -> Result<u64, DracError>;
  fn GetMacPortsCount() -> Result<u64, DracError>;
#elif defined(_WIN32)
  fn GetWinRTCount() -> Result<u64, DracError>;
  fn GetChocolateyCount() -> Result<u64, DracError>;
  fn GetScoopCount() -> Result<u64, DracError>;
#endif

#ifndef _WIN32
  fn GetNixCount() -> Result<u64, DracError>;
#endif
  fn GetCargoCount() -> Result<u64, DracError>;
} // namespace packages