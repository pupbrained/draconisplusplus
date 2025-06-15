#if DRAC_ENABLE_PACKAGECOUNT

  #include "Drac++/Services/PackageCounting.hpp"

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

  #include <DracUtils/Env.hpp>
  #include <DracUtils/Error.hpp>
  #include <DracUtils/Logging.hpp>
  #include <DracUtils/Types.hpp>

  #include "Utils/Caching.hpp"

namespace fs = std::filesystem;

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;
using util::cache::GetValidCache, util::cache::WriteCache;
using util::formatting::SzFormat;

namespace {
  constexpr const char* CACHE_KEY_PREFIX = "pkg_count_";

  fn GetCountFromDirectoryImpl(
    const SZString&         pmId,
    const fs::path&         dirPath,
    const Option<SZString>& fileExtensionFilter,
    const bool              subtractOne
  ) -> Result<u64> {
    std::error_code fsErrCode;

    const SZString cacheKey = SzFormat("{}{}", CACHE_KEY_PREFIX, pmId);

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    fsErrCode.clear();

    if (!fs::is_directory(dirPath, fsErrCode)) {
      if (fsErrCode && fsErrCode != std::errc::no_such_file_or_directory)
        return Err(DracError(
          IoError,
          SzFormat("Filesystem error checking if '{}' is a directory: {}", dirPath.string(), fsErrCode.message())
        ));

      return Err(DracError(NotFound, SzFormat("{} path is not a directory: {}", pmId, dirPath.string())));
    }

    fsErrCode.clear();

    u64                count     = 0;
    const bool         hasFilter = fileExtensionFilter.has_value();
    const SZStringView filter    = fileExtensionFilter ? SZStringView(*fileExtensionFilter) : SZStringView();

    try {
      const fs::directory_iterator dirIter(
        dirPath,
        fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink,
        fsErrCode
      );

      if (fsErrCode)
        return Err(DracError(IoError, SzFormat("Failed to create iterator for {} directory '{}': {}", pmId, dirPath.string(), fsErrCode.message())));

      if (hasFilter) {
        for (const fs::directory_entry& entry : dirIter) {
          if (entry.path().empty())
            continue;

          std::error_code isFileErr;
          if (entry.is_regular_file(isFileErr) && !isFileErr) {
            const SZStringView ext(entry.path().extension().string());
            if (ext == filter)
              count++;
          } else if (isFileErr)
            warn_log(SzFormat("Error stating entry '{}' in {} directory: {}", entry.path().string(), pmId, isFileErr.message()));
        }
      } else {
        for (const fs::directory_entry& entry : dirIter) {
          if (!entry.path().empty())
            count++;
        }
      }
    } catch (const fs::filesystem_error& fsCatchErr) {
      return Err(DracError(
        IoError,
        SzFormat("Filesystem error during {} directory iteration: {}", pmId, fsCatchErr.what())
      ));
    } catch (const Exception& exc) {
      return Err(DracError(InternalError, exc.what()));
    } catch (...) {
      return Err(DracError(Other, SzFormat("Unknown error iterating {} directory", pmId)));
    }

    if (subtractOne && count > 0)
      count--;

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
} // namespace

namespace package {
  fn GetCountFromDirectory(
    const SZString& pmId,
    const fs::path& dirPath,
    const SZString& fileExtensionFilter,
    const bool      subtractOne
  ) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, fileExtensionFilter, subtractOne);
  }

  fn GetCountFromDirectory(const SZString& pmId, const fs::path& dirPath, const SZString& fileExtensionFilter) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, fileExtensionFilter, false);
  }

  fn GetCountFromDirectory(const SZString& pmId, const fs::path& dirPath, const bool subtractOne) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, None, subtractOne);
  }

  fn GetCountFromDirectory(const SZString& pmId, const fs::path& dirPath) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, None, false);
  }

  #if !defined(__serenity__) && !defined(_WIN32)
  fn GetCountFromDb(const SZString& pmId, const fs::path& dbPath, const SZString& countQuery) -> Result<u64> {
    const SZString cacheKey = SzFormat("pkg_count_{}", pmId);

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    u64 count = 0;

    try {
      if (std::error_code existsErr; !fs::exists(dbPath, existsErr) || existsErr)
        return Err(DracError(NotFound, SzFormat("{} database not found at '{}'", pmId, dbPath.string())));

      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);

      if (SQLite::Statement queryStmt(database, countQuery); queryStmt.executeStep()) {
        const i64 countInt64 = queryStmt.getColumn(0).getInt64();

        if (countInt64 < 0)
          return Err(DracError(ParseError, SzFormat("Negative count returned by {} DB COUNT query.", pmId)));

        count = static_cast<u64>(countInt64);
      } else
        return Err(DracError(ParseError, SzFormat("No rows returned by {} DB COUNT query.", pmId)));
    } catch (const SQLite::Exception& e) {
      error_log(SzFormat("SQLite error occurred accessing {} DB '{}': {}", pmId, dbPath.string(), e.what()));

      return Err(DracError(ApiUnavailable, SzFormat("Failed to query {} database: {}", pmId, dbPath.string())));
    } catch (const Exception& e) {
      error_log(SzFormat("Standard exception accessing {} DB '{}': {}", pmId, dbPath.string(), e.what()));

      return Err(DracError(InternalError, e.what()));
    } catch (...) {
      error_log(SzFormat("Unknown error occurred accessing {} DB '{}'", pmId, dbPath.string()));

      return Err(DracError(Other, SzFormat("Unknown error occurred accessing {} DB", pmId)));
    }

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
  #endif // __serenity__ || _WIN32

  #if defined(__linux__) && defined(HAVE_PUGIXML)
  fn GetCountFromPlist(const SZString& pmId, const fs::path& plistPath) -> Result<u64> {
    using pugi::xml_document, pugi::xml_node, pugi::xml_parse_result;

    const SZString cacheKey = SzFormat("pkg_count_{}", pmId);

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    xml_document doc;

    if (const xml_parse_result result = doc.load_file(plistPath.c_str()); !result)
      return Err(DracError(DracErrorCode::ParseError, SzFormat("Failed to parse plist file '{}': {}", plistPath.string(), result.description())));

    const xml_node dict = doc.child("plist").child("dict");

    if (!dict)
      return Err(DracError(DracErrorCode::ParseError, SzFormat("No <dict> in plist file '{}'.", plistPath.string())));

    u64                count           = 0;
    const SZStringView alternativesKey = "_XBPS_ALTERNATIVES_";
    const SZStringView keyName         = "key";
    const SZStringView stateValue      = "installed";

    for (xml_node node = dict.first_child(); node; node = node.next_sibling()) {
      if (SZStringView(node.name()) != keyName)
        continue;

      if (const SZStringView keyName = node.child_value(); keyName == alternativesKey)
        continue;

      xml_node pkgDict = node.next_sibling("dict");

      if (!pkgDict)
        continue;

      bool isInstalled = false;

      for (xml_node pkgNode = pkgDict.first_child(); pkgNode; pkgNode = pkgNode.next_sibling())
        if (SZStringView(pkgNode.name()) == keyName && SZStringView(pkgNode.child_value()) == "state")
          if (xml_node stateValue = pkgNode.next_sibling("string"); stateValue && SZStringView(stateValue.child_value()) == stateValue) {
            isInstalled = true;
            break;
          }

      if (isInstalled)
        ++count;
    }

    if (count == 0)
      return Err(DracError(DracErrorCode::NotFound, SzFormat("No installed packages found in plist file '{}'.", plistPath.string())));

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
  #endif // __linux__

  #if defined(__linux__) || defined(__APPLE__)
  fn CountNix() -> Result<u64> {
    return GetCountFromDb("nix", "/nix/var/nix/db/db.sqlite", "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL");
  }
  #endif // __linux__ || __APPLE__

  fn CountCargo() -> Result<u64> {
    using util::helpers::GetEnv;

    fs::path cargoPath {};

    if (const Result<CStr> cargoHome = GetEnv("CARGO_HOME"))
      cargoPath = fs::path(*cargoHome) / "bin";
    else if (const Result<CStr> homeDir = GetEnv("HOME"))
      cargoPath = fs::path(*homeDir) / ".cargo" / "bin";

    if (cargoPath.empty() || !fs::exists(cargoPath))
      return Err(DracError(NotFound, "Could not find cargo directory"));

    return GetCountFromDirectory("cargo", cargoPath);
  }

  fn GetTotalCount(const Manager enabledPackageManagers) -> Result<u64> {
  #if DRAC_PRECOMPILED_CONFIG
    #if DRAC_ENABLE_PACKAGECOUNT
    Vec<Future<Result<u64>>> futures;
    futures.reserve(16);

    fn addFutureIfEnabled = [&futures, enabledPackageManagers](Manager manager, auto&& countFunc) -> void {
      if (HasPackageManager(enabledPackageManagers, manager))
        futures.emplace_back(std::async(std::launch::async, std::forward<decltype(countFunc)>(countFunc)));
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
      return Err(DracError(NotFound, "No enabled package managers for this platform in precompiled config."));
    #else
    return Err(DracError(NotSupported, "Package counting disabled by precompiled configuration."));
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

    Array<Future<Result<u64>>, numFutures>
      futures = {
        {
    #ifdef __linux__
         std::async(std::launch::async, CountApk),
         std::async(std::launch::async, CountDpkg),
         std::async(std::launch::async, CountMoss),
         std::async(std::launch::async, CountPacman),
         std::async(std::launch::async, CountRpm),
      #ifdef HAVE_PUGIXML
         std::async(std::launch::async, CountXbps),
      #endif
    #elifdef __APPLE__
          std::async(std::launch::async, GetHomebrewCount),
          std::async(std::launch::async, GetMacPortsCount),
    #elifdef _WIN32
          std::async(std::launch::async, CountWinGet),
          std::async(std::launch::async, CountChocolatey),
          std::async(std::launch::async, CountScoop),
    #elif defined(__FreeBSD__) || defined(__DragonFly__)
          std::async(std::launch::async, GetPkgNgCount),
    #elifdef __NetBSD__
          std::async(std::launch::async, GetPkgSrcCount),
    #elifdef __HAIKU__
          std::async(std::launch::async, GetHaikuCount),
    #elifdef __serenity__
          std::async(std::launch::async, GetSerenityCount),
    #endif

    #if defined(__linux__) || defined(__APPLE__)
         std::async(std::launch::async, CountNix),
    #endif

         std::async(std::launch::async, CountCargo),
         }
    };
  #endif // DRAC_PRECOMPILED_CONFIG
    u64  totalCount   = 0;
    bool oneSucceeded = false;

    constexpr size_t chunkSize = 4;

    for (size_t i = 0; i < futures.size(); i += chunkSize) {
      const size_t end = std::min(i + chunkSize, futures.size());

      for (size_t j = i; j < end; ++j) {
        try {
          using matchit::match, matchit::is, matchit::or_, matchit::_;

          if (Result<u64> result = futures[j].get()) {
            totalCount += *result;
            oneSucceeded = true;
          } else
            match(result.error().code)(
              is | or_(NotFound, ApiUnavailable, NotSupported) = [&] -> void { debug_at(result.error()); },
              is | _                                           = [&] -> void { error_at(result.error()); }
            );
        } catch (const Exception& exc) {
          error_log(SzFormat("Caught exception while getting package count future: {}", exc.what()));
        } catch (...) {
          error_log("Caught unknown exception while getting package count future.");
        }
      }
    }

    if (!oneSucceeded && totalCount == 0)
      return Err(DracError(NotFound, "No package managers found or none reported counts."));

    return totalCount;
  }
} // namespace package

#endif // DRAC_ENABLE_PACKAGECOUNT
