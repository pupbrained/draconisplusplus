#pragma once

#include <__msvc_chrono.hpp>
#include <chrono>
#include <filesystem>
#include <glaze/glaze.hpp>

#include "DataTypes.hpp"
#include "Definitions.hpp"
#include "Env.hpp"
#include "Error.hpp"
#include "Logging.hpp"

namespace draconis::utils::cache {
  namespace {
    using draconis::utils::types::Fn;
    using draconis::utils::types::None;
    using draconis::utils::types::Option;
    using draconis::utils::types::Pair;
    using draconis::utils::types::Result;
    using draconis::utils::types::Some;
    using draconis::utils::types::String;
    using draconis::utils::types::u64;
    using draconis::utils::types::u8;
    using draconis::utils::types::UniquePointer;
    using draconis::utils::types::UnorderedMap;

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

    static auto inMemory() -> CachePolicy {
      return { .location = CacheLocation::InMemory, .ttl = None };
    }

    static auto neverExpire() -> CachePolicy {
      return { .location = CacheLocation::Persistent, .ttl = None };
    }
  };

  class CacheManager {
   public:
    CacheManager() : m_globalPolicy { .location = CacheLocation::Persistent, .ttl = days(1) } {}

    fn setGlobalPolicy(const CachePolicy& policy) -> void {
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
      Fn<Result<T>()>     fetcher,
      Option<CachePolicy> overridePolicy = None
    ) -> Result<T> {
#ifdef ENABLE_CACHING
      const CachePolicy& policy = overridePolicy.value_or(m_globalPolicy);

      // 1. Check in-memory cache
      if (auto iter = m_inMemoryCache.find(key); iter != m_inMemoryCache.end())
        if (
          CacheEntry<T> entry; glz::read_beve(entry, iter->second.first) == glz::error_code::none &&
          (!entry.expires.has_value() || !policy.ttl.has_value() || system_clock::now() < system_clock::time_point(seconds(*entry.expires)))
        )
          return entry.data;

      // 2. Check filesystem cache
      const Option<fs::path> filePath = getCacheFilePath(key, policy.location);

      if (filePath && fs::exists(*filePath))
        if (std::ifstream ifs(*filePath, std::ios::binary); ifs) {
          std::string fileContents((std::istreambuf_iterator<char>(ifs)), {});

          CacheEntry<T> entry;

          if (glz::read_beve(entry, fileContents) == glz::error_code::none)
            if (!entry.expires.has_value() || !policy.ttl.has_value() || system_clock::now() < system_clock::time_point(seconds(*entry.expires))) {
              system_clock::time_point expiryTp = entry.expires.has_value() ? system_clock::time_point(seconds(*entry.expires)) : system_clock::time_point::max();

              m_inMemoryCache[key] = { fileContents, expiryTp };

              return entry.data;
            }
        }

      // 3. Cache miss: call fetcher
      debug_log("Cache miss for key: {}. Calling fetcher.", key);

      Result<T> fetchedResult = fetcher();

      if (!fetchedResult) {
        error_log("Fetcher for key: {} returned an error: {}", key, fetchedResult.error().message);
        return fetchedResult;
      }

      // 4. Store in cache
      Option<u64> expiryTs;
      if (policy.ttl.has_value())
        expiryTs = duration_cast<seconds>((system_clock::now() + *policy.ttl).time_since_epoch()).count();

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

   private:
    CachePolicy m_globalPolicy;

    UnorderedMap<String, Pair<String, system_clock::time_point>> m_inMemoryCache;

    static fn getCacheFilePath(const String& key, const CacheLocation location) -> Option<fs::path> {
      using matchit::match, matchit::is, matchit::_;

      Option<fs::path> cacheDir = match(location)(
        is | CacheLocation::TempDirectory = Some(fs::temp_directory_path()),
#ifdef __APPLE__
        is | CacheLocation::Persistent = Some(std::format("{}/Library/Caches/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."))),
#else
        is | CacheLocation::Persistent = Some(std::format("{}/.cache/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."))),
#endif
        is | _ = None
      );

      if (cacheDir.has_value()) {
        fs::create_directories(cacheDir.value());
        return cacheDir.value() / key;
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
  struct meta<draconis::utils::types::Output> {
    using T = draconis::utils::types::Output;

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
  struct meta<draconis::utils::types::Output::Resolution> {
    using T = draconis::utils::types::Output::Resolution;

    static constexpr detail::Object value = object("width", &T::width, "height", &T::height);
  };

  template <typename Tp>
  struct meta<draconis::utils::cache::CacheManager::CacheEntry<Tp>> {
    using T = draconis::utils::cache::CacheManager::CacheEntry<Tp>;

    static constexpr detail::Object value = object("data", &T::data, "expires", &T::expires);
  };
} // namespace glz