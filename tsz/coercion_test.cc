#include "tsz/coercion.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(CoercionTest, FixedT) {
  struct Foo {};
  struct Bar {};
  EXPECT_TRUE((std::is_same_v<tsz::util::FixedT<Foo, Bar>, Foo>));
}

TEST(CoercionTest, FixedV) {
  struct Foo {};
  EXPECT_EQ((tsz::util::FixedV<Foo, std::string>("lorem")), "lorem");
}

TEST(CoercionTest, CatTuple) {
  EXPECT_TRUE((std::is_same_v<tsz::util::CatTupleT<std::tuple<>, std::tuple<>>, std::tuple<>>));
  EXPECT_TRUE(
      (std::is_same_v<tsz::util::CatTupleT<std::tuple<>, std::tuple<int>>, std::tuple<int>>));
  EXPECT_TRUE(
      (std::is_same_v<tsz::util::CatTupleT<std::tuple<int>, std::tuple<>>, std::tuple<int>>));
  EXPECT_TRUE((std::is_same_v<tsz::util::CatTupleT<std::tuple<int>, std::tuple<bool>>,
                              std::tuple<int, bool>>));
  EXPECT_TRUE((std::is_same_v<tsz::util::CatTupleT<std::tuple<bool>, std::tuple<int>>,
                              std::tuple<bool, int>>));
  EXPECT_TRUE((std::is_same_v<tsz::util::CatTupleT<std::tuple<bool>, std::tuple<int, std::string>>,
                              std::tuple<bool, int, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<tsz::util::CatTupleT<std::tuple<bool, double>, std::tuple<int, std::string>>,
                      std::tuple<bool, double, int, std::string>>));
}

TEST(CoercionTest, IsIntegralStrict) {
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<signed char>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<unsigned char>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<signed short>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<unsigned short>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<signed int>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<unsigned int>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<signed long>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<unsigned long>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<signed long long>);
  EXPECT_TRUE(tsz::util::IsIntegralStrictV<unsigned long long>);
  EXPECT_FALSE(tsz::util::IsIntegralStrictV<bool>);
  EXPECT_FALSE(tsz::util::IsIntegralStrictV<std::string>);
  EXPECT_FALSE(tsz::util::IsIntegralStrictV<char*>);
}

TEST(CoercionTest, CanonicalType) {
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<bool>, bool>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<signed char>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<unsigned char>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<signed short>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<unsigned short>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<signed int>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<unsigned int>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<signed long>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<unsigned long>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<signed long long>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<unsigned long long>, int64_t>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<std::string>, std::string>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<char*>, std::string>));
  EXPECT_TRUE((std::is_same_v<tsz::CanonicalTypeT<char const*>, std::string>));
}

TEST(CoercionTest, ParameterType) {
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<bool>, bool>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<signed char>, signed char>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<unsigned char>, unsigned char>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<signed short>, signed short>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<unsigned short>, unsigned short>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<signed int>, signed int>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<unsigned int>, unsigned int>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<signed long>, signed long>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<unsigned long>, unsigned long>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<signed long long>, signed long long>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<unsigned long long>, unsigned long long>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<float>, float>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<double>, double>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<long double>, long double>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<std::string>, std::string_view>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<char*>, std::string_view>));
  EXPECT_TRUE((std::is_same_v<tsz::ParameterTypeT<char const*>, std::string_view>));
}

}  // namespace
