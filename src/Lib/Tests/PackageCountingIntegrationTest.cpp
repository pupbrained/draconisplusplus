#include <../../../include/Drac++/Utils/Error.hpp>
#include <../../../include/Drac++/Utils/Types.hpp>
#include <filesystem> // std::filesystem::{create_directories, path, remove_all, temp_directory_path}
#include <fstream>    // std::ofstream

#include <Drac++/Services/Packages.hpp>

#include "gtest/gtest.h"

using namespace testing;
using namespace draconis::services::packages;
using namespace draconis::utils;

using enum error::DracErrorCode;
using types::i32;
using types::Result;
using types::String;
using types::u64;
using types::Unit;

namespace fs = std::filesystem;

#include <Drac++/Utils/CacheManager.hpp>

class PackageCountingIntegrationTest : public Test {
 public:
  fs::path            mTestDir;
  cache::CacheManager mCacheManager;

 protected:
  fn SetUp() -> Unit override {
    mTestDir = fs::temp_directory_path() / "draconis_pkg_test";
    fs::create_directories(mTestDir);
    mCacheManager.setGlobalPolicy({ .location = cache::CacheLocation::TempDirectory });
  }

  fn TearDown() -> Unit override {
    fs::remove_all(mTestDir);
  }

  static fn CreateTestFile(const fs::path& path, const String& content = "") -> Unit {
    std::ofstream file(path);
    file << content;
  }

  static fn CreateTestDir(const fs::path& path) -> Unit {
    fs::create_directories(path);
  }
};

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_EmptyDirectory) {
  const auto result = GetCountFromDirectory(mCacheManager, "test", mTestDir);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 0);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithFiles) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");

  const auto result = GetCountFromDirectory(mCacheManager, "test_files", mTestDir);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 3);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithFilter) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");
  CreateTestFile(mTestDir / "file1.dat");
  CreateTestFile(mTestDir / "file2.dat");

  const auto result = GetCountFromDirectory(mCacheManager, "test_filter", mTestDir, String(".txt"));
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 3);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithSubtractOne) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");

  const auto result = GetCountFromDirectory(mCacheManager, "test_subtract", mTestDir, true);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 2);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_NonexistentDirectory) {
  const auto result = GetCountFromDirectory(mCacheManager, "test_nonexistent", mTestDir / "nonexistent");
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, NotFound);
}

TEST_F(PackageCountingIntegrationTest, GetTotalCount_NoManagers) {
  const auto result = GetTotalCount(mCacheManager, static_cast<Manager>(0));
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, UnavailableFeature);
}

TEST_F(PackageCountingIntegrationTest, GetTotalCount_CargoOnly) {
  if (Result<u64> result = GetTotalCount(mCacheManager, Manager::CARGO); !result)
    EXPECT_TRUE(result.error().code == NotFound || result.error().code == ApiUnavailable);
}

#if defined(__linux__) || defined(__APPLE__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_NixOnly) {
  // Act
  Result<u64> result = GetTotalCount(mCacheManager, Manager::NIX);

  // Assert
  // Note: This test might pass or fail depending on whether Nix is installed
  if (!result)
    EXPECT_TRUE(result.error().code == NotFound || result.error().code == ApiUnavailable);
}
#endif

#if defined(__linux__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_LinuxManagers) {
  // Act
  Result<u64> result = GetTotalCount(mCacheManager, Manager::CARGO | Manager::NIX | Manager::PACMAN | Manager::DPKG);

  // Assert
  // Note: This test might pass or fail depending on which package managers are installed
  if (!result)
    EXPECT_TRUE(result.error().code == NotFound || result.error().code == ApiUnavailable);
}
#endif

#if defined(__APPLE__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_MacManagers) {
  // Act
  Result<u64> result = GetTotalCount(mCacheManager, Manager::CARGO | Manager::NIX | Manager::HOMEBREW);

  // Assert
  // Note: This test might pass or fail depending on which package managers are installed
  if (!result)
    EXPECT_TRUE(result.error().code == NotFound || result.error().code == ApiUnavailable);
}
#endif

#if defined(_WIN32)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_WindowsManagers) {
  if (Result<u64> result = GetTotalCount(mCacheManager, Manager::CARGO | Manager::WINGET | Manager::CHOCOLATEY); !result)
    EXPECT_TRUE(result.error().code == NotFound || result.error().code == ApiUnavailable);
}
#endif

fn main(i32 argc, char** argv) -> i32 {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}