#pragma once

#include <filesystem>            // std::filesystem::path
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

// Include necessary type headers used in declarations
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp" // Brings in Result, u64, etc.

namespace package {
  namespace fs = std::filesystem;
  using util::error::DracError;
  using util::types::Future;
  using util::types::i64;
  using util::types::Result;
  using util::types::String;
  using util::types::u64;

  /**
   * @struct PkgCountCacheData
   * @brief Structure for caching package count results along with a timestamp.
   */
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

  /**
   * @struct PackageManagerInfo
   * @brief Holds information needed to query a database-backed package manager.
   */
  struct PackageManagerInfo {
    String   id;         ///< Unique identifier (e.g., "pacman", "dpkg", used for cache key).
    fs::path dbPath;     ///< Filesystem path to the database or primary directory.
    String   countQuery; ///< Query string (e.g., SQL) or specific file/pattern if not DB.
  };

  /**
   * @brief Gets the total package count by querying all relevant package managers.
   * @return Result containing the total package count (u64) on success,
   * or a DracError if aggregation fails (individual errors logged).
   */
  fn GetTotalCount() -> Result<u64, DracError>;

  /**
   * @brief Gets package count from a database using SQLite.
   * @param pmInfo Information about the package manager database.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDb(const PackageManagerInfo& pmInfo) -> Result<u64, DracError>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally filtering and subtracting.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDirectory(
    const String&   pmId,
    const fs::path& dirPath,
    const String&   fileExtensionFilter,
    bool            subtractOne
  ) -> Result<u64, DracError>;

  /**
   * @brief Gets package count by iterating entries in a directory, filtering by extension.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @return Result containing the count (u64) or a DracError. Defaults subtractOne to false.
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, const String& fileExtensionFilter)
    -> Result<u64, DracError>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally subtracting one.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError. Defaults fileExtensionFilter to "".
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, bool subtractOne) -> Result<u64, DracError>;

  /**
   * @brief Gets package count by iterating all entries in a directory.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @return Result containing the count (u64) or a DracError. Defaults filter to "" and subtractOne to false.
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath) -> Result<u64, DracError>;

#ifdef __linux__
  fn GetDpkgCount() -> Result<u64, DracError>;
  fn GetPacmanCount() -> Result<u64, DracError>;
  fn GetMossCount() -> Result<u64, DracError>;
  fn GetRpmCount() -> Result<u64, DracError>;
  fn GetZypperCount() -> Result<u64, DracError>;
  fn GetPortageCount() -> Result<u64, DracError>;
  fn GetApkCount() -> Result<u64, DracError>;
#elifdef __APPLE__
  fn GetHomebrewCount() -> Result<u64, DracError>;
  fn GetMacPortsCount() -> Result<u64, DracError>;
#elifdef _WIN32
  fn GetWinRTCount() -> Result<u64, DracError>;
  fn GetChocolateyCount() -> Result<u64, DracError>;
  fn GetScoopCount() -> Result<u64, DracError>;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
  fn GetPkgNgCount() -> Result<u64, DracError>;
#elifdef __NetBSD__
  fn GetPkgSrcCount() -> Result<u64, DracError>;
#elifdef __HAIKU__
  fn GetHaikuCount() -> Result<u64, DracError>;
#elifdef __serenity__
  fn GetSerenityCount() -> Result<u64, DracError>;
#endif

  // Common (potentially cross-platform)
#ifndef _WIN32
  fn GetNixCount() -> Result<u64, DracError>;
#endif
  fn GetCargoCount() -> Result<u64, DracError>;
} // namespace package
