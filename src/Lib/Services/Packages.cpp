#if DRAC_ENABLE_PACKAGECOUNT

  #include "Drac++/Services/Packages.hpp"

  #if !defined(__serenity__) && !defined(_WIN32)
    #include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
    #include <SQLiteCpp/Exception.h> // SQLite::Exception
    #include <SQLiteCpp/Statement.h> // SQLite::Statement
  #endif

  #if defined(__linux__) && defined(HAVE_PUGIXML)
    #include <pugixml.hpp> // pugi::{xml_document, xml_node, xml_parse_result}
  #endif

  #include <filesystem>   // std::filesystem
  #include <future>       // std::{async, future, launch}
  #include <matchit.hpp>  // matchit::{match, is, or_, _}
  #include <system_error> // std::{errc, error_code}

  #include "Drac++/Utils/Env.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Logging.hpp"
  #include "Drac++/Utils/Types.hpp"

namespace fs = std::filesystem;

using namespace draconis::utils::types;
using draconis::utils::cache::CacheManager;
using enum draconis::utils::error::DracErrorCode;

namespace {
  constexpr const char* CACHE_KEY_PREFIX = "pkg_count_";

  fn GetCountFromDirectoryImplNoCache(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    std::error_code fsErrCode;

    fsErrCode.clear();

    if (!fs::is_directory(dirPath, fsErrCode)) {
      if (fsErrCode && fsErrCode != std::errc::no_such_file_or_directory)
        ERR_FMT(ResourceExhausted, "Filesystem error checking if '{}' is a directory: {} (resource exhausted or API unavailable)", dirPath.string(), fsErrCode.message());

      ERR_FMT(NotFound, "{} path is not a directory: {}", pmId, dirPath.string());
    }

    fsErrCode.clear();

    u64              count     = 0;
    const bool       hasFilter = fileExtensionFilter.has_value();
    const StringView filter    = fileExtensionFilter ? StringView(*fileExtensionFilter) : StringView();

    try {
      const fs::directory_iterator dirIter(
        dirPath,
        fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink,
        fsErrCode
      );

      if (fsErrCode)
        ERR_FMT(ResourceExhausted, "Failed to create iterator for {} directory '{}': {} (resource exhausted or API unavailable)", pmId, dirPath.string(), fsErrCode.message());

      if (hasFilter) {
        for (const fs::directory_entry& entry : dirIter) {
          if (entry.path().empty())
            continue;

          if (std::error_code isFileErr; entry.is_regular_file(isFileErr) && !isFileErr) {
            if (entry.path().extension().string() == filter)
              count++;
          } else if (isFileErr)
            warn_log("Error stating entry '{}' in {} directory: {}", entry.path().string(), pmId, isFileErr.message());
        }
      } else {
        for (const fs::directory_entry& entry : dirIter) {
          if (!entry.path().empty())
            count++;
        }
      }
    } catch (const fs::filesystem_error& fsCatchErr) {
      ERR_FMT(ResourceExhausted, "Filesystem error during {} directory iteration: {} (resource exhausted or API unavailable)", pmId, fsCatchErr.what());
    } catch (const Exception& exc) {
      ERR_FMT(InternalError, "Internal error during {} directory iteration: {}", pmId, exc.what());
    } catch (...) {
      ERR_FMT(Other, "Unknown error iterating {} directory (unexpected exception)", pmId);
    }

    if (subtractOne && count > 0)
      count--;

    return count;
  }

  fn GetCountFromDirectoryImpl(
    CacheManager&         cache,
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    return cache.getOrSet<u64>(std::format("{}{}", CACHE_KEY_PREFIX, pmId), [&]() -> Result<u64> {
      return GetCountFromDirectoryImplNoCache(pmId, dirPath, fileExtensionFilter, subtractOne);
    });
  }

} // namespace

namespace draconis::services::packages {
  fn GetCountFromDirectory(
    CacheManager&   cache,
    const String&   pmId,
    const fs::path& dirPath,
    const String&   fileExtensionFilter,
    const bool      subtractOne
  ) -> Result<u64> {
    return GetCountFromDirectoryImpl(cache, pmId, dirPath, fileExtensionFilter, subtractOne);
  }

  fn GetCountFromDirectory(CacheManager& cache, const String& pmId, const fs::path& dirPath, const String& fileExtensionFilter) -> Result<u64> {
    return GetCountFromDirectoryImpl(cache, pmId, dirPath, fileExtensionFilter, false);
  }

  fn GetCountFromDirectory(CacheManager& cache, const String& pmId, const fs::path& dirPath, const bool subtractOne) -> Result<u64> {
    return GetCountFromDirectoryImpl(cache, pmId, dirPath, None, subtractOne);
  }

  fn GetCountFromDirectory(CacheManager& cache, const String& pmId, const fs::path& dirPath) -> Result<u64> {
    return GetCountFromDirectoryImpl(cache, pmId, dirPath, None, false);
  }

  fn GetCountFromDirectoryNoCache(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    return GetCountFromDirectoryImplNoCache(pmId, dirPath, fileExtensionFilter, subtractOne);
  }

  #if !defined(__serenity__) && !defined(_WIN32)
  fn GetCountFromDb(
    CacheManager&   cache,
    const String&   pmId,
    const fs::path& dbPath,
    const String&   countQuery
  ) -> Result<u64> {
    return cache.getOrSet<u64>(std::format("{}{}", CACHE_KEY_PREFIX, pmId), [&]() -> Result<u64> {
      u64 count = 0;

      try {
        if (std::error_code existsErr; !fs::exists(dbPath, existsErr) || existsErr)
          ERR_FMT(NotFound, "{} database not found at '{}' (file does not exist or access denied)", pmId, dbPath.string());

        const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);

        if (SQLite::Statement queryStmt(database, countQuery); queryStmt.executeStep()) {
          const i64 countInt64 = queryStmt.getColumn(0).getInt64();

          if (countInt64 < 0)
            ERR_FMT(CorruptedData, "Negative count returned by {} DB COUNT query (corrupt database data)", pmId);

          count = static_cast<u64>(countInt64);
        } else
          ERR_FMT(ParseError, "No rows returned by {} DB COUNT query (empty result set)", pmId);
      } catch (const SQLite::Exception& e) {
        ERR_FMT(ApiUnavailable, "SQLite error occurred accessing {} database '{}': {}", pmId, dbPath.string(), e.what());
      } catch (const Exception& e) {
        ERR_FMT(InternalError, "Standard exception accessing {} database '{}': {}", pmId, dbPath.string(), e.what());
      } catch (...) {
        ERR_FMT(Other, "Unknown error occurred accessing {} database (unexpected exception)", pmId);
      }

      return count;
    });
  }
  #endif // __serenity__ || _WIN32

  #if defined(__linux__) && defined(HAVE_PUGIXML)
  fn GetCountFromPlist(
    CacheManager&   cache,
    const String&   pmId,
    const fs::path& plistPath
  ) -> Result<u64> {
    return cache.getOrSet<u64>(std::format("{}{}", CACHE_KEY_PREFIX, pmId), [&]() -> Result<u64> {
      xml_document doc;

      if (const xml_parse_result result = doc.load_file(plistPath.c_str()); !result)
        ERR_FMT(ParseError, "Failed to parse plist file '{}': {} (malformed XML)", plistPath.string(), result.description());

      const xml_node dict = doc.child("plist").child("dict");

      if (!dict)
        ERR_FMT(CorruptedData, "No <dict> element found in plist file '{}' (corrupt plist structure)", plistPath.string());

      u64              count           = 0;
      const StringView alternativesKey = "_XBPS_ALTERNATIVES_";
      const StringView keyName         = "key";
      const StringView stateValue      = "installed";

      for (xml_node node = dict.first_child(); node; node = node.next_sibling()) {
        if (StringView(node.name()) != keyName)
          continue;

        if (const StringView keyName = node.child_value(); keyName == alternativesKey)
          continue;

        xml_node pkgDict = node.next_sibling("dict");

        if (!pkgDict)
          continue;

        bool isInstalled = false;

        for (xml_node pkgNode = pkgDict.first_child(); pkgNode; pkgNode = pkgNode.next_sibling())
          if (StringView(pkgNode.name()) == keyName && StringView(pkgNode.child_value()) == "state")
            if (xml_node stateValue = pkgNode.next_sibling("string"); stateValue && StringView(stateValue.child_value()) == stateValue) {
              isInstalled = true;
              break;
            }

        if (isInstalled)
          ++count;
      }

      if (count == 0)
        ERR_FMT(NotFound, "No installed packages found in plist file '{}' (empty package list)", plistPath.string());

      return count;
    });
  }
  #endif // __linux__

  #if defined(__linux__) || defined(__APPLE__)
  fn CountNix(CacheManager& cache) -> Result<u64> {
    return GetCountFromDb(cache, "nix", "/nix/var/nix/db/db.sqlite", "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL");
  }
  #endif // __linux__ || __APPLE__

  fn CountCargo(CacheManager& cache) -> Result<u64> {
    using draconis::utils::env::GetEnv;

    fs::path cargoPath {};

    if (const Result<PCStr> cargoHome = GetEnv("CARGO_HOME"))
      cargoPath = fs::path(*cargoHome) / "bin";
    else if (const Result<PCStr> homeDir = GetEnv("HOME"))
      cargoPath = fs::path(*homeDir) / ".cargo" / "bin";

    if (cargoPath.empty() || !fs::exists(cargoPath))
      ERR(ConfigurationError, "Could not find cargo directory (CARGO_HOME or ~/.cargo/bin not configured)");

    return GetCountFromDirectory(cache, "cargo", cargoPath);
  }

  fn GetTotalCount(CacheManager& cache, const Manager enabledPackageManagers) -> Result<u64> {
  #if DRAC_PRECOMPILED_CONFIG
    Vec<Future<Result<u64>>> futures;
    futures.reserve(16);

    fn addFutureIfEnabled = [&futures, &cache, enabledPackageManagers]<typename T>(const Manager manager, T&& countFunc) -> Unit {
      if (HasPackageManager(enabledPackageManagers, manager))
        futures.emplace_back(std::async(std::launch::async, std::forward<T>(countFunc), std::ref(cache)));
    };

    #ifdef __linux__
    addFutureIfEnabled(Manager::Apk, CountApk);
    addFutureIfEnabled(Manager::Dpkg, CountDpkg);
    addFutureIfEnabled(Manager::Moss, CountMoss);
    addFutureIfEnabled(Manager::Pacman, CountPacman);
    addFutureIfEnabled(Manager::Rpm, CountRpm);
      #ifdef HAVE_PUGIXML
    addFutureIfEnabled(Manager::Xbps, CountXbps);
      #endif
    #elifdef __APPLE__
    addFutureIfEnabled(Manager::Homebrew, GetHomebrewCount);
    addFutureIfEnabled(Manager::Macports, GetMacPortsCount);
    #elifdef _WIN32
    addFutureIfEnabled(Manager::Winget, CountWinGet);
    addFutureIfEnabled(Manager::Chocolatey, CountChocolatey);
    addFutureIfEnabled(Manager::Scoop, CountScoop);
    #elif defined(__FreeBSD__) || defined(__DragonFly__)
    addFutureIfEnabled(Manager::PkgNg, GetPkgNgCount);
    #elifdef __NetBSD__
    addFutureIfEnabled(Manager::PkgSrc, GetPkgSrcCount);
    #elifdef __HAIKU__
    addFutureIfEnabled(Manager::HaikuPkg, GetHaikuCount);
    #elifdef __serenity__
    addFutureIfEnabled(Manager::Serenity, GetSerenityCount);
    #endif

    #if defined(__linux__) || defined(__APPLE__)
    addFutureIfEnabled(Manager::Nix, CountNix);
    #endif
    addFutureIfEnabled(Manager::Cargo, CountCargo);

    if (futures.empty())
      ERR(UnavailableFeature, "No enabled package managers for this platform in precompiled config (feature not available)");
  #else
    #ifdef __linux__
      #ifdef HAVE_PUGIXML
    constexpr size_t platformSpecificCount = 6; // Apk, Dpkg, Moss, Pacman, Rpm, Xbps
      #else
    constexpr size_t platformSpecificCount = 5; // Apk, Dpkg, Moss, Pacman, Rpm
      #endif
    #elifdef __APPLE__
    constexpr size_t platformSpecificCount = 2; // Homebrew, MacPorts
    #elifdef _WIN32
    constexpr size_t platformSpecificCount = 3; // WinGet, Chocolatey, Scoop
    #elif defined(__FreeBSD__) || defined(__DragonFly__)
    constexpr size_t platformSpecificCount = 1; // GetPkgNgCount
    #elifdef __NetBSD__
    constexpr size_t platformSpecificCount = 1; // GetPkgSrcCount
    #elifdef __HAIKU__
    constexpr size_t platformSpecificCount = 1; // GetHaikuCount
    #elifdef __serenity__
    constexpr size_t platformSpecificCount = 1; // GetSerenityCount
    #endif

    #if defined(__linux__) || defined(__APPLE__)
    // platform specific + cargo + nix
    constexpr size_t numFutures = platformSpecificCount + 2;
    #else
    // platform specific + cargo
    constexpr size_t numFutures = platformSpecificCount + 1;
    #endif

    Array<Future<Result<u64>>, numFutures> futures;
    size_t                                 active = 0;

    #ifdef __linux__
    if (HasPackageManager(enabledPackageManagers, Manager::Apk))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountApk(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Dpkg))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountDpkg(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Moss))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountMoss(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Pacman))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountPacman(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Rpm))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountRpm(cache); });
      #ifdef HAVE_PUGIXML
    if (HasPackageManager(enabledPackageManagers, Manager::Xbps))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountXbps(cache); });
      #endif
    #elifdef __APPLE__
    if (HasPackageManager(enabledPackageManagers, Manager::Homebrew))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetHomebrewCount(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Macports))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetMacPortsCount(cache); });
    #elifdef _WIN32
    if (HasPackageManager(enabledPackageManagers, Manager::Winget))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountWinGet(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Chocolatey))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountChocolatey(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::Scoop))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountScoop(cache); });
    #elif defined(__FreeBSD__) || defined(__DragonFly__)
    if (HasPackageManager(enabledPackageManagers, Manager::PkgNg))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetPkgNgCount(cache); });
    #elifdef __NetBSD__
    if (HasPackageManager(enabledPackageManagers, Manager::PkgSrc))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetPkgSrcCount(cache); });
    #elifdef __HAIKU__
    if (HasPackageManager(enabledPackageManagers, Manager::HaikuPkg))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetHaikuCount(cache); });
    #elifdef __serenity__
    if (HasPackageManager(enabledPackageManagers, Manager::Serenity))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetSerenityCount(cache); });
    #endif

    #if defined(__linux__) || defined(__APPLE__)
    if (HasPackageManager(enabledPackageManagers, Manager::Nix))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountNix(cache); });
    #endif

    if (HasPackageManager(enabledPackageManagers, Manager::Cargo))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountCargo(cache); });

    if (active == 0)
      ERR(UnavailableFeature, "No enabled package managers for this platform.");
  #endif // DRAC_PRECOMPILED_CONFIG
    u64  totalCount   = 0;
    bool oneSucceeded = false;

    constexpr usize chunkSize = 4;

    usize effectiveSize =
  #if DRAC_PRECOMPILED_CONFIG
      futures.size();
  #else
      active;
  #endif

    for (usize i = 0; i < effectiveSize; i += chunkSize) {
      const usize end = std::min(i + chunkSize, effectiveSize);

      for (usize j = i; j < end; ++j) {
        try {
          using matchit::match, matchit::is, matchit::or_, matchit::_;

          if (const Result<u64> result = futures.at(j).get()) {
            totalCount += *result;
            oneSucceeded = true;
          } else
            match(result.error().code)(
              is | or_(NotFound, ApiUnavailable, NotSupported) = [&] -> Unit { debug_at(result.error()); },
              is | _                                           = [&] -> Unit { error_at(result.error()); }
            );
        } catch (const Exception& exc) {
          error_log("Exception while getting package count future: {}", exc.what());
        } catch (...) {
          error_log("Unknown exception while getting package count future (unexpected exception)");
        }
      }
    }

    if (!oneSucceeded && totalCount == 0)
      ERR(UnavailableFeature, "No package managers found or none reported counts (feature not available)");

    return totalCount;
  }

  fn GetIndividualCounts(CacheManager& cache, const Manager enabledPackageManagers) -> Result<Map<String, u64>> {
  #if DRAC_PRECOMPILED_CONFIG
    Vec<Future<Result<u64>>> futures;
    Vec<String>              managerNames;
    futures.reserve(16);
    managerNames.reserve(16);
  #endif

    if constexpr (DRAC_PRECOMPILED_CONFIG) {
      fn addFutureIfEnabled = [&futures, &managerNames, &cache, enabledPackageManagers]<typename T>(const Manager manager, const String& name, T&& countFunc) -> Unit {
        if (HasPackageManager(enabledPackageManagers, manager)) {
          futures.emplace_back(std::async(std::launch::async, std::forward<T>(countFunc), std::ref(cache)));
          managerNames.emplace_back(name);
        }
      };

  #ifdef __linux__
      addFutureIfEnabled(Manager::Apk, "apk", CountApk);
      addFutureIfEnabled(Manager::Dpkg, "dpkg", CountDpkg);
      addFutureIfEnabled(Manager::Moss, "moss", CountMoss);
      addFutureIfEnabled(Manager::Pacman, "pacman", CountPacman);
      addFutureIfEnabled(Manager::Rpm, "rpm", CountRpm);
    #ifdef HAVE_PUGIXML
      addFutureIfEnabled(Manager::Xbps, "xbps", CountXbps);
    #endif
  #elifdef __APPLE__
      addFutureIfEnabled(Manager::Homebrew, "homebrew", GetHomebrewCount);
      addFutureIfEnabled(Manager::Macports, "macports", GetMacPortsCount);
  #elifdef _WIN32
      addFutureIfEnabled(Manager::Winget, "winget", CountWinGet);
      addFutureIfEnabled(Manager::Chocolatey, "chocolatey", CountChocolatey);
      addFutureIfEnabled(Manager::Scoop, "scoop", CountScoop);
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
      addFutureIfEnabled(Manager::PkgNg, "pkgng", GetPkgNgCount);
  #elifdef __NetBSD__
      addFutureIfEnabled(Manager::PkgSrc, "pkgsrc", GetPkgSrcCount);
  #elifdef __HAIKU__
      addFutureIfEnabled(Manager::HaikuPkg, "haikupkg", GetHaikuCount);
  #endif

  #if defined(__linux__) || defined(__APPLE__)
      addFutureIfEnabled(Manager::Nix, "nix", CountNix);
  #endif
      addFutureIfEnabled(Manager::Cargo, "cargo", CountCargo);

      if (futures.empty())
        ERR(UnavailableFeature, "No enabled package managers for this platform in precompiled config (feature not available)");
    } else {
  #ifdef __linux__
    #ifdef HAVE_PUGIXML
      constexpr size_t platformSpecificCount = 6; // Apk, Dpkg, Moss, Pacman, Rpm, Xbps
    #else
      constexpr size_t platformSpecificCount = 5; // Apk, Dpkg, Moss, Pacman, Rpm
    #endif
  #elifdef __APPLE__
      constexpr size_t platformSpecificCount = 2; // Homebrew, MacPorts
  #elifdef _WIN32
      constexpr size_t platformSpecificCount = 3; // WinGet, Chocolatey, Scoop
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
      constexpr size_t platformSpecificCount = 1; // GetPkgNgCount
  #elifdef __NetBSD__
      constexpr size_t platformSpecificCount = 1; // GetPkgSrcCount
  #elifdef __HAIKU__
      constexpr size_t platformSpecificCount = 1; // GetHaikuCount
  #elifdef __serenity__
      constexpr size_t platformSpecificCount = 1; // GetSerenityCount
  #endif

  #if defined(__linux__) || defined(__APPLE__)
      // platform specific + cargo + nix
      constexpr size_t numFutures = platformSpecificCount + 2;
  #else
      // platform specific + cargo
      constexpr size_t numFutures = platformSpecificCount + 1;
  #endif

      Array<Future<Result<u64>>, numFutures> futures;
      Array<String, numFutures>              managerNames;
      size_t                                 active = 0;

  #ifdef __linux__
      if (HasPackageManager(enabledPackageManagers, Manager::Apk)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountApk(cache); });
        managerNames.at(active++) = "apk";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Dpkg)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountDpkg(cache); });
        managerNames.at(active++) = "dpkg";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Moss)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountMoss(cache); });
        managerNames.at(active++) = "moss";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Pacman)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountPacman(cache); });
        managerNames.at(active++) = "pacman";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Rpm)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountRpm(cache); });
        managerNames.at(active++) = "rpm";
      }
    #ifdef HAVE_PUGIXML
      if (HasPackageManager(enabledPackageManagers, Manager::Xbps)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountXbps(cache); });
        managerNames.at(active++) = "xbps";
      }
    #endif
  #elifdef __APPLE__
      if (HasPackageManager(enabledPackageManagers, Manager::Homebrew)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetHomebrewCount(cache); });
        managerNames.at(active++) = "homebrew";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Macports)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetMacPortsCount(cache); });
        managerNames.at(active++) = "macports";
      }
  #elifdef _WIN32
      if (HasPackageManager(enabledPackageManagers, Manager::Winget)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountWinGet(cache); });
        managerNames.at(active++) = "winget";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Chocolatey)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountChocolatey(cache); });
        managerNames.at(active++) = "chocolatey";
      }
      if (HasPackageManager(enabledPackageManagers, Manager::Scoop)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountScoop(cache); });
        managerNames.at(active++) = "scoop";
      }
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
      if (HasPackageManager(enabledPackageManagers, Manager::PkgNg)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetPkgNgCount(cache); });
        managerNames.at(active++) = "pkgng";
      }
  #elifdef __NetBSD__
      if (HasPackageManager(enabledPackageManagers, Manager::PkgSrc)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetPkgSrcCount(cache); });
        managerNames.at(active++) = "pkgsrc";
      }
  #elifdef __HAIKU__
      if (HasPackageManager(enabledPackageManagers, Manager::HaikuPkg)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetHaikuCount(cache); });
        managerNames.at(active++) = "haikupkg";
      }
  #elifdef __serenity__
      if (HasPackageManager(enabledPackageManagers, Manager::Serenity)) { // NOLINT(readability-identifier-naming)
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return GetSerenityCount(cache); });
        managerNames.at(active++) = "serenity";
      }
  #endif

  #if defined(__linux__) || defined(__APPLE__)
      if (HasPackageManager(enabledPackageManagers, Manager::Nix)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountNix(cache); });
        managerNames.at(active++) = "nix";
      }
  #endif

      if (HasPackageManager(enabledPackageManagers, Manager::Cargo)) {
        futures.at(active)        = std::async(std::launch::async, [&cache]() { return CountCargo(cache); });
        managerNames.at(active++) = "cargo";
      }

      if (active == 0)
        ERR(UnavailableFeature, "No enabled package managers for this platform.");
    }

    Map<String, u64> individualCounts;
    bool             oneSucceeded = false;

    constexpr usize chunkSize = 4;

    usize effectiveSize =
  #if DRAC_PRECOMPILED_CONFIG
      futures.size();
  #else
      active;
  #endif

    for (usize i = 0; i < effectiveSize; i += chunkSize) {
      const usize end = std::min(i + chunkSize, effectiveSize);

      for (usize j = i; j < end; ++j) {
        try {
          using matchit::match, matchit::is, matchit::or_, matchit::_;

          if (const Result<u64> result = futures.at(j).get()) {
            individualCounts[managerNames.at(j)] = *result;
            oneSucceeded                         = true;
          } else
            match(result.error().code)(
              is | or_(NotFound, ApiUnavailable, NotSupported) = [&] -> Unit { debug_at(result.error()); },
              is | _                                           = [&] -> Unit { error_at(result.error()); }
            );
        } catch (const Exception& exc) {
          error_log("Exception while getting package count future: {}", exc.what());
        } catch (...) {
          error_log("Unknown exception while getting package count future (unexpected exception)");
        }
      }
    }

    if (!oneSucceeded && individualCounts.empty())
      ERR(UnavailableFeature, "No package managers found or none reported counts (feature not available)");

    return individualCounts;
  }
} // namespace draconis::services::packages

#endif // DRAC_ENABLE_PACKAGECOUNT
