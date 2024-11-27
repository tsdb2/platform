#include "common/simple_condition.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::SimpleCondition;

TEST(SimpleConditionTest, Eval) {
  EXPECT_TRUE(SimpleCondition([] { return true; }).Eval());
  EXPECT_FALSE(SimpleCondition([] { return false; }).Eval());
}

}  // namespace
