#include "package.hpp"

#if !defined(__serenity_) && !defined(_WIN32)
  #include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
  #include <SQLiteCpp/Exception.h> // SQLite::Exception
  #include <SQLiteCpp/Statement.h> // SQLite::Statement
#endif

#ifdef __linux__
  #include <pugixml.hpp>
#endif

#include <chrono>       // std::chrono
#include <filesystem>   // std::filesystem
#include <format>       // std::format
#include <future>       // std::{async, future, launch}
#include <system_error> // std::error_code

#include "src/util/cache.hpp"
#include "src/util/error.hpp"
#include "src/util/helpers.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

namespace fs = std::filesystem;
using namespace std::chrono;
using util::cache::ReadCache, util::cache::WriteCache;
using util::error::DracError, util::error::DracErrorCode;
using util::types::Err, util::types::Exception, util::types::Future, util::types::Result, util::types::String,
  util::types::Vec, util::types::i64, util::types::u64, util::types::Option, util::types::None;

namespace {
  fn GetCountFromDirectoryImpl(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    using package::PkgCountCacheData;

    std::error_code fsErrCode;

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(pmId)) {
      const auto& [cachedCount, timestamp] = *cachedDataResult;

      if (!fs::exists(dirPath, fsErrCode) || fsErrCode)
        warn_log(
          "Error checking existence for directory '{}' before cache validation: {}, Invalidating {} cache",
          dirPath.string(),
          fsErrCode.message(),
          pmId
        );
      else {
        fsErrCode.clear();
        const fs::file_time_type dirModTime = fs::last_write_time(dirPath, fsErrCode);

        if (fsErrCode)
          warn_log(
            "Could not get modification time for directory '{}': {}. Invalidating {} cache",
            dirPath.string(),
            fsErrCode.message(),
            pmId
          );
        else {
          if (const system_clock::time_point cacheTimePoint = system_clock::time_point(seconds(timestamp));
              cacheTimePoint.time_since_epoch() >= dirModTime.time_since_epoch()) {
            debug_log(
              "Using valid {} directory count cache (Dir '{}' unchanged since {}). Count: {}",
              pmId,
              dirPath.string(),
              std::format("{:%F %T %Z}", floor<seconds>(cacheTimePoint)),
              cachedCount
            );
            return cachedCount;
          }
        }
      }
    } else if (cachedDataResult.error().code != DracErrorCode::NotFound) {
      debug_at(cachedDataResult.error());
    } else
      debug_log("{} directory count cache not found or unreadable", pmId, pmId);

    fsErrCode.clear();

    if (!fs::is_directory(dirPath, fsErrCode)) {
      if (fsErrCode && fsErrCode != std::errc::no_such_file_or_directory)
        return Err(DracError(
          DracErrorCode::IoError,
          std::format("Filesystem error checking if '{}' is a directory: {}", dirPath.string(), fsErrCode.message())
        ));

      return Err(
        DracError(DracErrorCode::NotFound, std::format("{} path is not a directory: {}", pmId, dirPath.string()))
      );
    }

    fsErrCode.clear();

    u64 count = 0;

    try {
      const fs::directory_iterator dirIter(dirPath, fs::directory_options::skip_permission_denied, fsErrCode);

      if (fsErrCode)
        return Err(DracError(
          DracErrorCode::IoError,
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
        DracErrorCode::IoError,
        std::format("Filesystem error during {} directory iteration: {}", pmId, fsCatchErr.what())
      ));
    } catch (const Exception& exc) { return Err(DracError(DracErrorCode::InternalError, exc.what())); } catch (...) {
      return Err(DracError(DracErrorCode::Other, std::format("Unknown error iterating {} directory", pmId)));
    }

    if (subtractOne && count > 0)
      count--;

    if (count == 0)
      return Err(DracError(DracErrorCode::NotFound, std::format("No packages found in {} directory", pmId)));

    const i64 timestampEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    const PkgCountCacheData dataToCache(count, timestampEpochSeconds);

    if (Result writeResult = WriteCache(pmId, dataToCache); !writeResult)
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
  fn GetCountFromDb(const PackageManagerInfo& pmInfo) -> Result<u64> {
    const auto& [pmId, dbPath, countQuery] = pmInfo;
    const String cacheKey                  = "pkg_count_" + pmId; // More specific cache key

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [count, timestamp] = *cachedDataResult;
      std::error_code                       errc;
      const std::filesystem::file_time_type dbModTime = fs::last_write_time(dbPath, errc);

      if (errc) {
        warn_log(
          "Could not get modification time for '{}': {}. Invalidating {} cache.", dbPath.string(), errc.message(), pmId
        );
      } else {
        if (const system_clock::time_point cacheTimePoint = system_clock::time_point(seconds(timestamp));
            cacheTimePoint.time_since_epoch() >= dbModTime.time_since_epoch()) {
          debug_log(
            "Using valid {} package count cache (DB file unchanged since {}). Count: {}",
            pmId,
            std::format("{:%F %T %Z}", floor<seconds>(cacheTimePoint)),
            count
          );
          return count;
        }
        debug_log("{} package count cache stale (DB file modified).", pmId);
      }
    } else {
      if (cachedDataResult.error().code != DracErrorCode::NotFound)
        debug_at(cachedDataResult.error());
      debug_log("{} package count cache not found or unreadable.", pmId);
    }

    debug_log("Fetching fresh {} package count from database: {}", pmId, dbPath.string());
    u64 count = 0;

    try {
      std::error_code existsErr;
      if (!fs::exists(dbPath, existsErr) || existsErr) {
        if (existsErr) {
          warn_log("Error checking existence of {} DB '{}': {}", pmId, dbPath.string(), existsErr.message());
        }
        return Err(
          DracError(DracErrorCode::NotFound, std::format("{} database not found at '{}'", pmId, dbPath.string()))
        );
      }

      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);
      SQLite::Statement      queryStmt(database, countQuery); // Use query directly

      if (queryStmt.executeStep()) {
        const i64 countInt64 = queryStmt.getColumn(0).getInt64();
        if (countInt64 < 0)
          return Err(
            DracError(DracErrorCode::ParseError, std::format("Negative count returned by {} DB COUNT query.", pmId))
          );
        count = static_cast<u64>(countInt64);
      } else
        return Err(DracError(DracErrorCode::ParseError, std::format("No rows returned by {} DB COUNT query.", pmId)));
    } catch (const SQLite::Exception& e) {
      error_log("SQLite error occurred accessing {} DB '{}': {}", pmId, dbPath.string(), e.what());
      return Err(
        DracError(DracErrorCode::ApiUnavailable, std::format("Failed to query {} database: {}", pmId, dbPath.string()))
      );
    } catch (const Exception& e) {
      error_log("Standard exception accessing {} DB '{}': {}", pmId, dbPath.string(), e.what());
      return Err(DracError(DracErrorCode::InternalError, e.what()));
    } catch (...) {
      error_log("Unknown error occurred accessing {} DB '{}'", pmId, dbPath.string());
      return Err(DracError(DracErrorCode::Other, std::format("Unknown error occurred accessing {} DB", pmId)));
    }

    debug_log("Successfully fetched {} package count: {}.", pmId, count);

    const i64 timestampEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    const PkgCountCacheData dataToCache(count, timestampEpochSeconds);

    if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
#endif // __serenity__ || _WIN32

#ifdef __linux__
  fn GetCountFromPlist(const String& pmId, const std::filesystem::path& plistPath) -> Result<u64> {
    using namespace pugi;
    using util::types::StringView;

    const String    cacheKey = "pkg_count_" + pmId;
    std::error_code fsErrCode;

    // Cache check
    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [cachedCount, timestamp] = *cachedDataResult;
      if (fs::exists(plistPath, fsErrCode) && !fsErrCode) {
        const fs::file_time_type plistModTime = fs::last_write_time(plistPath, fsErrCode);
        if (!fsErrCode) {
          if (const std::chrono::system_clock::time_point cacheTimePoint = std::chrono::system_clock::time_point(std::chrono::seconds(timestamp));
              cacheTimePoint.time_since_epoch() >= plistModTime.time_since_epoch()) {
            debug_log("Using valid {} plist count cache (file '{}' unchanged since {}). Count: {}", pmId, plistPath.string(), std::format("{:%F %T %Z}", std::chrono::floor<std::chrono::seconds>(cacheTimePoint)), cachedCount);
            return cachedCount;
          }
        } else {
          warn_log("Could not get modification time for '{}': {}. Invalidating {} cache.", plistPath.string(), fsErrCode.message(), pmId);
        }
      }
    } else if (cachedDataResult.error().code != DracErrorCode::NotFound) {
      debug_at(cachedDataResult.error());
    } else {
      debug_log("{} plist count cache not found or unreadable", pmId);
    }

    // Parse plist and count
    xml_document     doc;
    xml_parse_result result = doc.load_file(plistPath.c_str());

    if (!result)
      return Err(util::error::DracError(util::error::DracErrorCode::ParseError, std::format("Failed to parse plist file '{}': {}", plistPath.string(), result.description())));

    xml_node dict = doc.child("plist").child("dict");

    if (!dict)
      return Err(util::error::DracError(util::error::DracErrorCode::ParseError, std::format("No <dict> in plist file '{}'.", plistPath.string())));

    u64 count = 0;

    for (xml_node node = dict.first_child(); node; node = node.next_sibling()) {
      if (StringView(node.name()) != "key")
        continue;

      const StringView keyName = node.child_value();

      if (keyName == "_XBPS_ALTERNATIVES_")
        continue;

      xml_node pkgDict = node.next_sibling("dict");

      if (!pkgDict)
        continue;

      bool isInstalled = false;

      for (xml_node pkgNode = pkgDict.first_child(); pkgNode; pkgNode = pkgNode.next_sibling())
        if (StringView(pkgNode.name()) == "key" && StringView(pkgNode.child_value()) == "state") {
          xml_node stateValue = pkgNode.next_sibling("string");
          if (stateValue && StringView(stateValue.child_value()) == "installed") {
            isInstalled = true;
            break;
          }
        }

      if (isInstalled)
        ++count;
    }

    if (count == 0)
      return Err(util::error::DracError(util::error::DracErrorCode::NotFound, std::format("No installed packages found in plist file '{}'.", plistPath.string())));

    const i64               timestampEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const PkgCountCacheData dataToCache(count, timestampEpochSeconds);
    if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
      debug_at(writeResult.error());
    return count;
  }
#endif // __linux__

#if defined(__linux__) || defined(__APPLE__)
  fn GetNixCount() -> Result<u64> {
    const PackageManagerInfo nixInfo = {
      .id         = "nix",
      .dbPath     = "/nix/var/nix/db/db.sqlite",
      .countQuery = "SELECT COUNT(path) FROM ValidPaths WHERE sigs IS NOT NULL",
    };

    if (std::error_code errc; !fs::exists(nixInfo.dbPath, errc)) {
      if (errc) {
        warn_log("Filesystem error checking for Nix DB at '{}': {}", nixInfo.dbPath.string(), errc.message());
        return Err(DracError(DracErrorCode::IoError, "Filesystem error checking Nix DB: " + errc.message()));
      }

      return Err(DracError(DracErrorCode::ApiUnavailable, "Nix db not found: " + nixInfo.dbPath.string()));
    }

    return GetCountFromDb(nixInfo);
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
      return Err(DracError(DracErrorCode::NotFound, "Could not find cargo directory"));

    return GetCountFromDirectory("cargo", cargoPath);
  }

  fn GetTotalCount() -> Result<u64> {
    Vec<Future<Result<u64>>> futures;

#ifdef __linux__
    // futures.push_back(std::async(std::launch::async, GetApkCount));
    futures.push_back(std::async(std::launch::async, GetDpkgCount));
    futures.push_back(std::async(std::launch::async, GetMossCount));
    futures.push_back(std::async(std::launch::async, GetPacmanCount));
    // futures.push_back(std::async(std::launch::async, GetPortageCount));
    futures.push_back(std::async(std::launch::async, GetRpmCount));
    futures.push_back(std::async(std::launch::async, GetXbpsCount));
    // futures.push_back(std::async(std::launch::async, GetZypperCount));
#elifdef __APPLE__
    futures.push_back(std::async(std::launch::async, GetHomebrewCount));
    futures.push_back(std::async(std::launch::async, GetMacPortsCount));
#elifdef _WIN32
    futures.push_back(std::async(std::launch::async, CountWinGet));
    futures.push_back(std::async(std::launch::async, CountChocolatey));
    futures.push_back(std::async(std::launch::async, CountScoop));
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    futures.push_back(std::async(std::launch::async, GetPkgNgCount));
#elifdef __NetBSD__
    futures.push_back(std::async(std::launch::async, GetPkgSrcCount));
#elifdef __HAIKU__
    futures.push_back(std::async(std::launch::async, GetHaikuCount));
#elifdef __serenity__
    futures.push_back(std::async(std::launch::async, GetSerenityCount));
#endif

#if defined(__linux__) || defined(__APPLE__)
    futures.push_back(std::async(std::launch::async, GetNixCount));
#endif
    futures.push_back(std::async(std::launch::async, CountCargo));

    u64  totalCount   = 0;
    bool oneSucceeded = false;

    for (Future<Result<u64>>& fut : futures) {
      try {
        using enum util::error::DracErrorCode;

        if (Result<u64> result = fut.get()) {
          totalCount += *result;
          oneSucceeded = true;
          debug_log("Added {} packages. Current total: {}", *result, totalCount);
        } else if (result.error().code != NotFound && result.error().code != ApiUnavailable &&
                   result.error().code != NotSupported) {
          error_at(result.error());
        } else
          debug_at(result.error());
      } catch (const Exception& exc) {
        error_log("Caught exception while getting package count future: {}", exc.what());
      } catch (...) { error_log("Caught unknown exception while getting package count future."); }
    }

    if (!oneSucceeded && totalCount == 0)
      return Err(DracError(DracErrorCode::NotFound, "No package managers found or none reported counts."));

    debug_log("Final total package count: {}", totalCount);
    return totalCount;
  }
} // namespace package
