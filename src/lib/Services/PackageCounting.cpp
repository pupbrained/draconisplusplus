#if DRAC_ENABLE_PACKAGECOUNT

// clang-format off
#include "PackageCounting.hpp"

#if DRAC_PRECOMPILED_CONFIG
  #include "config.hpp"
#endif

#if !defined(__serenity__) && !defined(_WIN32)
  #include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
  #include <SQLiteCpp/Exception.h> // SQLite::Exception
  #include <SQLiteCpp/Statement.h> // SQLite::Statement
#endif

#if defined(__linux__) && defined(HAVE_PUGIXML)
  #include <pugixml.hpp> // pugi::{xml_document, xml_node, xml_parse_result}
#endif

#include <filesystem>   // std::filesystem
#include <format>       // std::format
#include <future>       // std::{async, future, launch}
#include <matchit.hpp>  // matchit::{match, is, or_, _}
#include <system_error> // std::{errc, error_code}

#include "Util/Caching.hpp"
#include "Util/Env.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"
// clang-format on

namespace fs = std::filesystem;

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;
using util::cache::GetValidCache, util::cache::WriteCache;

namespace {

  fn GetCountFromDirectoryImpl(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    std::error_code fsErrCode;

    const String cacheKey = std::format("pkg_count_{}", pmId);

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    fsErrCode.clear();

    if (!fs::is_directory(dirPath, fsErrCode)) {
      if (fsErrCode && fsErrCode != std::errc::no_such_file_or_directory)
        return Err(DracError(
          IoError,
          std::format("Filesystem error checking if '{}' is a directory: {}", dirPath.string(), fsErrCode.message())
        ));

      return Err(DracError(NotFound, std::format("{} path is not a directory: {}", pmId, dirPath.string())));
    }

    fsErrCode.clear();

    u64 count = 0;

    try {
      const fs::directory_iterator dirIter(dirPath, fs::directory_options::skip_permission_denied, fsErrCode);

      if (fsErrCode)
        return Err(DracError(
          IoError,
          std::format(
            "Failed to create iterator for {} directory '{}': {}", pmId, dirPath.string(), fsErrCode.message()
          )
        ));

      for (const fs::directory_entry& entry : dirIter) {
        fsErrCode.clear();

        if (entry.path().empty())
          continue;

        if (fileExtensionFilter) {
          bool isFile = false;
          isFile      = entry.is_regular_file(fsErrCode);

          if (fsErrCode) {
            warn_log("Error stating entry '{}' in {} directory: {}", entry.path().string(), pmId, fsErrCode.message());
            continue;
          }

          if (isFile && entry.path().extension().string() == *fileExtensionFilter)
            count++;

          continue;
        }

        if (!fileExtensionFilter)
          count++;
      }
    } catch (const fs::filesystem_error& fsCatchErr) {
      return Err(DracError(
        IoError,
        std::format("Filesystem error during {} directory iteration: {}", pmId, fsCatchErr.what())
      ));
    } catch (const Exception& exc) { return Err(DracError(InternalError, exc.what())); } catch (...) {
      return Err(DracError(Other, std::format("Unknown error iterating {} directory", pmId)));
    }

    if (subtractOne && count > 0)
      count--;

    if (count == 0)
      return Err(DracError(NotFound, std::format("No packages found in {} directory", pmId)));

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
} // namespace

namespace package {
  fn GetCountFromDirectory(
    const String&   pmId,
    const fs::path& dirPath,
    const String&   fileExtensionFilter,
    const bool      subtractOne
  ) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, fileExtensionFilter, subtractOne);
  }

  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, const String& fileExtensionFilter)
    -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, fileExtensionFilter, false);
  }

  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath, const bool subtractOne) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, None, subtractOne);
  }

  fn GetCountFromDirectory(const String& pmId, const fs::path& dirPath) -> Result<u64> {
    return GetCountFromDirectoryImpl(pmId, dirPath, None, false);
  }

  #if !defined(__serenity__) && !defined(_WIN32)
  fn GetCountFromDb(const String& pmId, const fs::path& dbPath, const String& countQuery) -> Result<u64> {
    const String cacheKey = std::format("pkg_count_{}", pmId);

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    u64 count = 0;

    try {
      if (std::error_code existsErr; !fs::exists(dbPath, existsErr) || existsErr)
        return Err(DracError(NotFound, std::format("{} database not found at '{}'", pmId, dbPath.string())));

      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);

      if (SQLite::Statement queryStmt(database, countQuery); queryStmt.executeStep()) {
        const i64 countInt64 = queryStmt.getColumn(0).getInt64();

        if (countInt64 < 0)
          return Err(DracError(ParseError, std::format("Negative count returned by {} DB COUNT query.", pmId)));

        count = static_cast<u64>(countInt64);
      } else
        return Err(DracError(ParseError, std::format("No rows returned by {} DB COUNT query.", pmId)));
    } catch (const SQLite::Exception& e) {
      error_log("SQLite error occurred accessing {} DB '{}': {}", pmId, dbPath.string(), e.what());

      return Err(DracError(ApiUnavailable, std::format("Failed to query {} database: {}", pmId, dbPath.string())));
    } catch (const Exception& e) {
      error_log("Standard exception accessing {} DB '{}': {}", pmId, dbPath.string(), e.what());

      return Err(DracError(InternalError, e.what()));
    } catch (...) {
      error_log("Unknown error occurred accessing {} DB '{}'", pmId, dbPath.string());

      return Err(DracError(Other, std::format("Unknown error occurred accessing {} DB", pmId)));
    }

    if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
  #endif // __serenity__ || _WIN32

  #if defined(__linux__) && defined(HAVE_PUGIXML)
  fn GetCountFromPlist(const String& pmId, const fs::path& plistPath) -> Result<u64> {
    using pugi::xml_document, pugi::xml_node, pugi::xml_parse_result;

    const String cacheKey = "pkg_count_" + pmId;

    if (Result<u64> cachedDataResult = GetValidCache<u64>(cacheKey))
      return *cachedDataResult;
    else
      debug_at(cachedDataResult.error());

    xml_document doc;

    if (const xml_parse_result result = doc.load_file(plistPath.c_str()); !result)
      return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse plist file '{}': {}", plistPath.string(), result.description())));

    const xml_node dict = doc.child("plist").child("dict");

    if (!dict)
      return Err(DracError(DracErrorCode::ParseError, std::format("No <dict> in plist file '{}'.", plistPath.string())));

    u64 count = 0;

    for (xml_node node = dict.first_child(); node; node = node.next_sibling()) {
      if (StringView(node.name()) != "key")
        continue;

      if (const StringView keyName = node.child_value(); keyName == "_XBPS_ALTERNATIVES_")
        continue;

      xml_node pkgDict = node.next_sibling("dict");

      if (!pkgDict)
        continue;

      bool isInstalled = false;

      for (xml_node pkgNode = pkgDict.first_child(); pkgNode; pkgNode = pkgNode.next_sibling())
        if (StringView(pkgNode.name()) == "key" && StringView(pkgNode.child_value()) == "state")
          if (xml_node stateValue = pkgNode.next_sibling("string"); stateValue && StringView(stateValue.child_value()) == "installed") {
            isInstalled = true;
            break;
          }

      if (isInstalled)
        ++count;
    }

    if (count == 0)
      return Err(DracError(DracErrorCode::NotFound, std::format("No installed packages found in plist file '{}'.", plistPath.string())));

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

    if (const Result<String> cargoHome = GetEnv("CARGO_HOME"))
      cargoPath = fs::path(*cargoHome) / "bin";
    else if (const Result<String> homeDir = GetEnv("HOME"))
      cargoPath = fs::path(*homeDir) / ".cargo" / "bin";

    if (cargoPath.empty() || !fs::exists(cargoPath))
      return Err(DracError(NotFound, "Could not find cargo directory"));

    return GetCountFromDirectory("cargo", cargoPath);
  }

  fn GetTotalCount() -> Result<u64> {
  #if DRAC_PRECOMPILED_CONFIG
    #if DRAC_ENABLE_PACKAGECOUNT
    Vec<Future<Result<u64>>> futures;
    futures.reserve(8); // Reserve a reasonable amount of space

      // Platform-specific package managers
      #ifdef __linux__
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::APK))
      futures.emplace_back(std::async(std::launch::async, CountApk));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::DPKG))
      futures.emplace_back(std::async(std::launch::async, CountDpkg));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::MOSS))
      futures.emplace_back(std::async(std::launch::async, CountMoss));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::PACMAN))
      futures.emplace_back(std::async(std::launch::async, CountPacman));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::RPM))
      futures.emplace_back(std::async(std::launch::async, CountRpm));
        #ifdef HAVE_PUGIXML
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::XBPS))
      futures.emplace_back(std::async(std::launch::async, CountXbps));
        #endif
      #elifdef __APPLE__
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::HOMEBREW))
      futures.emplace_back(std::async(std::launch::async, GetHomebrewCount));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::MACPORTS))
      futures.emplace_back(std::async(std::launch::async, GetMacPortsCount));
      #elifdef _WIN32
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::WINGET))
      futures.emplace_back(std::async(std::launch::async, CountWinGet));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::CHOCOLATEY))
      futures.emplace_back(std::async(std::launch::async, CountChocolatey));
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::SCOOP))
      futures.emplace_back(std::async(std::launch::async, CountScoop));
      #elif defined(__FreeBSD__) || defined(__DragonFly__)
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::PKGNG))
      futures.emplace_back(std::async(std::launch::async, GetPkgNgCount));
      #elifdef __NetBSD__
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::PKGSRC))
      futures.emplace_back(std::async(std::launch::async, GetPkgSrcCount));
      #elifdef __HAIKU__
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::HAIKUPKG))
      futures.emplace_back(std::async(std::launch::async, GetHaikuCount));
      #elifdef __serenity__
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::SERENITY))
      futures.emplace_back(std::async(std::launch::async, GetSerenityCount));
      #endif

      // Cross-platform package managers
      #if defined(__linux__) || defined(__APPLE__) // Nix support
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::NIX))
      futures.emplace_back(std::async(std::launch::async, CountNix));
      #endif
    if (HasPackageManager(config::DRAC_ENABLED_PACKAGE_MANAGERS, config::PackageManager::CARGO))
      futures.emplace_back(std::async(std::launch::async, CountCargo));

    if (futures.empty())
      return Err(DracError(NotFound, "No enabled package managers for this platform in precompiled config."));
    #else // DRAC_ENABLE_PACKAGECOUNT is false
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

    for (Future<Result<u64>>& fut : futures) {
      try {
        using matchit::match, matchit::is, matchit::or_, matchit::_;

        if (Result<u64> result = fut.get()) {
          totalCount += *result;
          oneSucceeded = true;
        } else
          match(result.error().code)(
            is | or_(NotFound, ApiUnavailable, NotSupported) = [&] -> void { debug_at(result.error()); },
            is | _                                           = [&] -> void { error_at(result.error()); }
          );
      } catch (const Exception& exc) {
        error_log("Caught exception while getting package count future: {}", exc.what());
      } catch (...) { error_log("Caught unknown exception while getting package count future."); }
    }

    if (!oneSucceeded && totalCount == 0)
      return Err(DracError(NotFound, "No package managers found or none reported counts."));

    return totalCount;
  }
} // namespace package

#endif // DRAC_ENABLE_PACKAGECOUNT
