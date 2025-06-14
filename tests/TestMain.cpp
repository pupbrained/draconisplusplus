#include <DracUtils/Definitions.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"

using util::types::i32;

fn main(i32 argc, char** argv) -> i32 {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
