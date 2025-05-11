#pragma once

#include <filesystem>            // std::filesystem::path
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

namespace package {
  namespace fs = std::filesystem;
  using util::error::DracError;
  using util::types::Future, util::types::i64, util::types::Result, util::types::String, util::types::u64;

  /**
   * @struct PkgCountCacheData
   * @brief Structure for caching package count results along with a timestamp.
   */
  struct PkgCountCacheData {
    u64 count {};
    i64 timestampEpochSeconds {};

    PkgCountCacheData() = default;
    PkgCountCacheData(u64 count, i64 timestampEpochSeconds)
      : count(count), timestampEpochSeconds(timestampEpochSeconds) {}

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
  fn GetTotalCount() -> Result<u64>;

  /**
   * @brief Gets package count from a database using SQLite.
   * @param pmInfo Information about the package manager database.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDb(const String& pmId, const fs::path& dbPath, const String& countQuery) -> Result<u64>;

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
  ) -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, filtering by extension.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @return Result containing the count (u64) or a DracError. Defaults subtractOne to false.
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, const String& fileExtensionFilter)
    -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally subtracting one.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError. Defaults fileExtensionFilter to "".
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, bool subtractOne) -> Result<u64>;

  /**
   * @brief Gets package count by iterating all entries in a directory.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @return Result containing the count (u64) or a DracError. Defaults filter to "" and subtractOne to false.
   */
  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath) -> Result<u64>;

#ifdef __linux__
  fn CountApk() -> Result<u64>;
  fn CountDpkg() -> Result<u64>;
  fn CountMoss() -> Result<u64>;
  fn CountPacman() -> Result<u64>;
  fn CountRpm() -> Result<u64>;
  fn CountXbps() -> Result<u64>;

  /**
   * @brief Counts installed packages in a plist file (used by xbps and potentially others).
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param plistPath Path to the plist file.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromPlist(const String& pmId, const std::filesystem::path& plistPath) -> Result<u64>;
#elifdef __APPLE__
  fn GetHomebrewCount() -> Result<u64>;
  fn GetMacPortsCount() -> Result<u64>;
#elifdef _WIN32
  fn CountWinGet() -> Result<u64>;
  fn CountChocolatey() -> Result<u64>;
  fn CountScoop() -> Result<u64>;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
  fn GetPkgNgCount() -> Result<u64>;
#elifdef __NetBSD__
  fn GetPkgSrcCount() -> Result<u64>;
#elifdef __HAIKU__
  fn GetHaikuCount() -> Result<u64>;
#elifdef __serenity__
  fn GetSerenityCount() -> Result<u64>;
#endif

#if defined(__linux__) || defined(__APPLE__)
  fn CountNix() -> Result<u64>;
#endif
  fn CountCargo() -> Result<u64>;
} // namespace package
