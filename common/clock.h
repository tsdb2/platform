#ifndef __TSDB2_COMMON_CLOCK_H__
#define __TSDB2_COMMON_CLOCK_H__

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace tsdb2 {
namespace common {

// Provides several time-related utilities.
//
// The default implementation simply relies on Abseil's time utilities, but in
// tests it can be replaced with a `MockClock` that allows controlling time.
//
// This class is abstract, you can get a predefined implementation with
// `RealClock::GetInstance`.
class Clock {
 public:
  virtual ~Clock() = default;

  // Returns the current time.
  virtual absl::Time TimeNow() const = 0;

  // Blocks the caller for at least the specified `duration`.
  virtual void SleepFor(absl::Duration duration) const = 0;

  // Blocks the caller until the specified wake up time.
  virtual void SleepUntil(absl::Time wakeup_time) const = 0;

  // Conditionally acquires `mutex` when either `condition` becomes true or the `timeout` has
  // elapsed, whichever occurs first. Returns true iff acquisition resulted from the condition
  // becoming true, otherwise it returns false.
  //
  // The implementation of this method from `RealClock` simply defers to `mutex->AwaitWithTimeout`,
  // while the implementation from `MockClock` uses the simulated time.
  virtual bool AwaitWithTimeout(absl::Mutex* mutex, absl::Condition const& condition,
                                absl::Duration timeout) const ABSL_SHARED_LOCKS_REQUIRED(mutex) = 0;

  // Conditionally acquires `mutex` when either `condition` becomes true or the `deadline` has
  // occurred, whichever occurs first. Returns true iff acquisition resulted from the condition
  // becoming true, otherwise it returns false.
  //
  // The implementation of this method from `RealClock` simply defers to `mutex->AwaitWithDeadline`,
  // while the implementation from `MockClock` uses the simulated time.
  virtual bool AwaitWithDeadline(absl::Mutex* mutex, absl::Condition const& condition,
                                 absl::Time deadline) const ABSL_SHARED_LOCKS_REQUIRED(mutex) = 0;
};

// Default `Clock` implementation.
class RealClock final : public Clock {
 public:
  // Returns the singleton `RealClock` instance.
  static RealClock const* GetInstance();

  absl::Time TimeNow() const override;

  void SleepFor(absl::Duration duration) const override;

  void SleepUntil(absl::Time wakeup_time) const override;

  bool AwaitWithTimeout(absl::Mutex* mutex, absl::Condition const& condition,
                        absl::Duration timeout) const override ABSL_SHARED_LOCKS_REQUIRED(mutex);

  bool AwaitWithDeadline(absl::Mutex* mutex, absl::Condition const& condition,
                         absl::Time deadline) const override ABSL_SHARED_LOCKS_REQUIRED(mutex);

 private:
  explicit RealClock() = default;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_CLOCK_H__
