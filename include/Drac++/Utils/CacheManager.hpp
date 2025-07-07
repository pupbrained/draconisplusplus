#pragma once

#include <chrono>
#include <filesystem>
#include <glaze/glaze.hpp>

#include "DataTypes.hpp"
#include "Env.hpp"
#include "Logging.hpp"

namespace draconis::utils::cache {
  namespace {
    using types::Fn;
    using types::LockGuard;
    using types::Mutex;
    using types::None;
    using types::Option;
    using types::Pair;
    using types::Result;
    using types::Some;
    using types::String;
    using types::u64;
    using types::u8;
    using types::UniquePointer;
    using types::Unit;
    using types::UnorderedMap;
    using types::Vec;

    using std::chrono::days;
    using std::chrono::duration_cast;
    using std::chrono::seconds;
    using std::chrono::system_clock;

    namespace fs = std::filesystem;
  } // namespace

  enum class CacheLocation : u8 {
    InMemory,      ///< Volatile, lost on app exit. Fastest.
    TempDirectory, ///< Persists until next reboot or system cleanup.
    Persistent     ///< Stored in a user-level cache dir (e.g., ~/.cache).
  };

  struct CachePolicy {
    CacheLocation location = CacheLocation::Persistent;

    Option<seconds> ttl = days(1); ///< Default to 1 day.

    static fn inMemory() -> CachePolicy {
      return { .location = CacheLocation::InMemory, .ttl = None };
    }

    static fn neverExpire() -> CachePolicy {
      return { .location = CacheLocation::Persistent, .ttl = None };
    }

    static fn tempDirectory() -> CachePolicy {
      return { .location = CacheLocation::TempDirectory, .ttl = None };
    }
  };

  class CacheManager {
   public:
    /*!
     * @brief Global flag to bypass all caching logic at runtime.
     *
     * When set to true, getOrSet() will skip both reading from and writing to
     * any cache (in-memory or on-disk) and will directly invoke the provided
     * fetcher each time. This makes it easy for the CLI to offer a
     * "--ignore-cache" option without having to modify every call-site.
     */
    static inline bool ignoreCache = false;

    CacheManager() : m_globalPolicy { .location = CacheLocation::Persistent, .ttl = days(1) } {}

    fn setGlobalPolicy(const CachePolicy& policy) -> Unit {
      LockGuard lock(m_cacheMutex);
      m_globalPolicy = policy;
    }

    template <typename T>
    struct CacheEntry {
      T           data;
      Option<u64> expires; // store as UNIX timestamp (seconds since epoch), None if no expiry
    };

    template <typename T>
    fn getOrSet(
      const String&       key,
      Option<CachePolicy> overridePolicy,
      Fn<Result<T>()>     fetcher
    ) -> Result<T> {
#ifdef DRAC_ENABLE_CACHING
      /* Early-exit if caching is globally disabled for this run. */
      if (ignoreCache)
        return fetcher();

      LockGuard lock(m_cacheMutex);

      const CachePolicy& policy = overridePolicy.value_or(m_globalPolicy);

      // 1. Check in-memory cache
      if (const auto iter = m_inMemoryCache.find(key); iter != m_inMemoryCache.end())
        if (
          CacheEntry<T> entry; glz::read_beve(entry, iter->second.first) == glz::error_code::none &&
          (!entry.expires.has_value() || system_clock::now() < system_clock::time_point(seconds(*entry.expires)))
        )
          return entry.data;

      // 2. Check filesystem cache
      const Option<fs::path> filePath = getCacheFilePath(key, policy.location);

      if (filePath && fs::exists(*filePath)) {
        if (std::ifstream ifs(*filePath, std::ios::binary); ifs) {
          std::string fileContents((std::istreambuf_iterator<char>(ifs)), {});

          CacheEntry<T> entry;

          if (glz::read_beve(entry, fileContents) == glz::error_code::none) {
            if (!entry.expires.has_value() || system_clock::now() < system_clock::time_point(seconds(*entry.expires))) {
              system_clock::time_point expiryTp = entry.expires.has_value() ? system_clock::time_point(seconds(*entry.expires)) : system_clock::time_point::max();

              m_inMemoryCache[key] = { fileContents, expiryTp };

              return entry.data;
            }
          }
        }
      }

      // 3. Cache miss: call fetcher (move the callable to indicate consumption)
      Result<T> fetchedResult = fetcher();

      if (!fetchedResult)
        return fetchedResult;

      // 4. Store in cache
      Option<u64> expiryTs;
      if (policy.ttl.has_value()) {
        system_clock::time_point now        = system_clock::now();
        system_clock::time_point expiryTime = now + *policy.ttl;

        expiryTs = duration_cast<seconds>(expiryTime.time_since_epoch()).count();
      }

      CacheEntry<T> newEntry {
        .data    = *fetchedResult,
        .expires = expiryTs
      };

      std::string binaryBuffer;
      glz::write_beve(newEntry, binaryBuffer);

      system_clock::time_point inMemoryExpiryTp = expiryTs.has_value()
        ? system_clock::time_point(seconds(*expiryTs))
        : system_clock::time_point::max();

      m_inMemoryCache[key] = { binaryBuffer, inMemoryExpiryTp };

      if (policy.location != CacheLocation::InMemory) {
        fs::create_directories(filePath->parent_path());
        std::ofstream ofs(*filePath, std::ios::binary | std::ios::trunc);
        ofs.write(binaryBuffer.data(), static_cast<std::streamsize>(binaryBuffer.size()));
      }

      return fetchedResult;
#else
      (void)key;
      (void)overridePolicy;
      return fetcher();
#endif
    }

    template <typename T>
    fn getOrSet(const String& key, Fn<Result<T>()> fetcher) -> Result<T> {
      return getOrSet(key, None, fetcher);
    }

    /**
     * @brief Remove a cached entry corresponding to the given key.
     *
     * This erases the entry from the in-memory cache and also attempts to
     * remove any corresponding files in the temporary and persistent cache
     * locations (if they exist).
     *
     * @param key Cache key to invalidate.
     */
    fn invalidate(const String& key) -> Unit {
#ifdef DRAC_ENABLE_CACHING
      LockGuard lock(m_cacheMutex);

      // Erase from in-memory cache (no harm if the key is absent).
      m_inMemoryCache.erase(key);

      // Attempt to remove the on-disk copies for both possible locations.
      for (const CacheLocation loc : { CacheLocation::TempDirectory, CacheLocation::Persistent })
        if (const Option<fs::path> filePath = getCacheFilePath(key, loc); filePath && fs::exists(*filePath)) {
          std::error_code errc;
          fs::remove(*filePath, errc);
        }
#else
      (void)key;
#endif
    }

    /**
     * @brief Remove **all** cached data – both in-memory and on-disk.
     *
     * This clears the in-memory cache map and removes all files from the
     * persistent and temporary cache directories while preserving the
     * directory structure.
     */
    fn invalidateAll(bool logRemovals = false) -> u8 {
#ifdef DRAC_ENABLE_CACHING
      LockGuard lock(m_cacheMutex);

      u8 removedCount = 0;

      // Record keys currently present so we can clean their temp-dir copies
      // after we clear the map.
      Vec<String> keys;
      keys.reserve(m_inMemoryCache.size());
      for (const auto& [key, val] : m_inMemoryCache)
        keys.emplace_back(key);

      // Clear in-memory cache.
      m_inMemoryCache.clear();

      // Remove all files from persistent cache directory.
  #ifdef __APPLE__
      const fs::path persistentDir = std::format("{}/Library/Caches/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."));
  #else
      const fs::path persistentDir = std::format("{}/.cache/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."));
  #endif

      if (fs::exists(persistentDir)) {
        std::error_code errc;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(persistentDir, errc))
          if (entry.is_regular_file()) {
            fs::remove(entry.path(), errc);
            removedCount++;
            if (logRemovals)
              logging::Println("Removed persistent cache file: {}", entry.path().string());
          }
      }

      // Remove all files from temp directory that match our cache pattern.
      const fs::path tempDir = fs::temp_directory_path();
      if (fs::exists(tempDir)) {
        std::error_code errc;
        for (const fs::directory_entry& entry : fs::directory_iterator(tempDir, errc)) {
          if (entry.is_regular_file()) {
            // Check if this file is one of our cache files by looking at known keys
            // or by checking if it's a file we might have created
            bool shouldRemove = false;

            // Remove files that match our known keys
            for (const String& key : keys)
              if (entry.path().filename() == key) {
                shouldRemove = true;
                break;
              }

            // Also remove any files that might be orphaned cache files
            // (files that don't have extensions and are likely our cache files)
            if (!shouldRemove && entry.path().extension().empty())
              shouldRemove = true;

            if (shouldRemove) {
              fs::remove(entry.path(), errc);
              removedCount++;
              if (logRemovals)
                logging::Println("Removed temp-directory cache file: {}", entry.path().string());
            }
          }
        }
      }

      return removedCount;
#else
      (void)logRemovals;
      return 0;
#endif
    }

   private:
    CachePolicy m_globalPolicy;

    UnorderedMap<String, Pair<String, system_clock::time_point>> m_inMemoryCache;

    Mutex m_cacheMutex;

    static fn getCacheFilePath(const String& key, const CacheLocation location) -> Option<fs::path> {
      using matchit::match, matchit::is, matchit::_;

      Option<fs::path> cacheDir = None;

      if (location == CacheLocation::InMemory)
        return None; // In-memory cache does not have a file path

      if (location == CacheLocation::TempDirectory)
        return Some(fs::temp_directory_path() / key);

      if (location == CacheLocation::Persistent)
#ifdef __APPLE__
        return Some(std::format("{}/Library/Caches/draconis++/{}", draconis::utils::env::GetEnv("HOME").value_or("."), key));
#else
        return Some(std::format("{}/.cache/draconis++/{}", draconis::utils::env::GetEnv("HOME").value_or("."), key));
#endif

      if (cacheDir) {
        fs::create_directories(*cacheDir);
        return *cacheDir / key;
      }

      return None;
    }
  };
} // namespace draconis::utils::cache

namespace glz {
  template <>
  struct meta<draconis::utils::types::CPUCores> {
    using T = draconis::utils::types::CPUCores;

    static constexpr detail::Object value = object("physical", &T::physical, "logical", &T::logical);
  };

  template <>
  struct meta<draconis::utils::types::NetworkInterface> {
    using T = draconis::utils::types::NetworkInterface;

    // clang-format off
    static constexpr detail::Object value = object(
      "name",        &T::name,
      "isUp",        &T::isUp,
      "isLoopback",  &T::isLoopback,
      "ipv4Address", &T::ipv4Address,
      "macAddress",  &T::macAddress
    );
    // clang-format on
  };

  template <>
  struct meta<draconis::utils::types::DisplayInfo> {
    using T = draconis::utils::types::DisplayInfo;

    // clang-format off
    static constexpr detail::Object value = object(
      "id",          &T::id,
      "resolution",  &T::resolution,
      "refreshRate", &T::refreshRate,
      "isPrimary",   &T::isPrimary
    );
    // clang-format on
  };

  template <>
  struct meta<draconis::utils::types::DisplayInfo::Resolution> {
    using T = draconis::utils::types::DisplayInfo::Resolution;

    static constexpr detail::Object value = object("width", &T::width, "height", &T::height);
  };

  template <typename Tp>
  struct meta<draconis::utils::cache::CacheManager::CacheEntry<Tp>> {
    using T = draconis::utils::cache::CacheManager::CacheEntry<Tp>;

    static constexpr detail::Object value = object("data", &T::data, "expires", &T::expires);
  };
} // namespace glz
