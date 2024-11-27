#include "common/periodic_thread.h"

#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/mock_clock.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MockClock;
using ::tsdb2::common::PeriodicClosure;

class PeriodicThreadTest : public ::testing::Test {
 public:
  explicit PeriodicThreadTest() { clock_.AdvanceTime(absl::Seconds(123)); }

 protected:
  MockClock clock_;
};

TEST_F(PeriodicThreadTest, NotStarted) {
  PeriodicClosure pc{PeriodicClosure::Options{
                         .period = absl::Seconds(10),
                         .clock = &clock_,
                     },
                     [] { FAIL(); }};
  EXPECT_EQ(pc.state(), PeriodicClosure::State::IDLE);
  clock_.AdvanceTime(absl::Seconds(11));
  ASSERT_OK(pc.WaitUntilAsleep());
}

TEST_F(PeriodicThreadTest, FirstRun) {
  int runs = 0;
  PeriodicClosure pc{PeriodicClosure::Options{
                         .period = absl::Seconds(10),
                         .clock = &clock_,
                     },
                     [&] { ++runs; }};
  pc.Start();
  EXPECT_EQ(pc.state(), PeriodicClosure::State::STARTED);
  ASSERT_OK(pc.WaitUntilAsleep());
  EXPECT_EQ(runs, 1);
}

TEST_F(PeriodicThreadTest, SecondRun) {
  int runs = 0;
  PeriodicClosure pc{PeriodicClosure::Options{
                         .period = absl::Seconds(10),
                         .clock = &clock_,
                     },
                     [&] { ++runs; }};
  pc.Start();
  ASSERT_OK(pc.WaitUntilAsleep());
  clock_.AdvanceTime(absl::Seconds(10));
  ASSERT_OK(pc.WaitUntilAsleep());
  EXPECT_EQ(runs, 2);
}

TEST_F(PeriodicThreadTest, StartLate) {
  int runs = 0;
  PeriodicClosure pc{PeriodicClosure::Options{
                         .period = absl::Seconds(10),
                         .clock = &clock_,
                     },
                     [&] { ++runs; }};
  clock_.AdvanceTime(absl::Seconds(3));
  pc.Start();
  ASSERT_OK(pc.WaitUntilAsleep());
  clock_.AdvanceTime(absl::Seconds(7));
  ASSERT_OK(pc.WaitUntilAsleep());
  EXPECT_EQ(runs, 2);
}

// TODO

}  // namespace
