#ifndef __TSDB2_COMMON_TESTING_H__
#define __TSDB2_COMMON_TESTING_H__

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// absl::Status(Or) testing utilities that are still missing from Googletest. See
// https://github.com/abseil/abseil-cpp/issues/951#issuecomment-828460483 for context.

namespace testing {
namespace status {

inline const absl::Status& GetStatus(absl::Status const& status) { return status; }

template <typename T>
inline const absl::Status& GetStatus(absl::StatusOr<T> const& status) {
  return status.status();
}

// Monomorphic implementation of matcher IsOkAndHolds(m). StatusOrType is a reference to
// StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl : public MatcherInterface<StatusOrType> {
 public:
  typedef typename std::remove_reference<StatusOrType>::type::value_type value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(
            SafeMatcherCast<value_type const&>(std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(StatusOrType const actual_value,
                       MatchResultListener* const result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    StringMatchResultListener inner_listener;
    bool const matches = inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    std::string const inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value " << PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  Matcher<value_type const&> const inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the given type.  StatusOrType can
  // be either StatusOr<T> or a reference to StatusOr<T>.
  template <typename StatusOrType>
  operator Matcher<StatusOrType>() const {  // NOLINT
    return Matcher<StatusOrType>(new IsOkAndHoldsMatcherImpl<StatusOrType const&>(inner_matcher_));
  }

 private:
  InnerMatcher const inner_matcher_;
};

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* const os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* const os) const override { *os << "is not OK"; }
  bool MatchAndExplain(T const actual_value, MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator Matcher<T>() const {  // NOLINT
    return Matcher<T>(new MonoIsOkMatcherImpl<T>());
  }
};

// Macros for testing the results of functions that return absl::Status or absl::StatusOr<T> (for
// any type T).
#define EXPECT_OK(expression) EXPECT_THAT(expression, ::testing::IsOk())
#define ASSERT_OK(expression) ASSERT_THAT(expression, ::testing::IsOk())

// Returns a gMock matcher that matches a StatusOr<> whose status is OK and whose value matches the
// inner matcher.
template <typename InnerMatcher>
IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type> IsOkAndHolds(
    InnerMatcher&& inner_matcher) {
  return IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline IsOkMatcher IsOk() { return IsOkMatcher(); }

}  // namespace status
}  // namespace testing

#endif  // __TSDB2_COMMON_TESTING_H__
