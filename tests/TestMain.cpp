#include "gtest/gtest.h"

#include "Util/Definitions.hpp"
#include "Util/Types.hpp"

using util::types::i32;

fn main(i32 argc, char** argv) -> i32 {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
