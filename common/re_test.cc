#include "common/re.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::RE;

TEST(RegexpTest, StaticTest) {
  EXPECT_TRUE(RE::Test("lore", "lo+rem?"));
  EXPECT_TRUE(RE::Test("looorem", "lo+rem?"));
  EXPECT_FALSE(RE::Test("lrem", "lo+rem?"));
}

// TODO

}  // namespace
