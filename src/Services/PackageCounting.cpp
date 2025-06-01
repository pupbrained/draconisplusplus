#include "PackageCounting.hpp"

#if !defined(__serenity__) && !defined(_WIN32)
  #include <SQLiteCpp/Database.h>  // SQLite::{Database, OPEN_READONLY}
  #include <SQLiteCpp/Exception.h> // SQLite::Exception
  #include <SQLiteCpp/Statement.h> // SQLite::Statement
#endif

#if defined(__linux__) && defined(HAVE_PUGIXML)
  #include <pugixml.hpp> // pugi::{xml_document, xml_node, xml_parse_result}
#endif

#include <chrono>       // std::chrono
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

namespace {
  namespace fs = std::filesystem;
  using std::chrono::system_clock, std::chrono::seconds, std::chrono::hours, std::chrono::duration_cast;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::error::DracError, util::error::DracErrorCode;

  constexpr auto CACHE_EXPIRY_DURATION = std::chrono::hours(24);
  using util::types::Err, util::types::Exception, util::types::Result, util::types::String, util::types::u64, util::types::i64, util::types::Option;

  fn GetCountFromDirectoryImpl(
    const String&         pmId,
    const fs::path&       dirPath,
    const Option<String>& fileExtensionFilter,
    const bool            subtractOne
  ) -> Result<u64> {
    using package::PkgCountCacheData;

    std::error_code fsErrCode;

    const String cacheKey = std::format("pkg_count_{}", pmId);

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [cachedCount, timestamp] = *cachedDataResult;
      const auto cacheTimePoint            = system_clock::time_point(seconds(timestamp));

      if ((system_clock::now() - cacheTimePoint) < CACHE_EXPIRY_DURATION) {
        return cachedCount; // Cache is valid and not expired
      }
      // Cache expired, fall through to recalculate
    } else { // ReadCache failed
      if (cachedDataResult.error().code != DracErrorCode::NotFound) {
        // Log error if ReadCache failed for a reason other than NotFound
        debug_at(cachedDataResult.error());
      }
      // Fall through to recalculate for NotFound or after logging other errors
    }

    fsErrCode.clear();

    if (!fs::is_directory(dirPath, fsErrCode)) {
      if (fsErrCode && fsErrCode != std::errc::no_such_file_or_directory)
        return Err(DracError(
          DracErrorCode::IoError,
          std::format("Filesystem error checking if '{}' is a directory: {}", dirPath.string(), fsErrCode.message())
        ));

      return Err(DracError(DracErrorCode::NotFound, std::format("{} path is not a directory: {}", pmId, dirPath.string())));
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

    if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
} // namespace

namespace package {
  namespace fs = std::filesystem;
  using util::types::Err, util::types::None, util::types::Option, util::types::Result, util::types::String, util::types::u64;

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
    using util::cache::ReadCache, util::cache::WriteCache;
    using util::error::DracError, util::error::DracErrorCode;
    using util::types::Exception, util::types::i64;

    const String cacheKey = std::format("pkg_count_{}", pmId);

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [cachedDbCount, timestamp] = *cachedDataResult;
      const auto cacheTimePoint              = system_clock::time_point(seconds(timestamp));

      if ((system_clock::now() - cacheTimePoint) < CACHE_EXPIRY_DURATION) {
        return cachedDbCount; // Cache is valid and not expired
      }
      // Cache expired, fall through to recalculate
    } else { // ReadCache failed
      if (cachedDataResult.error().code != DracErrorCode::NotFound) {
        // Log error if ReadCache failed for a reason other than NotFound
        debug_at(cachedDataResult.error());
      }
      // Fall through to recalculate for NotFound or after logging other errors
    }

    u64 count = 0;

    try {
      if (std::error_code existsErr; !fs::exists(dbPath, existsErr) || existsErr)
        return Err(
          DracError(DracErrorCode::NotFound, std::format("{} database not found at '{}'", pmId, dbPath.string()))
        );

      const SQLite::Database database(dbPath.string(), SQLite::OPEN_READONLY);

      if (SQLite::Statement queryStmt(database, countQuery); queryStmt.executeStep()) {
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

    const i64 timestampEpochSeconds = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    const PkgCountCacheData dataToCache(count, timestampEpochSeconds);

    if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
      debug_at(writeResult.error());

    return count;
  }
#endif // __serenity__ || _WIN32

#if defined(__linux__) && defined(HAVE_PUGIXML)
  fn GetCountFromPlist(const String& pmId, const fs::path& plistPath) -> Result<u64> {
    using pugi::xml_document, pugi::xml_node, pugi::xml_parse_result;
    using util::cache::ReadCache, util::cache::WriteCache;
    using util::error::DracError, util::error::DracErrorCode;
    using util::types::i64, util::types::StringView;

    const String cacheKey = "pkg_count_" + pmId;

    if (Result<PkgCountCacheData> cachedDataResult = ReadCache<PkgCountCacheData>(cacheKey)) {
      const auto& [cachedPlistCount, timestamp] = *cachedDataResult;
      const auto cacheTimePoint                 = system_clock::time_point(seconds(timestamp));

      if ((system_clock::now() - cacheTimePoint) < CACHE_EXPIRY_DURATION) {
        return cachedPlistCount; // Cache is valid and not expired
      }
      // Cache expired, fall through to recalculate
    } else { // ReadCache failed
      if (cachedDataResult.error().code != DracErrorCode::NotFound) {
        // Log error if ReadCache failed for a reason other than NotFound
        debug_at(cachedDataResult.error());
      }
      // Fall through to recalculate for NotFound or after logging other errors
    }

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

    const i64               timestampEpochSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const PkgCountCacheData dataToCache(count, timestampEpochSeconds);

    if (Result writeResult = WriteCache(cacheKey, dataToCache); !writeResult)
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
    using util::error::DracError, util::error::DracErrorCode;
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
    using util::error::DracError;
    using util::types::Array, util::types::Exception, util::types::Future;

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

    u64  totalCount   = 0;
    bool oneSucceeded = false;

    for (Future<Result<u64>>& fut : futures) {
      try {
        using matchit::match, matchit::is, matchit::or_, matchit::_;
        using enum util::error::DracErrorCode;

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
      return Err(DracError(DracErrorCode::NotFound, "No package managers found or none reported counts."));

    return totalCount;
  }
} // namespace package
