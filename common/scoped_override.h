#ifndef __TSDB2_COMMON_SCOPED_OVERRIDE_H__
#define __TSDB2_COMMON_SCOPED_OVERRIDE_H__

#include "common/utilities.h"

namespace tsdb2 {
namespace common {

// Scoped object to call `Override` on construction and `Restore` on destruction on a given
// overridable type. The allowed overridable types are `Overridable` and `Singleton`.
//
// `ScopedOverride` is movable but not copyable.
//
// Example usage:
//
//   using SingletonClock = tsdb2::common::Singleton<tsdb2::common::Clock const>;
//
//   SingletonClock const singleton_clock{+[] { return tsdb2::common::RealClock::GetInstance(); }};
//
//   ...
//
//   class FooTest : public ::testing::Test {
//    protected:
//     tsdb2::common::MockClock mock_clock_;
//   };
//
//   TEST_F(FooTest, Bar) {
//     tsdb2::common::ScopedOverride override_clock_{&singleton_clock, &mock_clock_};
//     clock_->AdvanceTime(absl::Seconds(123));
//     // ...
//   }
//
// WARNING: `ScopedOverride` does NOT support nesting. It uses `Overridable::OverrideOrDie`, so it
// will check-fail if two instances are nested in scope.
template <typename Overridable>
class ScopedOverride {
 public:
  template <typename T>
  explicit ScopedOverride(Overridable* const overridable, gsl::owner<T*> const value)
      : overridable_(overridable) {
    overridable_->OverrideOrDie(value);
  }

  ~ScopedOverride() { MaybeRestore(); }

  ScopedOverride(ScopedOverride&& other) noexcept : overridable_(other.overridable_) {
    other.overridable_ = nullptr;
  }

  ScopedOverride& operator=(ScopedOverride&& other) noexcept {
    MaybeRestore();
    overridable_ = other.overridable_;
    other.overridable_ = nullptr;
    return *this;
  }

 private:
  ScopedOverride(ScopedOverride const&) = delete;
  ScopedOverride& operator=(ScopedOverride const&) = delete;

  void MaybeRestore() {
    if (overridable_) {
      overridable_->Restore();
    }
  }

  Overridable* overridable_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_SCOPED_OVERRIDE_H__
