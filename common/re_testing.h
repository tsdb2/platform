#ifndef __TSDB2_COMMON_RE_TESTING_H__
#define __TSDB2_COMMON_RE_TESTING_H__

#include <ostream>
#include <string>
#include <string_view>

#include "absl/strings/escaping.h"
#include "common/re.h"
#include "gtest/gtest.h"

namespace tsdb2 {
namespace testing {
namespace regexp {

// GoogleTest matcher for regular expressions.
//
// Example usage:
//
//   EXPECT_THAT(foo, Matches("hell+o"));  // matches "hello", "helllo", "hellllo", etc.
//
class Matches : public ::testing::MatcherInterface<std::string_view> {
 public:
  using is_gtest_matcher = void;

  explicit Matches(std::string_view const pattern)
      : pattern_(pattern), re_(tsdb2::common::RE::Create(pattern).value()) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "matches \"" << absl::CEscape(pattern_) << "\"";
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "doesn't match \"" << absl::CEscape(pattern_) << "\"";
  }

  bool MatchAndExplain(std::string_view const value,
                       ::testing::MatchResultListener* const listener) const override {
    bool const result = re_.Test(value);
    if (result) {
      *listener << "matches \"" << absl::CEscape(pattern_) << "\"";
    } else {
      *listener << "doesn't match \"" << absl::CEscape(pattern_) << "\"";
    }
    return result;
  }

 private:
  std::string const pattern_;
  tsdb2::common::RE const re_;
};

}  // namespace regexp
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_TESTING_H__
