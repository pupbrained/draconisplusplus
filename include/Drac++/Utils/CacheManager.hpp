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

namespace draconis::utils::cache {
  using draconis::utils::types::Result;
  using draconis::utils::types::String;
  using draconis::utils::types::u64;
  using draconis::utils::types::UniquePointer;

  class CacheManager {
   public:
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
        if (glz::read_json(entry, iter->second.first) == glz::error_code::none) {
          if (std::chrono::system_clock::now() < entry.expires) {
            return entry.data;
          }
        }
      }

      // 2. Check filesystem cache
      const auto filePath = getCacheFilePath(key, policy.location);
      if (std::filesystem::exists(filePath)) {
        std::string   fileContents;
        CacheEntry<T> entry;
        if (glz::read_file_json(entry, filePath.string(), fileContents) == glz::error_code::none) {
          if (std::chrono::system_clock::now() < entry.expires) {
            // Load into in-memory cache
            m_inMemoryCache[key] = { fileContents, entry.expires };
            return entry.data;
          }
        }
      }

      // 3. Cache miss: call fetcher
      Result<T> fetchedResult = fetcher();
      if (!fetchedResult) {
        return fetchedResult; // Propagate error
      }

      // 4. Store in cache
      CacheEntry<T> newEntry {
        .data    = *fetchedResult,
        .expires = std::chrono::system_clock::now() + policy.ttl
      };

      std::string serializedData;
      glz::write<glz::opts {}>(newEntry, serializedData);

      m_inMemoryCache[key] = { serializedData, newEntry.expires };

      if (policy.location != CacheLocation::InMemory) {
        std::filesystem::create_directories(filePath.parent_path());
        glz::write_file_json(newEntry, filePath.string(), std::string {});
      }

      return fetchedResult;
    }

   private:
    CachePolicy m_globalPolicy;

    std::unordered_map<String, std::pair<String, std::chrono::system_clock::time_point>> m_inMemoryCache;

    template <typename T>
    struct CacheEntry {
      T                                     data;
      std::chrono::system_clock::time_point expires;
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
