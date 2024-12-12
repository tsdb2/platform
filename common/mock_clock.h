#ifndef __TSDB2_COMMON_MOCK_CLOCK_H__
#define __TSDB2_COMMON_MOCK_CLOCK_H__

#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/clock.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace common {

// This `Clock` implementation allows creating test scenarios with simulated time. It never relies
// on the real time: instead, it encapsulates a fake time that only changes in response to `SetTime`
// or `AdvanceTime` calls.
//
// Advancements of the fake time are correctly reflected in all public methods; as such, they may
// unblock `Sleep*` and `Await*` calls from other threads.
//
// This class is thread-safe.
class MockClock : public Clock {
 public:
  explicit MockClock(absl::Time const current_time = absl::UnixEpoch())
      : current_time_{current_time} {}

  ~MockClock() override = default;

  absl::Time TimeNow() const override ABSL_LOCKS_EXCLUDED(mutex_);

  void SleepFor(absl::Duration duration) const override ABSL_LOCKS_EXCLUDED(mutex_);

  void SleepUntil(absl::Time wakeup_time) const override ABSL_LOCKS_EXCLUDED(mutex_);

  bool AwaitWithTimeout(absl::Mutex* mutex, absl::Condition const& condition,
                        absl::Duration timeout) const override ABSL_SHARED_LOCKS_REQUIRED(mutex);

  bool AwaitWithDeadline(absl::Mutex* mutex, absl::Condition const& condition,
                         absl::Time deadline) const override ABSL_SHARED_LOCKS_REQUIRED(mutex);

  // Sets the fake time to the specified value. Checkfails if `time` is not greater than or equal to
  // the current fake time.
  void SetTime(absl::Time time) ABSL_LOCKS_EXCLUDED(mutex_);

  // Advances the fake time by the specified amount.
  void AdvanceTime(absl::Duration delta) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  class TimeListener {
   public:
    explicit TimeListener(absl::Mutex* const mutex) : mutex_(mutex) {}
    ~TimeListener() = default;

    bool is_notified() const { return notified_; }
    void set_notified(bool const value) { notified_ = value; }

    void Notify() {
      absl::MutexLock lock{mutex_};
      notified_ = true;
    }

   private:
    TimeListener(TimeListener const&) = delete;
    TimeListener& operator=(TimeListener const&) = delete;
    TimeListener(TimeListener&&) = delete;
    TimeListener& operator=(TimeListener&&) = delete;

    absl::Mutex* const mutex_;
    bool notified_ = false;
  };

  MockClock(MockClock const&) = delete;
  MockClock& operator=(MockClock const&) = delete;
  MockClock(MockClock&&) = delete;
  MockClock& operator=(MockClock&&) = delete;

  bool AddListener(absl::Time deadline, TimeListener* listener) ABSL_LOCKS_EXCLUDED(mutex_);

  bool RemoveListener(absl::Time deadline, TimeListener* listener) ABSL_LOCKS_EXCLUDED(mutex_);

  std::vector<TimeListener*> GetDeadlinedListeners() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool AwaitWithDeadlineInternal(absl::Mutex* mutex, absl::Condition const& condition,
                                 absl::Time deadline) ABSL_SHARED_LOCKS_REQUIRED(mutex);

  absl::Mutex mutable mutex_;
  absl::Time current_time_ ABSL_GUARDED_BY(mutex_);
  flat_map<absl::Time, absl::flat_hash_set<TimeListener*>> listeners_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_MOCK_CLOCK_H__
