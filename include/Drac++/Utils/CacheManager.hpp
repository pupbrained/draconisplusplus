#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <glaze/glaze.hpp>
#include <optional>
#include <unordered_map>

#include <Drac++/Utils/Env.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Caching.hpp"

// Add glaze serialization support for custom types
namespace glz {
  // Serialization for CPUCores
  template <>
  struct meta<draconis::utils::types::CPUCores> {
    using T = draconis::utils::types::CPUCores;

    static constexpr detail::Object value = object("physical", &T::physical, "logical", &T::logical);
  };

  // Serialization for NetworkInterface
  template <>
  struct meta<draconis::utils::types::NetworkInterface> {
    using T = draconis::utils::types::NetworkInterface;

    static constexpr detail::Object value = object(
      "name",
      &T::name,
      "isUp",
      &T::isUp,
      "isLoopback",
      &T::isLoopback,
      "ipv4Address",
      &T::ipv4Address,
      "macAddress",
      &T::macAddress
    );
  };

  // Serialization for Display
  template <>
  struct meta<draconis::utils::types::Display> {
    using T = draconis::utils::types::Display;

    static constexpr detail::Object value = object(
      "id",
      &T::id,
      "resolution",
      &T::resolution,
      "refreshRate",
      &T::refreshRate,
      "isPrimary",
      &T::isPrimary
    );
  };

  // Serialization for Resolution
  template <>
  struct meta<draconis::utils::types::Display::Resolution> {
    using T = draconis::utils::types::Display::Resolution;

    static constexpr detail::Object value = object("width", &T::width, "height", &T::height);
  };
} // namespace glz

namespace draconis::utils::cache {
  using draconis::utils::types::Result;
  using draconis::utils::types::String;
  using draconis::utils::types::u64;
  using draconis::utils::types::UniquePointer;

  class CacheManager {
   public:
    CacheManager() : m_globalPolicy { CacheLocation::Persistent, std::chrono::hours(24) } {}

    void setGlobalPolicy(const CachePolicy& policy) {
      m_globalPolicy = policy;
    }

    template <typename T>
    fn getOrSet(
      const String&              key,
      std::function<Result<T>()> fetcher,
      std::optional<CachePolicy> overridePolicy = std::nullopt
    ) -> Result<T> {
      const CachePolicy& policy = overridePolicy.value_or(m_globalPolicy);

      // 1. Check in-memory cache
      if (auto iter = m_inMemoryCache.find(key); iter != m_inMemoryCache.end()) {
        CacheEntry<T> entry;
        if (glz::read_beve(entry, iter->second.first) == glz::error_code::none) {
          auto expiry_tp = std::chrono::system_clock::time_point(std::chrono::seconds(entry.expires));
          if (std::chrono::system_clock::now() < expiry_tp) {
            return entry.data;
          }
        }
      }

      // 2. Check filesystem cache
      const auto filePath = getCacheFilePath(key, policy.location);
      if (std::filesystem::exists(filePath)) {
        std::ifstream ifs(filePath, std::ios::binary);
        if (ifs) {
          std::string   fileContents((std::istreambuf_iterator<char>(ifs)), {});
          CacheEntry<T> entry;
          if (glz::read_beve(entry, fileContents) == glz::error_code::none) {
            auto expiry_tp = std::chrono::system_clock::time_point(std::chrono::seconds(entry.expires));
            if (std::chrono::system_clock::now() < expiry_tp) {
              // Load into in-memory cache
              m_inMemoryCache[key] = { fileContents, expiry_tp };
              return entry.data;
            }
          }
        }
      }

      // 3. Cache miss: call fetcher
      debug_log("Cache miss for key: {}. Calling fetcher.", key);
      Result<T> fetchedResult = fetcher();
      if (!fetchedResult) {
        error_log("Fetcher for key: {} returned an error: {}", key, fetchedResult.error().message);
        return fetchedResult; // Propagate error
      }

      // 4. Store in cache
      u64 expiry_ts = std::chrono::duration_cast<std::chrono::seconds>(
                        (std::chrono::system_clock::now() + policy.ttl).time_since_epoch()
      )
                        .count();
      CacheEntry<T> newEntry {
        .data    = *fetchedResult,
        .expires = expiry_ts
      };

      std::string binaryBuffer;
      glz::write_beve(newEntry, binaryBuffer);

      m_inMemoryCache[key] = { binaryBuffer, std::chrono::system_clock::time_point(std::chrono::seconds(expiry_ts)) };

      if (policy.location != CacheLocation::InMemory) {
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
        ofs.write(binaryBuffer.data(), static_cast<std::streamsize>(binaryBuffer.size()));
      }

      return fetchedResult;
    }

   private:
    CachePolicy m_globalPolicy;

    std::unordered_map<String, std::pair<String, std::chrono::system_clock::time_point>> m_inMemoryCache;

    template <typename T>
    struct CacheEntry {
      T   data;
      u64 expires; // store as UNIX timestamp (seconds since epoch)
    };

    static fn getCacheFilePath(const String& key, CacheLocation location) -> std::filesystem::path {
      std::filesystem::path cacheDir;
      switch (location) {
        case CacheLocation::TempDirectory:
          cacheDir = std::filesystem::temp_directory_path();
          break;
        case CacheLocation::Persistent:
#ifdef __APPLE__
          cacheDir = std::format("{}/Library/Caches/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."));
#else
          cacheDir = std::format("{}/.cache/draconis++", draconis::utils::env::GetEnv("HOME").value_or("."));
#endif
          break;
        case CacheLocation::InMemory:
          return ""; // No file path for in-memory
      }
      std::filesystem::create_directories(cacheDir);
      return cacheDir / key;
    }
  };
} // namespace draconis::utils::cache
