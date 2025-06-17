#include <filesystem> // std::filesystem::{path, temp_directory_path}

#include <Drac++/Services/PackageCounting.hpp>

#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;
using namespace package;
using drac::error::DracError;
using drac::error::DracErrorCode;
using drac::types::Err;
using drac::types::Result;
using drac::types::u64;

class PackageCountingMock {
 public:
  MOCK_METHOD(Result<u64>, GetCountFromDirectory, (const drac::types::String&, const std::filesystem::path&, const drac::types::String&, bool), ());
  MOCK_METHOD(Result<u64>, GetCountFromDb, (const drac::types::String&, const std::filesystem::path&, const drac::types::String&), ());
  MOCK_METHOD(Result<u64>, GetTotalCount, (Manager), ());
};

class PackageCountingTest : public Test {
 protected:
  PackageCountingMock m_mockCounter;
};

TEST_F(PackageCountingTest, GetCountFromDirectoryReturnsExpectedValue) {
  const drac::types::String   pmId        = "test";
  const std::filesystem::path dirPath     = "/test/path";
  const drac::types::String   filter      = ".pkg";
  constexpr bool              subtractOne = false;

  EXPECT_CALL(m_mockCounter, GetCountFromDirectory(pmId, dirPath, filter, subtractOne))
    .WillOnce(Return(Result<u64>(42)));

  const Result<u64> result = m_mockCounter.GetCountFromDirectory(pmId, dirPath, filter, subtractOne);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 42);
}

TEST_F(PackageCountingTest, GetCountFromDbReturnsExpectedValue) {
  const drac::types::String   pmId   = "test";
  const std::filesystem::path dbPath = "/test/db.sqlite";
  const drac::types::String   query  = "SELECT COUNT(*) FROM packages";

  EXPECT_CALL(m_mockCounter, GetCountFromDb(pmId, dbPath, query))
    .WillOnce(Return(Result<u64>(100)));

  const Result<u64> result = m_mockCounter.GetCountFromDb(pmId, dbPath, query);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 100);
}

TEST_F(PackageCountingTest, GetTotalCountReturnsExpectedValue) {
  Manager enabledManagers = Manager::CARGO;

#if defined(__linux__) || defined(__APPLE__)
  enabledManagers = enabledManagers | Manager::NIX;
#endif

  EXPECT_CALL(m_mockCounter, GetTotalCount(enabledManagers))
    .WillOnce(Return(Result<u64>(150)));

  const Result<u64> result = m_mockCounter.GetTotalCount(enabledManagers);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 150);
}

TEST_F(PackageCountingTest, GetCountFromDirectoryReturnsErrorWhenDirectoryNotFound) {
  const drac::types::String   pmId        = "test";
  const std::filesystem::path dirPath     = "/nonexistent/path";
  const drac::types::String   filter      = ".pkg";
  constexpr bool              subtractOne = false;

  EXPECT_CALL(m_mockCounter, GetCountFromDirectory(pmId, dirPath, filter, subtractOne))
    .WillOnce(Return(Err(DracError(DracErrorCode::NotFound, "Directory not found"))));

  Result<u64> result = m_mockCounter.GetCountFromDirectory(pmId, dirPath, filter, subtractOne);

  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::NotFound);
}

TEST_F(PackageCountingTest, GetCountFromDbReturnsErrorWhenDatabaseCorrupt) {
  const drac::types::String   pmId   = "test";
  const std::filesystem::path dbPath = "/test/corrupt.sqlite";
  const drac::types::String   query  = "SELECT COUNT(*) FROM packages";

  EXPECT_CALL(m_mockCounter, GetCountFromDb(pmId, dbPath, query))
    .WillOnce(Return(Err(DracError(DracErrorCode::ParseError, "Database is corrupt"))));

  Result<u64> result = m_mockCounter.GetCountFromDb(pmId, dbPath, query);

  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::ParseError);
}

TEST_F(PackageCountingTest, GetTotalCountReturnsErrorWhenNoManagersEnabled) {
  constexpr Manager enabledManagers = Manager::NONE;

  EXPECT_CALL(m_mockCounter, GetTotalCount(enabledManagers))
    .WillOnce(Return(Err(DracError(DracErrorCode::InvalidArgument, "No package managers enabled"))));

  Result<u64> result = m_mockCounter.GetTotalCount(enabledManagers);

  EXPECT_FALSE(result);
  EXPECT_EQ(result.error().code, DracErrorCode::InvalidArgument);
}

#ifdef __linux__
TEST_F(PackageCountingTest, LinuxPackageManagersAreAvailable) {
  constexpr Manager linuxManagers = Manager::CARGO | Manager::NIX | Manager::PACMAN | Manager::DPKG;

  EXPECT_CALL(m_mockCounter, GetTotalCount(linuxManagers))
    .WillOnce(Return(Result<u64>(200)));

  const Result<u64> result = m_mockCounter.GetTotalCount(linuxManagers);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 200);
}
#endif

#ifdef __APPLE__
TEST_F(PackageCountingTest, MacPackageManagersAreAvailable) {
  constexpr Manager macManagers = Manager::CARGO | Manager::NIX | Manager::HOMEBREW;

  EXPECT_CALL(m_mockCounter, GetTotalCount(macManagers))
    .WillOnce(Return(Result<u64>(150)));

  const Result<u64> result = m_mockCounter.GetTotalCount(macManagers);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 150);
}
#endif

#ifdef _WIN32
TEST_F(PackageCountingTest, WindowsPackageManagersAreAvailable) {
  constexpr Manager winManagers = Manager::CARGO | Manager::WINGET | Manager::CHOCOLATEY;

  EXPECT_CALL(m_mockCounter, GetTotalCount(winManagers))
    .WillOnce(Return(Result<u64>(100)));

  const Result<u64> result = m_mockCounter.GetTotalCount(winManagers);

  EXPECT_TRUE(result);
  EXPECT_EQ(*result, 100);
}
#endif

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}