#include "common/mock_clock.h"

#include <thread>

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::tsdb2::common::MockClock;

TEST(MockClockTest, InitialTime) {
  auto const time = absl::UnixEpoch() + absl::Seconds(123);
  MockClock clock{time};
  EXPECT_EQ(clock.TimeNow(), time);
}

TEST(MockClockTest, DefaultInitialTime) {
  MockClock clock;
  EXPECT_EQ(clock.TimeNow(), absl::UnixEpoch());
}

TEST(MockClockTest, SetTime) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  clock.SetTime(absl::UnixEpoch() + absl::Seconds(34));
  EXPECT_EQ(clock.TimeNow(), absl::UnixEpoch() + absl::Seconds(34));
}

TEST(MockClockDeathTest, SetTimeBackwards) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(34)};
  EXPECT_DEATH(clock.SetTime(absl::UnixEpoch() + absl::Seconds(12)), _);
}

TEST(MockClockTest, AdvanceTime) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  clock.AdvanceTime(absl::Seconds(34));
  EXPECT_EQ(clock.TimeNow(), absl::UnixEpoch() + absl::Seconds(46));
}

TEST(MockClockTest, AdvanceTimeTwice) {
  MockClock clock;
  clock.AdvanceTime(absl::Seconds(12));
  clock.AdvanceTime(absl::Seconds(34));
  EXPECT_EQ(clock.TimeNow(), absl::UnixEpoch() + absl::Seconds(46));
}

TEST(MockClockTest, SleepUntilBefore) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  auto const deadline = absl::UnixEpoch() + absl::Seconds(12) + absl::Seconds(34);
  absl::Notification started;
  std::thread thread{[&] {
    started.Notify();
    clock.SleepUntil(deadline);
  }};
  started.WaitForNotification();
  clock.AdvanceTime(absl::Seconds(34));
  thread.join();
}

TEST(MockClockTest, SleepUntilAfter) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  auto const deadline = absl::UnixEpoch() + absl::Seconds(12) + absl::Seconds(34);
  absl::Notification waiting;
  std::thread thread{[&] {
    waiting.WaitForNotification();
    clock.SleepUntil(deadline);
  }};
  clock.AdvanceTime(absl::Seconds(34));
  waiting.Notify();
  thread.join();
}

TEST(MockClockTest, AwaitWithTimeout) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  absl::Mutex mutex;
  bool started = false;
  bool const guarded = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    started = true;
    clock.AwaitWithTimeout(&mutex, absl::Condition(&guarded), absl::Seconds(34));
  }};
  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(&started));
  }
  clock.AdvanceTime(absl::Seconds(34));
  thread.join();
}

TEST(MockClockTest, AwaitWithDeadline) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  absl::Mutex mutex;
  bool started = false;
  bool const guarded = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    started = true;
    clock.AwaitWithDeadline(&mutex, absl::Condition(&guarded),
                            absl::UnixEpoch() + absl::Seconds(34));
  }};
  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(&started));
  }
  clock.AdvanceTime(absl::Seconds(34));
  thread.join();
}

TEST(MockClockTest, AwaitWithCondition) {
  MockClock clock{absl::UnixEpoch() + absl::Seconds(12)};
  absl::Mutex mutex;
  bool started = false;
  bool guarded = false;
  bool finished = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    started = true;
    clock.AwaitWithTimeout(&mutex, absl::Condition(&guarded), absl::Seconds(56));
    finished = true;
  }};
  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(&started));
  }
  clock.AdvanceTime(absl::Seconds(34));
  {
    absl::MutexLock lock{&mutex};
    EXPECT_FALSE(finished);
    guarded = true;
  }
  thread.join();
}

}  // namespace
