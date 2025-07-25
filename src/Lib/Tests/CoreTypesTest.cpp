#include <Drac++/Utils/DataTypes.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "gtest/gtest.h"

using namespace testing;
using draconis::utils::types::String, draconis::utils::types::i32, draconis::utils::types::BytesToGiB;

class CoreTypesTest : public Test {};

TEST_F(CoreTypesTest, BytesToGiB_ZeroBytes) {
  BytesToGiB   dataSize(0);
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OneGiB) {
  BytesToGiB   dataSize(1073741824); // 1 * GIB (1 * 1024 * 1024 * 1024)
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OnePointFiveGiB) {
  BytesToGiB   dataSize(1610612736); // 1.5 * GIB
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingToTwoDecimalPlaces) {
  BytesToGiB   dataSize(1325153042);
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "1.23GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_HalfGiB) {
  BytesToGiB   dataSize(536870912); // 0.5 * GIB
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_LargeValue) {
  BytesToGiB   dataSize(107374182400); // 100 * GIB
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "100.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_SmallFractionalValueRoundsToZero) {
  BytesToGiB   dataSize(1073741);
  const String formatted = std::format("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingNearBoundary) {
  BytesToGiB   dataSizeRoundDown(5368709);
  const String formattedRoundDown = std::format("{}", dataSizeRoundDown);
  EXPECT_EQ(formattedRoundDown, "0.00GiB");

  BytesToGiB   dataSizeRoundUp(10737418);
  const String formattedRoundUp = std::format("{}", dataSizeRoundUp);
  EXPECT_EQ(formattedRoundUp, "0.01GiB");
}

fn main(i32 argc, char** argv) -> i32 {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
