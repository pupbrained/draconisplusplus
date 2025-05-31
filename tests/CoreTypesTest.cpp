#include <format> // Required for std::format
#include <objidlbase.h>
#include <string>

#include "Core/SystemData.hpp" // For BytesToGiB

#include "gtest/gtest.h"

class CoreTypesTest : public testing::Test {};

TEST_F(CoreTypesTest, BytesToGiB_ZeroBytes) {
  BytesToGiB  dataSize { 0 };
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OneGiB) {
  BytesToGiB  dataSize { 1073741824ULL }; // 1 * GIB (1 * 1024 * 1024 * 1024)
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OnePointFiveGiB) {
  BytesToGiB  dataSize { 1610612736ULL }; // 1.5 * GIB
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingToTwoDecimalPlaces) {
  BytesToGiB  dataSize { 1325153042ULL };
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.23GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_HalfGiB) {
  BytesToGiB  dataSize { 536870912ULL }; // 0.5 * GIB
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_LargeValue) {
  BytesToGiB  dataSize { 107374182400ULL }; // 100 * GIB
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "100.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_SmallFractionalValueRoundsToZero) {
  BytesToGiB  dataSize { 1073741ULL };
  std::string formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingNearBoundary) {
  BytesToGiB  dataSizeRoundDown { 5368709ULL };
  std::string formattedRoundDown = std::format("{}", dataSizeRoundDown);
  EXPECT_EQ(formattedRoundDown, "0.00GiB");

  BytesToGiB  dataSizeRoundUp { 10737418ULL };
  std::string formattedRoundUp = std::format("{}", dataSizeRoundUp);
  EXPECT_EQ(formattedRoundUp, "0.01GiB");
}


