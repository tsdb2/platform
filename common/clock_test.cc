#include "common/clock.h"

#include <thread>

#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::RealClock;

TEST(ClockTest, TimeNow) {
  auto const now = absl::Now();
  EXPECT_GE(RealClock::GetInstance()->TimeNow(), now);
}

TEST(ClockTest, Await) {
  absl::Mutex mutex;
  bool started = false;
  bool finish = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    started = true;
    EXPECT_TRUE(RealClock::GetInstance()->AwaitWithTimeout(&mutex, absl::Condition(&finish),
                                                           absl::Seconds(10)));
  }};
  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(&started));
    EXPECT_FALSE(finish);
    finish = true;
  }
  thread.join();
}

}  // namespace
