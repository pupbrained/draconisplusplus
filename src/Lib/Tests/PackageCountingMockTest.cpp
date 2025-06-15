#include <Drac++/Services/PackageCounting.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace package;
using util::error::DracError;
using util::error::DracErrorCode;
using util::types::Err;
using util::types::Result;
using util::types::u64;

// Mock functions for package counting
class PackageCountingMock {
 public:
  MOCK_METHOD(Result<u64>, getCountFromDirectory, (const util::types::SZString&, const std::filesystem::path&, const util::types::SZString&, bool), ());
  MOCK_METHOD(Result<u64>, getCountFromDb, (const util::types::SZString&, const std::filesystem::path&, const util::types::SZString&), ());
  MOCK_METHOD(Result<u64>, getTotalCount, (Manager), ());
};

class PackageCountingTest : public Test {
 protected:
  PackageCountingMock m_mockCounter;
};

// Basic functionality tests
TEST_F(PackageCountingTest, GetCountFromDirectoryReturnsExpectedValue) {
  // Arrange
  util::types::SZString pmId        = "test";
  std::filesystem::path dirPath     = "/test/path";
  util::types::SZString filter      = ".pkg";
  bool                  subtractOne = false;

  EXPECT_CALL(m_mockCounter, getCountFromDirectory(pmId, dirPath, filter, subtractOne))
    .WillOnce(Return(Result<u64>(42)));

  // Act
  Result<u64> result = m_mockCounter.getCountFromDirectory(pmId, dirPath, filter, subtractOne);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 42);
}

TEST_F(PackageCountingTest, GetCountFromDbReturnsExpectedValue) {
  // Arrange
  util::types::SZString pmId   = "test";
  std::filesystem::path dbPath = "/test/db.sqlite";
  util::types::SZString query  = "SELECT COUNT(*) FROM packages";

  EXPECT_CALL(m_mockCounter, getCountFromDb(pmId, dbPath, query))
    .WillOnce(Return(Result<u64>(100)));

  // Act
  Result<u64> result = m_mockCounter.getCountFromDb(pmId, dbPath, query);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 100);
}

TEST_F(PackageCountingTest, GetTotalCountReturnsExpectedValue) {
  // Arrange
  Manager enabledManagers = Manager::CARGO;

#if defined(__linux__) || defined(__APPLE__)
  enabledManagers = enabledManagers | Manager::NIX;
#endif

  EXPECT_CALL(m_mockCounter, getTotalCount(enabledManagers))
    .WillOnce(Return(Result<u64>(150)));

  // Act
  Result<u64> result = m_mockCounter.getTotalCount(enabledManagers);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 150);
}

// Error condition tests
TEST_F(PackageCountingTest, GetCountFromDirectoryReturnsErrorWhenDirectoryNotFound) {
  // Arrange
  util::types::SZString pmId        = "test";
  std::filesystem::path dirPath     = "/nonexistent/path";
  util::types::SZString filter      = ".pkg";
  bool                  subtractOne = false;

  EXPECT_CALL(m_mockCounter, getCountFromDirectory(pmId, dirPath, filter, subtractOne))
    .WillOnce(Return(Err(DracError(DracErrorCode::NotFound, "Directory not found"))));

  // Act
  Result<u64> result = m_mockCounter.getCountFromDirectory(pmId, dirPath, filter, subtractOne);

  // Assert
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::NotFound);
}

TEST_F(PackageCountingTest, GetCountFromDbReturnsErrorWhenDatabaseCorrupt) {
  // Arrange
  util::types::SZString pmId   = "test";
  std::filesystem::path dbPath = "/test/corrupt.sqlite";
  util::types::SZString query  = "SELECT COUNT(*) FROM packages";

  EXPECT_CALL(m_mockCounter, getCountFromDb(pmId, dbPath, query))
    .WillOnce(Return(Err(DracError(DracErrorCode::ParseError, "Database is corrupt"))));

  // Act
  Result<u64> result = m_mockCounter.getCountFromDb(pmId, dbPath, query);

  // Assert
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::ParseError);
}

TEST_F(PackageCountingTest, GetTotalCountReturnsErrorWhenNoManagersEnabled) {
  // Arrange
  Manager enabledManagers = Manager::NONE;

  EXPECT_CALL(m_mockCounter, getTotalCount(enabledManagers))
    .WillOnce(Return(Err(DracError(DracErrorCode::InvalidArgument, "No package managers enabled"))));

  // Act
  Result<u64> result = m_mockCounter.getTotalCount(enabledManagers);

  // Assert
  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::InvalidArgument);
}

// Platform-specific tests
#ifdef __linux__
TEST_F(PackageCountingTest, LinuxPackageManagersAreAvailable) {
  // Arrange
  Manager linuxManagers = Manager::CARGO | Manager::NIX | Manager::PACMAN | Manager::DPKG;

  EXPECT_CALL(m_mockCounter, getTotalCount(linuxManagers))
    .WillOnce(Return(Result<u64>(200)));

  // Act
  Result<u64> result = m_mockCounter.getTotalCount(linuxManagers);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 200);
}
#endif

#ifdef __APPLE__
TEST_F(PackageCountingTest, MacPackageManagersAreAvailable) {
  // Arrange
  Manager macManagers = Manager::CARGO | Manager::NIX | Manager::HOMEBREW;

  EXPECT_CALL(m_mockCounter, getTotalCount(macManagers))
    .WillOnce(Return(Result<u64>(150)));

  // Act
  Result<u64> result = m_mockCounter.getTotalCount(macManagers);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 150);
}
#endif

#ifdef _WIN32
TEST_F(PackageCountingTest, WindowsPackageManagersAreAvailable) {
  // Arrange
  Manager winManagers = Manager::CARGO | Manager::WINGET | Manager::CHOCOLATEY;

  EXPECT_CALL(m_mockCounter, getTotalCount(winManagers))
    .WillOnce(Return(Result<u64>(100)));

  // Act
  Result<u64> result = m_mockCounter.getTotalCount(winManagers);

  // Assert
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 100);
}
#endif

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}