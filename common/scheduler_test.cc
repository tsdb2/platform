#include "common/scheduler.h"

#include <atomic>
#include <cstdint>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "common/mock_clock.h"
#include "common/simple_condition.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MockClock;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::SimpleCondition;

class SchedulerTest : public ::testing::TestWithParam<int> {
 protected:
  explicit SchedulerTest(bool const start_now)
      : scheduler_{{
            .num_workers = static_cast<uint16_t>(GetParam()),
            .clock = &clock_,
            .start_now = start_now,
        }} {}

  MockClock clock_;
  Scheduler scheduler_;
};

class SchedulerStateTest : public SchedulerTest {
 protected:
  explicit SchedulerStateTest() : SchedulerTest(/*start_now=*/false) {}
};

TEST_P(SchedulerStateTest, Clock) { EXPECT_EQ(scheduler_.clock(), &clock_); }

TEST_P(SchedulerStateTest, Idle) { EXPECT_EQ(scheduler_.state(), Scheduler::State::IDLE); }

TEST_P(SchedulerStateTest, Started) {
  scheduler_.Start();
  EXPECT_EQ(scheduler_.state(), Scheduler::State::STARTED);
}

TEST_P(SchedulerStateTest, StartedAgain) {
  scheduler_.Start();
  scheduler_.Start();
  EXPECT_EQ(scheduler_.state(), Scheduler::State::STARTED);
}

TEST_P(SchedulerStateTest, StartAfterScheduling) {
  absl::Notification done;
  scheduler_.ScheduleNow([&] {
    EXPECT_EQ(scheduler_.state(), Scheduler::State::STARTED);
    done.Notify();
  });
  scheduler_.Start();
  done.WaitForNotification();
}

TEST_P(SchedulerStateTest, Stopped) {
  scheduler_.Start();
  scheduler_.Stop();
  EXPECT_EQ(scheduler_.state(), Scheduler::State::STOPPED);
}

TEST_P(SchedulerStateTest, Stop) {
  scheduler_.Start();
  absl::Mutex mutex;
  bool started = false;
  bool stopped = false;
  scheduler_.ScheduleNow([&] {
    while (scheduler_.state() == Scheduler::State::STARTED) {
      absl::MutexLock lock{&mutex};
      started = true;
    }
    EXPECT_EQ(scheduler_.state(), Scheduler::State::STOPPING);
    stopped = true;
  });
  {
    absl::MutexLock lock{&mutex};
    mutex.Await(absl::Condition(&started));
  }
  scheduler_.Stop();
  EXPECT_EQ(scheduler_.state(), Scheduler::State::STOPPED);
  EXPECT_TRUE(stopped);
}

TEST_P(SchedulerStateTest, StoppedButNotStarted) {
  scheduler_.Stop();
  EXPECT_EQ(scheduler_.state(), Scheduler::State::STOPPED);
}

INSTANTIATE_TEST_SUITE_P(SchedulerStateTest, SchedulerStateTest, ::testing::Range(1, 10));

class SchedulerTaskTest : public SchedulerTest {
 protected:
  explicit SchedulerTaskTest() : SchedulerTest(/*start_now=*/true) {
    clock_.AdvanceTime(absl::Seconds(12));
    WaitUntilAllWorkersAsleep();
  }

  Scheduler::Handle ScheduleAt(absl::Duration const due_time, Scheduler::Callback callback) {
    return scheduler_.ScheduleAt(std::move(callback), absl::UnixEpoch() + due_time);
  }

  Scheduler::Handle ScheduleAt(absl::Duration const due_time, Scheduler::Callback callback,
                               absl::Duration const period) {
    return scheduler_.ScheduleRecurringAt(std::move(callback), absl::UnixEpoch() + due_time,
                                          period);
  }

  void WaitUntilAllWorkersAsleep() const { ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep()); }
};

TEST_P(SchedulerTaskTest, ScheduleNow) {
  absl::Notification done;
  ScheduleAt(absl::Seconds(12), [&] { done.Notify(); });
  done.WaitForNotification();
}

TEST_P(SchedulerTaskTest, SchedulePast) {
  absl::Notification done;
  ScheduleAt(absl::Seconds(10), [&] { done.Notify(); });
  done.WaitForNotification();
}

TEST_P(SchedulerTaskTest, ScheduleFuture) {
  absl::Notification done;
  ScheduleAt(absl::Seconds(34), [&] { done.Notify(); });
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(done.HasBeenNotified());
}

TEST_P(SchedulerTaskTest, AdvanceTime) {
  absl::Notification done;
  ScheduleAt(absl::Seconds(34), [&] { done.Notify(); });
  clock_.AdvanceTime(absl::Seconds(22));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(done.HasBeenNotified());
}

TEST_P(SchedulerTaskTest, AdvanceFurther) {
  absl::Notification done;
  ScheduleAt(absl::Seconds(34), [&] { done.Notify(); });
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(done.HasBeenNotified());
}

TEST_P(SchedulerTaskTest, Preempt) {
  bool run1 = false;
  bool run2 = false;
  ScheduleAt(absl::Seconds(56), [&] { run1 = true; });
  ScheduleAt(absl::Seconds(34), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(25));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run1);
  EXPECT_TRUE(run2);
  clock_.AdvanceTime(absl::Seconds(25));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run1);
  EXPECT_TRUE(run2);
}

TEST_P(SchedulerTaskTest, ParallelRun) {
  bool run1 = false;
  bool run2 = false;
  ScheduleAt(absl::Seconds(34), [&] { run1 = true; });
  ScheduleAt(absl::Seconds(56), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(50));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run1);
  EXPECT_TRUE(run2);
}

TEST_P(SchedulerTaskTest, ParallelRunPreempted) {
  bool run1 = false;
  bool run2 = false;
  ScheduleAt(absl::Seconds(56), [&] { run1 = true; });
  ScheduleAt(absl::Seconds(34), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(50));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run1);
  EXPECT_TRUE(run2);
}

TEST_P(SchedulerTaskTest, PreemptNPlusTwo) {
  std::atomic<int> run{0};
  for (int i = GetParam() + 2; i > 0; --i) {
    ScheduleAt(absl::Seconds(34) * i, [&] { run.fetch_add(1, std::memory_order_relaxed); });
  }
  clock_.AdvanceTime(absl::Seconds(34) * (GetParam() + 2));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(run, GetParam() + 2);
}

TEST_P(SchedulerTaskTest, CancelBefore) {
  bool run = false;
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run = true; });
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_FALSE(run);
  EXPECT_TRUE(scheduler_.Cancel(handle));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, CancelAfter) {
  auto const handle = ScheduleAt(absl::Seconds(34), [] {});
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(scheduler_.Cancel(handle));
}

TEST_P(SchedulerTaskTest, CancelWhileRunning) {
  absl::Notification started;
  absl::Notification unblock;
  absl::Notification exiting;
  auto const handle = ScheduleAt(absl::Seconds(34), [&] {
    started.Notify();
    unblock.WaitForNotification();
    exiting.Notify();
  });
  clock_.AdvanceTime(absl::Seconds(56));
  started.WaitForNotification();
  EXPECT_FALSE(scheduler_.Cancel(handle));
  unblock.Notify();
  exiting.WaitForNotification();
}

TEST_P(SchedulerTaskTest, CancelInvalid) {
  bool run = false;
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run = true; });
  ASSERT_NE(handle, 42);
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_FALSE(run);
  EXPECT_FALSE(scheduler_.Cancel(42));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run);
}

TEST_P(SchedulerTaskTest, CancelTwice) {
  bool run = false;
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run = true; });
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_FALSE(run);
  ASSERT_TRUE(scheduler_.Cancel(handle));
  EXPECT_FALSE(scheduler_.Cancel(handle));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, CancelOne) {
  bool run1 = false;
  bool run2 = false;
  ScheduleAt(absl::Seconds(56), [&] { run1 = true; });
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_TRUE(scheduler_.Cancel(handle));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run1);
  EXPECT_FALSE(run2);
}

TEST_P(SchedulerTaskTest, CancelBoth) {
  bool run1 = false;
  bool run2 = false;
  auto const handle1 = ScheduleAt(absl::Seconds(56), [&] { run1 = true; });
  auto const handle2 = ScheduleAt(absl::Seconds(56), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_TRUE(scheduler_.Cancel(handle1));
  ASSERT_TRUE(scheduler_.Cancel(handle2));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run1);
  EXPECT_FALSE(run2);
}

TEST_P(SchedulerTaskTest, PreemptionCancelled) {
  bool run1 = false;
  bool run2 = false;
  ScheduleAt(absl::Seconds(78), [&] { run1 = true; });
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run2 = true; });
  clock_.AdvanceTime(absl::Seconds(17));
  WaitUntilAllWorkersAsleep();
  ASSERT_TRUE(scheduler_.Cancel(handle));
  clock_.AdvanceTime(absl::Seconds(17));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run1);
  EXPECT_FALSE(run2);
  clock_.AdvanceTime(absl::Seconds(90));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run1);
  EXPECT_FALSE(run2);
}

TEST_P(SchedulerTaskTest, BlockingCancelBefore) {
  bool run = false;
  auto const handle = ScheduleAt(absl::Seconds(56), [&] { run = true; });
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_FALSE(run);
  EXPECT_TRUE(scheduler_.BlockingCancel(handle));
  clock_.AdvanceTime(absl::Seconds(78));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, BlockingCancelAfter) {
  auto const handle = ScheduleAt(absl::Seconds(34), [] {});
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(scheduler_.BlockingCancel(handle));
}

TEST_P(SchedulerTaskTest, BlockCancellationDuringExecution) {
  absl::Notification started;
  absl::Notification unblock;
  absl::Notification cancelling;
  absl::Notification cancelled;
  auto const handle = ScheduleAt(absl::Seconds(34), [&] {
    started.Notify();
    unblock.WaitForNotification();
  });
  clock_.AdvanceTime(absl::Seconds(56));
  started.WaitForNotification();
  std::thread canceller{[&] {
    cancelling.Notify();
    EXPECT_FALSE(scheduler_.BlockingCancel(handle));
    cancelled.Notify();
  }};
  cancelling.WaitForNotification();
  EXPECT_FALSE(cancelled.HasBeenNotified());
  unblock.Notify();
  canceller.join();
}

TEST_P(SchedulerTaskTest, Recurring) {
  int runs = 0;
  scheduler_.ScheduleRecurring([&] { ++runs; }, absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 1);
  clock_.AdvanceTime(absl::Seconds(30));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 1);
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 2);
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 3);
}

TEST_P(SchedulerTaskTest, RecurringWithDelay) {
  int runs = 0;
  scheduler_.ScheduleRecurringIn([&] { ++runs; }, /*delay=*/absl::Seconds(34),
                                 /*period=*/absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 0);
  clock_.AdvanceTime(absl::Seconds(30));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 0);
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 1);
  clock_.AdvanceTime(absl::Seconds(50));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 1);
  clock_.AdvanceTime(absl::Seconds(6));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 2);
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 3);
}

TEST_P(SchedulerTaskTest, CancelRecurring) {
  int runs = 0;
  auto const handle = scheduler_.ScheduleRecurringIn([&] { ++runs; }, /*delay=*/absl::Seconds(34),
                                                     /*period=*/absl::Seconds(56));
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  ASSERT_EQ(runs, 1);
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  ASSERT_EQ(runs, 2);
  scheduler_.Cancel(handle);
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 2);
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
  EXPECT_EQ(runs, 2);
}

TEST_P(SchedulerTaskTest, EmptyScopedHandle) {
  Scheduler::ScopedHandle handle;
  EXPECT_TRUE(handle.empty());
  EXPECT_FALSE(handle.operator bool());
  EXPECT_EQ(handle.parent(), nullptr);
  EXPECT_EQ(handle.value(), Scheduler::kInvalidHandle);
  EXPECT_EQ(*handle, Scheduler::kInvalidHandle);
}

TEST_P(SchedulerTaskTest, CancelledByScopedHandle) {
  bool run = false;
  {
    auto const handle = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
    EXPECT_FALSE(handle.empty());
    EXPECT_TRUE(handle.operator bool());
    EXPECT_EQ(handle.parent(), &scheduler_);
    EXPECT_NE(handle.value(), Scheduler::kInvalidHandle);
    EXPECT_NE(*handle, Scheduler::kInvalidHandle);
    clock_.AdvanceTime(absl::Seconds(30));
    WaitUntilAllWorkersAsleep();
  }
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, CancelScopedHandle) {
  bool run = false;
  auto handle = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
  clock_.AdvanceTime(absl::Seconds(30));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(handle.Cancel());
  EXPECT_TRUE(handle.empty());
  EXPECT_FALSE(handle.operator bool());
  EXPECT_EQ(handle.parent(), nullptr);
  EXPECT_EQ(handle.value(), Scheduler::kInvalidHandle);
  EXPECT_EQ(*handle, Scheduler::kInvalidHandle);
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, BlockingCancelScopedHandle) {
  absl::Notification started;
  absl::Notification unblock;
  absl::Notification cancelling;
  absl::Notification cancelled;
  auto handle = scheduler_.ScheduleScopedIn(
      [&] {
        started.Notify();
        unblock.WaitForNotification();
      },
      absl::Seconds(34));
  clock_.AdvanceTime(absl::Seconds(56));
  started.WaitForNotification();
  std::thread canceller{[&] {
    cancelling.Notify();
    EXPECT_FALSE(handle.BlockingCancel());
    cancelled.Notify();
  }};
  cancelling.WaitForNotification();
  EXPECT_FALSE(cancelled.HasBeenNotified());
  unblock.Notify();
  canceller.join();
  EXPECT_TRUE(handle.empty());
  EXPECT_FALSE(handle.operator bool());
  EXPECT_EQ(handle.parent(), nullptr);
  EXPECT_EQ(handle.value(), Scheduler::kInvalidHandle);
  EXPECT_EQ(*handle, Scheduler::kInvalidHandle);
}

TEST_P(SchedulerTaskTest, MoveConstructScopedHandle) {
  bool run = false;
  auto handle1 = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
  clock_.AdvanceTime(absl::Seconds(30));
  WaitUntilAllWorkersAsleep();
  ASSERT_FALSE(run);
  {
    Scheduler::ScopedHandle handle2{std::move(handle1)};
    EXPECT_FALSE(handle2.empty());
    EXPECT_TRUE(handle2.operator bool());
    EXPECT_EQ(handle2.parent(), &scheduler_);
    EXPECT_NE(handle2.value(), Scheduler::kInvalidHandle);
    EXPECT_NE(*handle2, Scheduler::kInvalidHandle);
    EXPECT_TRUE(handle1.empty());
    EXPECT_FALSE(handle1.operator bool());
    EXPECT_EQ(handle1.parent(), nullptr);
    EXPECT_EQ(handle1.value(), Scheduler::kInvalidHandle);
    EXPECT_EQ(*handle1, Scheduler::kInvalidHandle);
  }
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_FALSE(run);
}

TEST_P(SchedulerTaskTest, MoveAssignScopedHandle) {
  bool run = false;
  Scheduler::ScopedHandle handle1;
  {
    auto handle2 = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
    clock_.AdvanceTime(absl::Seconds(30));
    WaitUntilAllWorkersAsleep();
    ASSERT_FALSE(run);
    handle1 = std::move(handle2);
    EXPECT_FALSE(handle1.empty());
    EXPECT_TRUE(handle1.operator bool());
    EXPECT_EQ(handle1.parent(), &scheduler_);
    EXPECT_NE(handle1.value(), Scheduler::kInvalidHandle);
    EXPECT_NE(*handle1, Scheduler::kInvalidHandle);
    EXPECT_TRUE(handle2.empty());
    EXPECT_FALSE(handle2.operator bool());
    EXPECT_EQ(handle2.parent(), nullptr);
    EXPECT_EQ(handle2.value(), Scheduler::kInvalidHandle);
    EXPECT_EQ(*handle2, Scheduler::kInvalidHandle);
  }
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run);
}

TEST_P(SchedulerTaskTest, CancelEmptyScopedHandle) {
  bool run = false;
  Scheduler::ScopedHandle handle1;
  {
    auto handle2 = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
    clock_.AdvanceTime(absl::Seconds(30));
    WaitUntilAllWorkersAsleep();
    ASSERT_FALSE(run);
    handle1 = std::move(handle2);
    EXPECT_FALSE(handle2.Cancel());
    EXPECT_TRUE(handle2.empty());
    EXPECT_FALSE(handle2.operator bool());
    EXPECT_EQ(handle2.parent(), nullptr);
    EXPECT_EQ(handle2.value(), Scheduler::kInvalidHandle);
    EXPECT_EQ(*handle2, Scheduler::kInvalidHandle);
  }
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run);
}

TEST_P(SchedulerTaskTest, BlockingCancelEmptyScopedHandle) {
  bool run = false;
  Scheduler::ScopedHandle handle1;
  {
    auto handle2 = scheduler_.ScheduleScopedIn([&] { run = true; }, absl::Seconds(34));
    clock_.AdvanceTime(absl::Seconds(30));
    WaitUntilAllWorkersAsleep();
    ASSERT_FALSE(run);
    handle1 = std::move(handle2);
    EXPECT_FALSE(handle2.BlockingCancel());
    EXPECT_TRUE(handle2.empty());
    EXPECT_FALSE(handle2.operator bool());
    EXPECT_EQ(handle2.parent(), nullptr);
    EXPECT_EQ(handle2.value(), Scheduler::kInvalidHandle);
    EXPECT_EQ(*handle2, Scheduler::kInvalidHandle);
  }
  clock_.AdvanceTime(absl::Seconds(4));
  WaitUntilAllWorkersAsleep();
  EXPECT_TRUE(run);
}

TEST_P(SchedulerTaskTest, CurrentTaskHandle) {
  Scheduler::Handle const handle = scheduler_.ScheduleIn(
      [&] { EXPECT_EQ(handle, Scheduler::current_task_handle()); }, absl::Seconds(34));
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
}

TEST_P(SchedulerTaskTest, NextTaskHandle) {
  scheduler_.ScheduleIn([] {}, absl::Seconds(34));
  Scheduler::Handle const handle = scheduler_.ScheduleIn(
      [&] { EXPECT_EQ(handle, Scheduler::current_task_handle()); }, absl::Seconds(56));
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
  clock_.AdvanceTime(absl::Seconds(56));
  WaitUntilAllWorkersAsleep();
}

TEST_P(SchedulerTaskTest, ConcurrentTaskHandles) {
  Scheduler::Handle const handle1 = scheduler_.ScheduleIn(
      [&] { EXPECT_EQ(handle1, Scheduler::current_task_handle()); }, absl::Seconds(34));
  Scheduler::Handle const handle2 = scheduler_.ScheduleIn(
      [&] { EXPECT_EQ(handle2, Scheduler::current_task_handle()); }, absl::Seconds(34));
  clock_.AdvanceTime(absl::Seconds(34));
  WaitUntilAllWorkersAsleep();
}

INSTANTIATE_TEST_SUITE_P(SchedulerTaskTest, SchedulerTaskTest, ::testing::Range(1, 10));

}  // namespace
