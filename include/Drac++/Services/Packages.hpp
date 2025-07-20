#pragma once

#if DRAC_ENABLE_PACKAGECOUNT

  #include <filesystem> // std::filesystem::path

  #include "../Utils/CacheManager.hpp"
  #include "../Utils/Types.hpp"

namespace draconis::services::packages {
  namespace {
    namespace fs = std::filesystem;

    using utils::cache::CacheManager;

    using utils::types::Map;
    using utils::types::Option;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::u64;
    using utils::types::u8;
  } // namespace

  /**
   * @brief Represents available package managers for package counting.
   *
   * @details This enum is used as a bitmask. Individual values can be combined
   * using the bitwise OR operator (`|`). The availability of specific package managers
   * is conditional on the operating system detected at compile time.
   *
   * @see config::DRAC_ENABLED_PACKAGE_MANAGERS in `config(.example).hpp`.
   * @see draconis::services::packages::operator|
   * @see draconis::services::packages::HasPackageManager
   */
  enum class Manager : u8 {
    NONE  = 0,      ///< No package manager.
    CARGO = 1 << 0, ///< Cargo, the Rust package manager.

  #if defined(__linux__) || defined(__APPLE__)
    NIX = 1 << 1, ///< Nix package manager (available on Linux and macOS).
  #endif

  #ifdef __linux__
    APK    = 1 << 2, ///< apk, the Alpine Linux package manager.
    DPKG   = 1 << 3, ///< dpkg, the Debian package system (used by APT).
    MOSS   = 1 << 4, ///< moss, the package manager for AerynOS.
    PACMAN = 1 << 5, ///< Pacman, the Arch Linux package manager.
    RPM    = 1 << 6, ///< RPM, package manager used by Fedora, RHEL, etc.
    XBPS   = 1 << 7, ///< XBPS, the X Binary Package System (used by Void Linux).
  #elifdef __APPLE__
    HOMEBREW = 1 << 2, ///< Homebrew, package manager for macOS.
    MACPORTS = 1 << 3, ///< MacPorts, package manager for macOS.
  #elifdef _WIN32
    WINGET     = 1 << 1, ///< Winget, the Windows Package Manager.
    CHOCOLATEY = 1 << 2, ///< Chocolatey, package manager for Windows.
    SCOOP      = 1 << 3, ///< Scoop, command-line installer for Windows.
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
    PKGNG = 1 << 1, ///< pkg, package management system for FreeBSD and DragonFly BSD.
  #elifdef __NetBSD__
    PKGSRC = 1 << 1, ///< pkgsrc, package management system for NetBSD.
  #elifdef __HAIKU__
    HAIKUPKG = 1 << 1, ///< haikupkg, package manager for Haiku OS.
  #endif
  };

  /**
   * @brief Combines two PackageManager flags using a bitwise OR operation.
   *
   * @param pmA The first PackageManager flag.
   * @param pmB The second PackageManager flag.
   * @return A new PackageManager value representing the combination of pmA and pmB.
   */
  constexpr fn operator|(Manager pmA, Manager pmB)->Manager {
    return static_cast<Manager>(static_cast<u8>(pmA) | static_cast<u8>(pmB));
  }

  /**
   * @brief Combines two PackageManager flags using a bitwise OR operation and assigns the result to the first flag.
   *
   * @param lhs The first PackageManager flag.
   * @param rhs The second PackageManager flag.
   * @return The first PackageManager flag with the result of the bitwise OR operation.
   */
  constexpr fn operator|=(Manager& lhs, Manager rhs)->Manager& {
    return lhs = lhs | rhs;
  }

  /**
   * @brief Checks if a specific PackageManager flag is set in a given bitmask.
   * @note This is an internal helper function for the PackageCounting service.
   *
   * @param current_flags The bitmask of currently enabled PackageManager flags.
   * @param flag_to_check The specific PackageManager flag to check for.
   * @return `true` if `flag_to_check` is set in `current_flags`, `false` otherwise.
   */
  constexpr fn HasPackageManager(Manager current_flags, Manager flag_to_check) -> bool {
    return (static_cast<u8>(current_flags) & static_cast<u8>(flag_to_check)) != 0;
  }

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
  fn GetTotalCount(CacheManager& cache, Manager enabledPackageManagers) -> Result<u64>;

  /**
   * @brief Gets individual package counts from all enabled package managers.
   * @return Result containing a map of package manager names to their counts on success,
   * or a DracError if all package managers fail (individual errors logged).
   */
  fn GetIndividualCounts(CacheManager& cache, Manager enabledPackageManagers) -> Result<Map<String, u64>>;

  /**
   * @brief Gets package count from a database using SQLite.
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dbPath Path to the SQLite database file.
   * @param countQuery SQL query to count packages (e.g., "SELECT COUNT(*) FROM packages").
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDb(CacheManager& cache, const String& pmId, const fs::path& dbPath, const String& countQuery) -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally filtering and subtracting.
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromDirectory(
    CacheManager&   cache,
    const String&   pmId,
    const fs::path& dirPath,
    const String&   fileExtensionFilter,
    bool            subtractOne
  ) -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, filtering by extension.
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @return Result containing the count (u64) or a DracError. Defaults subtractOne to false.
   */
  fn GetCountFromDirectory(CacheManager& cache, const String& pmId, const fs::path& dirPath, const String& fileExtensionFilter) -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory, optionally subtracting one.
   *
   * @warning This function is prone to misuse due to C++'s implicit conversion rules.
   * A string literal (const char*) can be implicitly converted to bool, causing this
   * overload to be called unintentionally when the user likely intended to call the
   * overload that accepts a file extension filter. Prefer explicit `String` construction
   * for the filter overload.
   *
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError. Defaults fileExtensionFilter to "".
   */
  fn GetCountFromDirectory(
    CacheManager&   cache,
    const String&   pmId,
    const fs::path& dirPath,
    bool            subtractOne
  ) -> Result<u64>;

  /**
   * @brief Gets package count by iterating all entries in a directory.
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param dirPath Path to the directory to iterate.
   * @return Result containing the count (u64) or a DracError. Defaults filter to "" and subtractOne to false.
   */
  fn GetCountFromDirectory(CacheManager& cache, const String& pmId, const fs::path& dirPath) -> Result<u64>;

  /**
   * @brief Gets package count by iterating entries in a directory without caching (for internal use).
   * @param pmId Identifier for the package manager (for logging).
   * @param dirPath Path to the directory to iterate.
   * @param fileExtensionFilter Only count files with this extension (e.g., ".list").
   * @param subtractOne Subtract one from the final count.
   * @return Result containing the count (u64) or a DracError.
   * @note This function is for internal use to avoid nested cache calls.
   */
  fn GetCountFromDirectoryNoCache(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    bool                  subtractOne
  ) -> Result<u64>;

  #ifdef __linux__
  /**
   * @brief Counts installed packages using APK.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountApk(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Dpkg.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountDpkg(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Moss.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountMoss(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Pacman.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountPacman(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Rpm.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountRpm(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Xbps.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountXbps(CacheManager& cache) -> Result<u64>;

  /**
   * @brief Counts installed packages in a plist file (used by xbps and potentially others).
   * @param cache The CacheManager instance to use for caching.
   * @param pmId Identifier for the package manager (for logging/cache).
   * @param plistPath Path to the plist file.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetCountFromPlist(CacheManager& cache, const String& pmId, const fs::path& plistPath) -> Result<u64>;
  #elifdef __APPLE__
  /**
   * @brief Counts installed packages using Homebrew.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetHomebrewCount(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using MacPorts.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetMacPortsCount(CacheManager& cache) -> Result<u64>;
  #elifdef _WIN32
  /**
   * @brief Counts installed packages using WinGet.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountWinGet(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Chocolatey.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountChocolatey(CacheManager& cache) -> Result<u64>;
  /**
   * @brief Counts installed packages using Scoop.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountScoop(CacheManager& cache) -> Result<u64>;
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
  /**
   * @brief Counts installed packages using PkgNg.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetPkgNgCount(CacheManager& cache) -> Result<u64>;
  #elifdef __NetBSD__
  /**
   * @brief Counts installed packages using PkgSrc.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetPkgSrcCount(CacheManager& cache) -> Result<u64>;
  #elifdef __HAIKU__
  /**
   * @brief Counts installed packages using Haiku.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn GetSerenityCount(CacheManager& cache) -> Result<u64>;
  #endif

  #if defined(__linux__) || defined(__APPLE__)
  /**
   * @brief Counts installed packages using Nix.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountNix(CacheManager& cache) -> Result<u64>;
  #endif
  /**
   * @brief Counts installed packages using Cargo.
   * @param cache The CacheManager instance to use for caching.
   * @return Result containing the count (u64) or a DracError.
   */
  fn CountCargo(CacheManager& cache) -> Result<u64>;
} // namespace draconis::services::packages

#endif // DRAC_ENABLE_PACKAGECOUNT
