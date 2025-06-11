#pragma once

#if DRAC_ENABLE_PACKAGECOUNT

// clang-format off
#include <filesystem>            // std::filesystem::path

#include "Util/ConfigData.hpp"
#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"
// clang-format on

namespace package {
  /**
   * @brief Checks if a specific PackageManager flag is set in a given bitmask.
   * @note This is an internal helper function for the PackageCounting service.
   *
   * @param current_flags The bitmask of currently enabled PackageManager flags.
   * @param flag_to_check The specific PackageManager flag to check for.
   * @return `true` if `flag_to_check` is set in `current_flags`, `false` otherwise.
   */
  constexpr fn HasPackageManager(config::PackageManager current_flags, config::PackageManager flag_to_check) -> bool {
    return (static_cast<unsigned int>(current_flags) & static_cast<unsigned int>(flag_to_check)) != 0;
  }

  /**
   * @struct PackageManagerInfo
   * @brief Holds information needed to query a database-backed package manager.
   */
  struct PackageManagerInfo {
    util::types::String   id;         ///< Unique identifier (e.g., "pacman", "dpkg", used for cache key).
    std::filesystem::path dbPath;     ///< Filesystem path to the database or primary directory.
    util::types::String   countQuery; ///< Query string (e.g., SQL) or specific file/pattern if not DB.
  };

  /**
   * @brief Gets the total package count by querying all relevant package managers.
   * @return Result containing the total package count (u64) on success,
   * or a DracError if aggregation fails (individual errors logged).
   */
  fn GetTotalCount() -> util::types::Result<util::types::u64>;

  /**
   * @brief Gets package count from a database using SQLite.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dbPath Path to the SQLite database file.
   * @param countQuery SQL query to count packages (e.g., "SELECT COUNT(*) FROM packages").
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDb(const util::types::String& pmId, const std::filesystem::path& dbPath, const util::types::String& countQuery) -> util::types::Result<util::types::u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally filtering and subtracting.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDirectory(
    const util::types::String&   pmId,
    const std::filesystem::path& dirPath,
    const util::types::String&   fileExtensionFilter,
    bool                         subtractOne
  ) -> util::types::Result<util::types::u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, filtering by extension.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @return Result containing the count (u64) or a DracError. Defaults subtractOne to false.
   */
  fn GetCountFromDirectory(const util::types::String& pmId, const std::filesystem::path& dirPath, const util::types::String& fileExtensionFilter) -> util::types::Result<util::types::u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally subtracting one.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError. Defaults fileExtensionFilter to "".
   */
  fn GetCountFromDirectory(const util::types::String& pmId, const std::filesystem::path& dirPath, bool subtractOne) -> util::types::Result<util::types::u64>;

  /**
   * @brief Gets package count by iterating all entries in a directory.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @return Result containing the count (u64) or a DracError. Defaults filter to "" and subtractOne to false.
   */
  fn GetCountFromDirectory(const util::types::String& pmId, const std::filesystem::path& dirPath) -> util::types::Result<util::types::u64>;

  #ifdef __linux__
  /**
   * @brief Counts installed packages using APK.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountApk() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Dpkg.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountDpkg() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Moss.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountMoss() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Pacman.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountPacman() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Rpm.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountRpm() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Xbps.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountXbps() -> util::types::Result<util::types::u64>;

  /**
   * @brief Counts installed packages in a plist file (used by xbps and potentially others).
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param plistPath Path to the plist file.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromPlist(const util::types::String& pmId, const std::filesystem::path& plistPath) -> util::types::Result<util::types::u64>;
  #elifdef __APPLE__
  /**
   * @brief Counts installed packages using Homebrew.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetHomebrewCount() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using MacPorts.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetMacPortsCount() -> util::types::Result<util::types::u64>;
  #elifdef _WIN32
  /**
   * @brief Counts installed packages using WinGet.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountWinGet() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Chocolatey.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountChocolatey() -> util::types::Result<util::types::u64>;
  /**
   * @brief Counts installed packages using Scoop.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountScoop() -> util::types::Result<util::types::u64>;
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
  /**
   * @brief Counts installed packages using PkgNg.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetPkgNgCount() -> util::types::Result<util::types::u64>;
  #elifdef __NetBSD__
  /**
   * @brief Counts installed packages using PkgSrc.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetPkgSrcCount() -> util::types::Result<util::types::u64>;
  #elifdef __HAIKU__
  /**
   * @brief Counts installed packages using Haiku.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetSerenityCount() -> util::types::Result<util::types::u64>;
  #endif

  #if defined(__linux__) || defined(__APPLE__)
  /**
   * @brief Counts installed packages using Nix.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountNix() -> util::types::Result<util::types::u64>;
  #endif
  /**
   * @brief Counts installed packages using Cargo.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountCargo() -> util::types::Result<util::types::u64>;
} // namespace package

#endif // DRAC_ENABLE_PACKAGECOUNT
