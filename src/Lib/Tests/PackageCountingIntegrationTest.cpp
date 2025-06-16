#include <filesystem> // std::filesystem::{create_directories, path, remove_all, temp_directory_path}
#include <fstream>    // std::ofstream

#include <Drac++/Services/PackageCounting.hpp>

#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"

using namespace testing;
using namespace package;
using util::error::DracErrorCode;
using util::types::Result, util::types::String, util::types::u64;

namespace fs = std::filesystem;

class PackageCountingIntegrationTest : public Test {
 public:
  fs::path mTestDir;

 protected:
  void SetUp() override {
    mTestDir = fs::temp_directory_path() / "draconis_pkg_test";
    fs::create_directories(mTestDir);
  }

  void TearDown() override {
    fs::remove_all(mTestDir);
  }

  static void CreateTestFile(const fs::path& path, const String& content = "") {
    std::ofstream file(path);
    file << content;
  }

  static void CreateTestDir(const fs::path& path) {
    fs::create_directories(path);
  }
};

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_EmptyDirectory) {
  const auto result = package::GetCountFromDirectory("test", mTestDir);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 0);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithFiles) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");

  const auto result = package::GetCountFromDirectory("test_files", mTestDir);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 3);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithFilter) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");
  CreateTestFile(mTestDir / "file1.dat");
  CreateTestFile(mTestDir / "file2.dat");

  const auto result = package::GetCountFromDirectory("test_filter", mTestDir, String(".txt"));
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 3);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_WithSubtractOne) {
  CreateTestFile(mTestDir / "file1.txt");
  CreateTestFile(mTestDir / "file2.txt");
  CreateTestFile(mTestDir / "file3.txt");

  const auto result = package::GetCountFromDirectory("test_subtract", mTestDir, true);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 2);
}

TEST_F(PackageCountingIntegrationTest, GetCountFromDirectory_NonexistentDirectory) {
  const auto result = package::GetCountFromDirectory("test_nonexistent", mTestDir / "nonexistent");
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::NotFound);
}

TEST_F(PackageCountingIntegrationTest, GetTotalCount_NoManagers) {
  const auto result = package::GetTotalCount(static_cast<package::Manager>(0));
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::NotFound);
}

TEST_F(PackageCountingIntegrationTest, GetTotalCount_CargoOnly) {
  // Act
  Result<u64> result = GetTotalCount(Manager::CARGO);

  // Assert
  // Note: This test might pass or fail depending on whether Cargo is installed
  // We just check that it doesn't throw any unexpected errors
  if (!result)
    EXPECT_TRUE(result.error().code == DracErrorCode::NotFound || result.error().code == DracErrorCode::ApiUnavailable);
}

#if defined(__linux__) || defined(__APPLE__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_NixOnly) {
  // Act
  Result<u64> result = GetTotalCount(Manager::NIX);

  // Assert
  // Note: This test might pass or fail depending on whether Nix is installed
  if (!result) {
    EXPECT_TRUE(result.error().code == DracErrorCode::NotFound || result.error().code == DracErrorCode::ApiUnavailable);
  }
}
#endif

#if defined(__linux__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_LinuxManagers) {
  // Act
  Result<u64> result = GetTotalCount(Manager::CARGO | Manager::NIX | Manager::PACMAN | Manager::DPKG);

  // Assert
  // Note: This test might pass or fail depending on which package managers are installed
  if (!result) {
    EXPECT_TRUE(result.error().code == DracErrorCode::NotFound || result.error().code == DracErrorCode::ApiUnavailable);
  }
}
#endif

#if defined(__APPLE__)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_MacManagers) {
  // Act
  Result<u64> result = GetTotalCount(Manager::CARGO | Manager::NIX | Manager::HOMEBREW);

  // Assert
  // Note: This test might pass or fail depending on which package managers are installed
  if (!result) {
    EXPECT_TRUE(result.error().code == DracErrorCode::NotFound || result.error().code == DracErrorCode::ApiUnavailable);
  }
}
#endif

#if defined(_WIN32)
TEST_F(PackageCountingIntegrationTest, GetTotalCount_WindowsManagers) {
  // Act
  Result<u64> result = GetTotalCount(Manager::CARGO | Manager::WINGET | Manager::CHOCOLATEY);

  // Assert
  // Note: This test might pass or fail depending on which package managers are installed
  if (!result) {
    EXPECT_TRUE(result.error().code == DracErrorCode::NotFound || result.error().code == DracErrorCode::ApiUnavailable);
  }
}
#endif

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}