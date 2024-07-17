#include "common/re_testing.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::Not;
using ::tsdb2::testing::regexp::Matches;

TEST(RegexpMatcher, Matches) {
  EXPECT_THAT("looorem", Matches("lo+rem"));
  EXPECT_THAT("ipsum", Not(Matches("lo+rem")));
}

}  // namespace
