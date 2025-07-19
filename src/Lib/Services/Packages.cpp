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
    #if DRAC_ENABLE_PACKAGECOUNT
    Vec<Future<Result<u64>>> futures;
    futures.reserve(16);

    fn addFutureIfEnabled = [&futures, &cache, enabledPackageManagers]<typename T>(const Manager manager, T&& countFunc) -> Unit {
      if (HasPackageManager(enabledPackageManagers, manager))
        futures.emplace_back(std::async(std::launch::async, std::forward<T>(countFunc), std::ref(cache)));
    };

      #ifdef __linux__
    addFutureIfEnabled(Manager::APK, CountApk);
    addFutureIfEnabled(Manager::DPKG, CountDpkg);
    addFutureIfEnabled(Manager::MOSS, CountMoss);
    addFutureIfEnabled(Manager::PACMAN, CountPacman);
    addFutureIfEnabled(Manager::RPM, CountRpm);
        #ifdef HAVE_PUGIXML
    addFutureIfEnabled(Manager::XBPS, CountXbps);
        #endif
      #elifdef __APPLE__
    addFutureIfEnabled(Manager::HOMEBREW, GetHomebrewCount);
    addFutureIfEnabled(Manager::MACPORTS, GetMacPortsCount);
      #elifdef _WIN32
    addFutureIfEnabled(Manager::WINGET, CountWinGet);
    addFutureIfEnabled(Manager::CHOCOLATEY, CountChocolatey);
    addFutureIfEnabled(Manager::SCOOP, CountScoop);
      #elif defined(__FreeBSD__) || defined(__DragonFly__)
    addFutureIfEnabled(Manager::PKGNG, GetPkgNgCount);
      #elifdef __NetBSD__
    addFutureIfEnabled(Manager::PKGSRC, GetPkgSrcCount);
      #elifdef __HAIKU__
    addFutureIfEnabled(Manager::HAIKUPKG, GetHaikuCount);
      #elifdef __serenity__
    addFutureIfEnabled(Manager::SERENITY, GetSerenityCount);
      #endif

      #if defined(__linux__) || defined(__APPLE__)
    addFutureIfEnabled(Manager::NIX, CountNix);
      #endif
    addFutureIfEnabled(Manager::CARGO, CountCargo);

    if (futures.empty())
      ERR(UnavailableFeature, "No enabled package managers for this platform in precompiled config (feature not available)");
    #else
    ERR(NotSupported, "Package counting disabled by precompiled configuration (feature not supported)");
    #endif
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
    if (HasPackageManager(enabledPackageManagers, Manager::APK))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountApk(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::DPKG))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountDpkg(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::MOSS))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountMoss(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::PACMAN))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountPacman(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::RPM))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountRpm(cache); });
      #ifdef HAVE_PUGIXML
    if (HasPackageManager(enabledPackageManagers, Manager::XBPS))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountXbps(cache); });
      #endif
    #elifdef __APPLE__
    if (HasPackageManager(enabledPackageManagers, Manager::HOMEBREW))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetHomebrewCount(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::MACPORTS))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetMacPortsCount(cache); });
    #elifdef _WIN32
    if (HasPackageManager(enabledPackageManagers, Manager::WINGET))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountWinGet(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::CHOCOLATEY))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountChocolatey(cache); });
    if (HasPackageManager(enabledPackageManagers, Manager::SCOOP))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountScoop(cache); });
    #elif defined(__FreeBSD__) || defined(__DragonFly__)
    if (HasPackageManager(enabledPackageManagers, Manager::PKGNG))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetPkgNgCount(cache); });
    #elifdef __NetBSD__
    if (HasPackageManager(enabledPackageManagers, Manager::PKGSRC))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetPkgSrcCount(cache); });
    #elifdef __HAIKU__
    if (HasPackageManager(enabledPackageManagers, Manager::HAIKUPKG))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetHaikuCount(cache); });
    #elifdef __serenity__
    if (HasPackageManager(enabledPackageManagers, Manager::SERENITY))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return GetSerenityCount(cache); });
    #endif

    #if defined(__linux__) || defined(__APPLE__)
    if (HasPackageManager(enabledPackageManagers, Manager::NIX))
      futures.at(active++) = std::async(std::launch::async, [&cache]() { return CountNix(cache); });
    #endif

    if (HasPackageManager(enabledPackageManagers, Manager::CARGO))
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
} // namespace draconis::services::packages

#endif // DRAC_ENABLE_PACKAGECOUNT
