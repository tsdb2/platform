#ifndef __TSDB2_COMMON_TESTING_H__
#define __TSDB2_COMMON_TESTING_H__

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace testing {

// GoogleTest's `Pointee` matcher doesn't work with smart pointers, so we deploy our own.
template <typename Inner>
class Pointee2 {
 public:
  using is_gtest_matcher = void;

  explicit Pointee2(Inner inner) : inner_(std::move(inner)) {}

  // Unfortunately GoogleTest's architecture doesn't allow for a meaningful implementation of the
  // Describe methods for this matcher because we wouldn't be able to infer the value type, unlike
  // in `MatchAndExplain`.
  void DescribeTo(std::ostream* const) const {}
  void DescribeNegationTo(std::ostream* const) const {}

  template <typename Value>
  bool MatchAndExplain(Value&& value, ::testing::MatchResultListener* const listener) const {
    return SafeMatcherCast<Value>().MatchAndExplain(*std::forward<Value>(value), listener);
  }

 private:
  template <typename Value>
  auto SafeMatcherCast() const {
    return ::testing::SafeMatcherCast<decltype(*std::declval<Value>())>(inner_);
  }

  Inner inner_;
};

}  // namespace testing

// Macros for testing the results of functions that return absl::Status or absl::StatusOr<T> (for
// any type T).
#define EXPECT_OK(expression) EXPECT_THAT((expression), ::absl_testing::IsOk())
#define ASSERT_OK(expression) ASSERT_THAT((expression), ::absl_testing::IsOk())

#endif  // __TSDB2_COMMON_TESTING_H__
