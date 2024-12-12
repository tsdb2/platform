#include "common/clock.h"

#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

RealClock const* RealClock::GetInstance() {
  static gsl::owner<RealClock*> const kInstance = new RealClock();
  return kInstance;  // NOLINT(cppcoreguidelines-owning-memory)
}

absl::Time RealClock::TimeNow() const { return absl::Now(); }

void RealClock::SleepFor(absl::Duration const duration) const { absl::SleepFor(duration); }

void RealClock::SleepUntil(absl::Time const wakeup_time) const {
  auto const duration = wakeup_time - absl::Now();
  if (duration > absl::ZeroDuration()) {
    absl::SleepFor(duration);
  }
}

bool RealClock::AwaitWithTimeout(absl::Mutex* const mutex, absl::Condition const& condition,
                                 absl::Duration const timeout) const {
  return mutex->AwaitWithTimeout(condition, timeout);
}

bool RealClock::AwaitWithDeadline(absl::Mutex* const mutex, absl::Condition const& condition,
                                  absl::Time const deadline) const {
  return mutex->AwaitWithDeadline(condition, deadline);
}

}  // namespace common
}  // namespace tsdb2
