#include <chrono>
#include <filesystem>
#include <thread>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Types.hpp>

#include "gtest/gtest.h"

using namespace testing;
using namespace draconis::utils;

using cache::CacheLocation;
using cache::CacheManager;
using cache::CachePolicy;

using error::DracError;
using error::DracErrorCode;

using types::Err;
using types::i32;
using types::Option;
using types::PCStr;
using types::Result;
using types::String;
using types::Unit;

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class CacheManagerTest : public Test {
 protected:
  // NOLINTBEGIN(*-non-private-member-variables-in-classes)
  fs::path      m_testDir;
  Result<PCStr> m_originalHome;
  // NOLINTEND(*-non-private-member-variables-in-classes)

  fn SetUp() -> Unit override {
    // Ensure we have a clean test environment
    m_testDir = fs::temp_directory_path() / "draconis_cache_test";

    if (fs::exists(m_testDir))
      fs::remove_all(m_testDir);

    fs::create_directories(m_testDir);

    // Clean up any existing temp directory cache files from previous test runs
    // This is needed because TempDirectoryCache test uses /tmp/temp_key
    fs::path tempKeyFile = fs::temp_directory_path() / "temp_key";
    if (fs::exists(tempKeyFile))
      fs::remove(tempKeyFile);

    // Set environment variable for test
    m_originalHome = env::GetEnv("HOME");

    env::SetEnv("HOME", m_testDir.c_str());
  }

  fn TearDown() -> Unit override {
    // Restore original environment
    if (m_originalHome)
      env::SetEnv("HOME", *m_originalHome);
    else
      env::UnsetEnv("HOME");

    // Clean up test directory
    if (fs::exists(m_testDir))
      fs::remove_all(m_testDir);
  }

  // Helper function to create a fetcher that counts calls
  static auto createCountingFetcher(i32& counter, i32 value) {
    return [&counter, value]() -> Result<i32> {
      counter++;
      return value;
    };
  }

  // Helper function to create a fetcher that simulates failure
  static auto createFailingFetcher() {
    return []() -> Result<i32> {
      return Err(DracError(DracErrorCode::Other, "Fetch failed"));
    };
  }

  // Helper function to create a fetcher with delay
  static auto createDelayedFetcher(i32 value, std::chrono::milliseconds delay) {
    return [value, delay]() -> Result<i32> {
      std::this_thread::sleep_for(delay);
      return value;
    };
  }
};

// Default constructor should not throw
TEST_F(CacheManagerTest, DefaultConstructor) {
  CacheManager cache;
}

// Basic memory cache tests
TEST_F(CacheManagerTest, MemoryCacheHit) {
  CacheManager cache;
  cache.setGlobalPolicy(CachePolicy::inMemory());

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch
  ASSERT_TRUE(cache.getOrSet<i32>("test_key", fetcher).has_value());
  EXPECT_EQ(*cache.getOrSet<i32>("test_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Second call should use cache
  EXPECT_EQ(*cache.getOrSet<i32>("test_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1); // Fetcher should not be called again
}

TEST_F(CacheManagerTest, DifferentKeysInMemory) {
  CacheManager cache;
  cache.setGlobalPolicy(CachePolicy::inMemory());

  i32 fetchCount1 = 0, fetchCount2 = 0;

  // First key
  auto fetcher1 = createCountingFetcher(fetchCount1, 42);
  EXPECT_EQ(*cache.getOrSet<i32>("key1", fetcher1), 42);

  // Second key
  auto fetcher2 = createCountingFetcher(fetchCount2, 84);
  EXPECT_EQ(*cache.getOrSet<i32>("key2", fetcher2), 84);

  // Both should be cached independently
  EXPECT_EQ(*cache.getOrSet<i32>("key1", fetcher1), 42);
  EXPECT_EQ(*cache.getOrSet<i32>("key2", fetcher2), 84);
}

TEST_F(CacheManagerTest, FetcherFailure) {
  CacheManager cache;
  cache.setGlobalPolicy(CachePolicy::inMemory());

  auto failingFetcher = createFailingFetcher();
  auto result         = cache.getOrSet<i32>("error_key", failingFetcher);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, DracErrorCode::Other);
  EXPECT_EQ(result.error().message, "Fetch failed");
}

// Test with custom struct
struct TestData {
  i32    value;
  String name;

  bool operator==(const TestData& other) const {
    return value == other.value && name == other.name;
  }
};

namespace glz {
  template <>
  struct meta<TestData> {
    using T = TestData;

    static constexpr auto value = glz::object(
      "value",
      &TestData::value,
      "name",
      &TestData::name
    );
  };
} // namespace glz

TEST_F(CacheManagerTest, DifferentTypes) {
  CacheManager cache;
  cache.setGlobalPolicy(CachePolicy::inMemory());

  // Test with integer
  i32 intFetchCount = 0;
  EXPECT_EQ(*cache.getOrSet<i32>("int_key", createCountingFetcher(intFetchCount, 42)), 42);

  // Test with string
  i32  strFetchCount = 0;
  auto stringFetcher = [&strFetchCount]() -> Result<String> {
    strFetchCount++;
    return "cached string";
  };

  EXPECT_EQ(*cache.getOrSet<String>("string_key", stringFetcher), "cached string");
  EXPECT_EQ(strFetchCount, 1);

  i32  structFetchCount = 0;
  auto structFetcher    = [&structFetchCount]() -> Result<TestData> {
    structFetchCount++;
    return TestData { .value = 100, .name = "test struct" };
  };

  auto result = cache.getOrSet<TestData>("struct_key", structFetcher);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value, 100);
  EXPECT_EQ(result->name, "test struct");
  EXPECT_EQ(structFetchCount, 1);

  // Cached fetch should not increment counter
  auto cachedResult = cache.getOrSet<TestData>("struct_key", structFetcher);
  ASSERT_TRUE(cachedResult.has_value());
  EXPECT_EQ(cachedResult->value, 100);
  EXPECT_EQ(cachedResult->name, "test struct");
  EXPECT_EQ(structFetchCount, 1);
}

TEST_F(CacheManagerTest, PolicyOverride) {
  CacheManager cache;
  // Set global policy to persistent
  cache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = std::chrono::hours(24) });

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // Override with in-memory policy for this specific call
  auto result = cache.getOrSet<i32>(
    "override_key",
    fetcher,
    CachePolicy::inMemory()
  );

  EXPECT_EQ(*result, 42);
  EXPECT_EQ(fetchCount, 1);

  // Should be cached in memory
  EXPECT_EQ(*cache.getOrSet<i32>("override_key", fetcher, CachePolicy::inMemory()), 42);
  EXPECT_EQ(fetchCount, 1);
}

// File cache tests
TEST_F(CacheManagerTest, TempDirectoryCache) {
  CacheManager cache;
  cache.setGlobalPolicy({ .location = CacheLocation::TempDirectory, .ttl = std::chrono::hours(24) });

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch and store in temp directory
  EXPECT_EQ(*cache.getOrSet<i32>("temp_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Create a new cache manager to simulate application restart
  CacheManager newCache;
  newCache.setGlobalPolicy({ .location = CacheLocation::TempDirectory, .ttl = std::chrono::hours(24) });

  // Should read from cache file without fetching
  EXPECT_EQ(*newCache.getOrSet<i32>("temp_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1); // Fetcher should not be called again
}

TEST_F(CacheManagerTest, PersistentDirectoryCache) {
  CacheManager cache;
  cache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = std::chrono::hours(24) });

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch and store in persistent directory
  EXPECT_EQ(*cache.getOrSet<i32>("persistent_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Verify cache file exists in the expected location
  fs::path cacheDir  = fs::path(m_testDir) / ".cache" / "draconis++";
  fs::path cacheFile = cacheDir / "persistent_key";
  ASSERT_TRUE(fs::exists(cacheFile));

  // Create a new cache manager to simulate application restart
  CacheManager newCache;
  newCache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = std::chrono::hours(24) });

  // Should read from cache file without fetching
  EXPECT_EQ(*newCache.getOrSet<i32>("persistent_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1); // Fetcher should not be called again
}

TEST_F(CacheManagerTest, CrossCacheLocationRetrieval) {
  // Test that caches with different locations don't interfere

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // Create and populate in-memory cache
  {
    CacheManager memoryCache;
    memoryCache.setGlobalPolicy(CachePolicy::inMemory());
    EXPECT_EQ(*memoryCache.getOrSet<i32>("cross_location_key", fetcher), 42);
    EXPECT_EQ(fetchCount, 1);
  }

  // Create file cache and verify it doesn't see memory-only cache
  CacheManager fileCache;
  fileCache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = std::chrono::hours(24) });

  EXPECT_EQ(*fileCache.getOrSet<i32>("cross_location_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 2); // Should fetch again because memory-only cache isn't persisted

  // Create a new file cache to verify persistence
  CacheManager newFileCache;
  newFileCache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = std::chrono::hours(24) });

  EXPECT_EQ(*newFileCache.getOrSet<i32>("cross_location_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 2); // Should read from persistent cache
}

// TTL and expiration tests
TEST_F(CacheManagerTest, MemoryCacheTTL) {
  using namespace std::chrono_literals;

  CacheManager cache;
  cache.setGlobalPolicy({ .location = CacheLocation::InMemory, .ttl = 1s }); // Use 1 second instead of 100ms

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch
  EXPECT_EQ(*cache.getOrSet<i32>("ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Call before TTL expiration should use cache
  EXPECT_EQ(*cache.getOrSet<i32>("ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Wait for TTL to expire
  std::this_thread::sleep_for(1100ms); // Wait slightly longer than 1 second

  // Call after TTL expiration should fetch again
  EXPECT_EQ(*cache.getOrSet<i32>("ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 2);
}

TEST_F(CacheManagerTest, PersistentCacheTTL) {
  using namespace std::chrono_literals;

  CacheManager cache;
  cache.setGlobalPolicy({ .location = CacheLocation::Persistent, .ttl = 1s }); // Use 1 second instead of 100ms

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch
  EXPECT_EQ(*cache.getOrSet<i32>("persistent_ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Call before TTL expiration should use cache
  EXPECT_EQ(*cache.getOrSet<i32>("persistent_ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Wait for TTL to expire
  std::this_thread::sleep_for(1100ms); // Wait slightly longer than 1 second

  // Call after TTL expiration should fetch again
  EXPECT_EQ(*cache.getOrSet<i32>("persistent_ttl_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 2);
}

TEST_F(CacheManagerTest, NeverExpire) {
  CacheManager cache;
  cache.setGlobalPolicy(CachePolicy::neverExpire());

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch
  EXPECT_EQ(*cache.getOrSet<i32>("never_expire_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);

  // Wait some time
  std::this_thread::sleep_for(200ms);

  // Should still use cache since it never expires
  EXPECT_EQ(*cache.getOrSet<i32>("never_expire_key", fetcher), 42);
  EXPECT_EQ(fetchCount, 1);
}

TEST_F(CacheManagerTest, TTLOverride) {
  using namespace std::chrono_literals;

  CacheManager cache;
  cache.setGlobalPolicy({ .location = CacheLocation::InMemory, .ttl = 1h }); // Long global TTL

  i32  fetchCount = 0;
  auto fetcher    = createCountingFetcher(fetchCount, 42);

  // First call should fetch
  auto result = cache.getOrSet<i32>(
    "ttl_override_key",
    fetcher,
    CachePolicy { .location = CacheLocation::InMemory, .ttl = 1s } // Override with 1 second TTL
  );

  EXPECT_EQ(*result, 42);
  EXPECT_EQ(fetchCount, 1);

  // Wait for override TTL to expire
  std::this_thread::sleep_for(1100ms); // Wait slightly longer than 1 second

  // Should fetch again because we used the override TTL
  result = cache.getOrSet<i32>(
    "ttl_override_key",
    fetcher,
    CachePolicy { .location = CacheLocation::InMemory, .ttl = 1s } // Override with 1 second TTL again
  );

  EXPECT_EQ(*result, 42);
  EXPECT_EQ(fetchCount, 2);
}

fn main(i32 argc, char** argv) -> i32 {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
