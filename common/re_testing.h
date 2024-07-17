#ifndef __TSDB2_COMMON_RE_TESTING_H__
#define __TSDB2_COMMON_RE_TESTING_H__

#include <ostream>
#include <string>
#include <string_view>

#include "absl/strings/escaping.h"
#include "common/re.h"

namespace testing {
namespace regexp {

class Matches {
 public:
  using is_gtest_matcher = void;

  explicit Matches(std::string_view const pattern)
      : pattern_(pattern), re_(tsdb2::common::RE::Create(pattern).value()) {}

  void DescribeTo(std::ostream* const os) const {
    *os << "matches \"" << absl::CEscape(pattern_) << "\"";
  }

  void DescribeNegationTo(std::ostream* const os) const {
    *os << "doesn't match \"" << absl::CEscape(pattern_) << "\"";
  }

  bool MatchAndExplain(std::string_view const value, std::ostream*) const {
    return re_.Test(value);
  }

 private:
  std::string const pattern_;
  tsdb2::common::RE const re_;
};

}  // namespace regexp
}  // namespace testing

#endif  // __TSDB2_COMMON_RE_TESTING_H__
