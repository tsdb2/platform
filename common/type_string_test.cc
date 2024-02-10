#include "common/type_string.h"

#include <type_traits>

#include "gtest/gtest.h"

namespace {

using tsdb2::common::TypeStringMatcher;
using tsdb2::common::TypeStringT;

char constexpr kString1[] = "lorem";
char constexpr kString2[] = "ipsum";

TEST(TypeStringTest, Expansion) {
  EXPECT_TRUE((std::is_same_v<TypeStringT<kString1>, TypeStringMatcher<'l', 'o', 'r', 'e', 'm'>>));
}

TEST(TypeStringTest, Match) {
  EXPECT_TRUE((std::is_same_v<TypeStringT<kString1>, TypeStringT<kString1>>));
}

TEST(TypeStringTest, Mismatch) {
  EXPECT_FALSE((std::is_same_v<TypeStringT<kString1>, TypeStringT<kString2>>));
}

TEST(TypeStringTest, Value) {
  EXPECT_EQ(TypeStringT<kString1>::value, kString1);
  EXPECT_EQ(TypeStringT<kString2>::value, kString2);
}

}  // namespace
