#ifndef __TSDB2_COMMON_FLAG_OVERRIDE_H_
#define __TSDB2_COMMON_FLAG_OVERRIDE_H_

#include "absl/flags/flag.h"

namespace tsdb2 {
namespace common {
namespace testing {

// Scoped object used by unit tests to temporarily override a command line flag. The destructor
// takes care of restoring the original value.
//
// Example:
//
//   TEST(FooTest, Bar) {
//     FlagOverride fo{&FLAG_foo_bar, 42};
//     // ...
//   }
//
template <typename T>
class FlagOverride {
 public:
  template <typename Arg>
  explicit FlagOverride(absl::Flag<T> *const flag, Arg &&arg)
      : flag_(flag), original_value_(absl::GetFlag(*flag_)) {
    absl::SetFlag(flag_, std::forward<Arg>(arg));
  }

  ~FlagOverride() { absl::SetFlag(flag_, original_value_); }

 private:
  absl::Flag<T> *const flag_;
  T original_value_;
};

}  // namespace testing
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FLAG_OVERRIDE_H_
