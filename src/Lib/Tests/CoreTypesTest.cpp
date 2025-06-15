#include <Drac++/Core/System.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"

using util::types::SZString, util::types::i32;

class CoreTypesTest : public testing::Test {};

TEST_F(CoreTypesTest, BytesToGiB_ZeroBytes) {
  BytesToGiB dataSize { 0 };
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OneGiB) {
  BytesToGiB dataSize { 1073741824ULL }; // 1 * GIB (1 * 1024 * 1024 * 1024)
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "1.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_OnePointFiveGiB) {
  BytesToGiB dataSize { 1610612736ULL }; // 1.5 * GIB
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "1.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingToTwoDecimalPlaces) {
  BytesToGiB dataSize { 1325153042ULL };
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "1.23GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_HalfGiB) {
  BytesToGiB dataSize { 536870912ULL }; // 0.5 * GIB
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "0.50GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_LargeValue) {
  BytesToGiB dataSize { 107374182400ULL }; // 100 * GIB
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "100.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_SmallFractionalValueRoundsToZero) {
  BytesToGiB dataSize { 1073741ULL };
  SZString   formatted = util::formatting::SzFormat("{}", dataSize);
  EXPECT_EQ(formatted, "0.00GiB");
}

TEST_F(CoreTypesTest, BytesToGiB_RoundingNearBoundary) {
  BytesToGiB dataSizeRoundDown { 5368709ULL };
  SZString   formattedRoundDown = util::formatting::SzFormat("{}", dataSizeRoundDown);
  EXPECT_EQ(formattedRoundDown, "0.00GiB");

  BytesToGiB dataSizeRoundUp { 10737418ULL };
  SZString   formattedRoundUp = util::formatting::SzFormat("{}", dataSizeRoundUp);
  EXPECT_EQ(formattedRoundUp, "0.01GiB");
}

fn main(i32 argc, char** argv) -> i32 {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}