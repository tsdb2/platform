#include "common/sleep.h"

#include "absl/log/check.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/mock_clock.h"
#include "common/scheduler.h"
#include "common/scoped_override.h"
#include "common/singleton.h"
#include "common/testing.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MockClock;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::Singleton;
using ::tsdb2::common::Sleep;

class SleepTest : public ::testing::Test {
 protected:
  explicit SleepTest() {
    clock_.AdvanceTime(absl::Seconds(123));
    CHECK_OK(scheduler_.WaitUntilAllWorkersAsleep());
  }

  MockClock clock_;
  Scheduler scheduler_{Scheduler::Options{
      .clock = &clock_,
      .start_now = true,
  }};
  ScopedOverride<Singleton<Scheduler>> scheduler_override_{&tsdb2::common::default_scheduler,
                                                           &scheduler_};
};

TEST_F(SleepTest, Sleep10s) {
  bool done = false;
  auto const p = Sleep(absl::Seconds(10)).Then([&] { done = true; });
  clock_.AdvanceTime(absl::Seconds(9));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_FALSE(done);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done);
}

TEST_F(SleepTest, Sleep0s) {
  absl::Notification done;
  auto const p = Sleep(absl::ZeroDuration()).Then([&] { done.Notify(); });
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  done.WaitForNotification();
}

TEST_F(SleepTest, SleepNegative) {
  absl::Notification done;
  auto const p = Sleep(absl::Seconds(-10)).Then([&] { done.Notify(); });
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  done.WaitForNotification();
}

}  // namespace
