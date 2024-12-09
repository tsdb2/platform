#include "common/mock_clock.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/simple_condition.h"

namespace tsdb2 {
namespace common {

absl::Time MockClock::TimeNow() const {
  absl::MutexLock lock{&mutex_};
  return current_time_;
}

void MockClock::SleepFor(absl::Duration const duration) const {
  absl::MutexLock lock{&mutex_};
  auto const wakeup_time = current_time_ + duration;
  mutex_.Await(SimpleCondition([this, wakeup_time]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
    return current_time_ >= wakeup_time;
  }));
}

void MockClock::SleepUntil(absl::Time const wakeup_time) const {
  absl::MutexLock lock{&mutex_};
  mutex_.Await(SimpleCondition([this, wakeup_time]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
    return current_time_ >= wakeup_time;
  }));
}

bool MockClock::AwaitWithTimeout(absl::Mutex* const mutex, absl::Condition const& condition,
                                 absl::Duration const timeout) const {
  return const_cast<MockClock*>(this)->AwaitWithDeadlineInternal(mutex, condition,
                                                                 TimeNow() + timeout);
}

bool MockClock::AwaitWithDeadline(absl::Mutex* const mutex, absl::Condition const& condition,
                                  absl::Time const deadline) const {
  return const_cast<MockClock*>(this)->AwaitWithDeadlineInternal(mutex, condition, deadline);
}

void MockClock::SetTime(absl::Time const time) {
  std::vector<TimeListener*> listeners;
  {
    absl::MutexLock lock{&mutex_};
    CHECK_GE(time, current_time_) << "MockClock's time cannot move backward!";
    current_time_ = time;
    listeners = GetDeadlinedListeners();
  }
  for (auto const listener : listeners) {
    listener->Notify();
  }
}

void MockClock::AdvanceTime(absl::Duration const delta) {
  std::vector<TimeListener*> listeners;
  {
    absl::MutexLock lock{&mutex_};
    current_time_ += delta;
    listeners = GetDeadlinedListeners();
  }
  for (auto const listener : listeners) {
    listener->Notify();
  }
}

bool MockClock::AddListener(absl::Time const deadline, TimeListener* const listener) {
  absl::MutexLock lock{&mutex_};
  if (current_time_ < deadline) {
    auto const [it, unused] = listeners_.try_emplace(deadline);
    it->second.emplace(listener);
    return true;
  } else {
    return false;
  }
}

bool MockClock::RemoveListener(absl::Time const deadline, TimeListener* const listener) {
  absl::MutexLock lock{&mutex_};
  auto const it = listeners_.find(deadline);
  if (it != listeners_.end()) {
    return it->second.erase(listener) > 0;
  } else {
    return false;
  }
}

std::vector<MockClock::TimeListener*> MockClock::GetDeadlinedListeners() {
  std::vector<TimeListener*> result;
  for (auto it = listeners_.begin(); it != listeners_.end() && it->first <= current_time_;
       it = listeners_.erase(it)) {
    auto const& listener_set = it->second;
    result.insert(result.end(), listener_set.begin(), listener_set.end());
  }
  return result;
}

bool MockClock::AwaitWithDeadlineInternal(absl::Mutex* const mutex,
                                          absl::Condition const& condition,
                                          absl::Time const deadline) {
  TimeListener listener{mutex};
  listener.set_notified(!AddListener(deadline, &listener));
  mutex->Await(SimpleCondition([&] { return listener.is_notified() || condition.Eval(); }));
  if (!RemoveListener(deadline, &listener)) {
    mutex->Await(absl::Condition(&listener, &TimeListener::is_notified));
  }
  return !listener.is_notified();
}

}  // namespace common
}  // namespace tsdb2
