#include "common/stats_counter.h"

#include <thread>

#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::StatsCounter;

TEST(StatsCounterTest, Default) {
  StatsCounter counter;
  EXPECT_EQ(counter.value(), 0);
}

TEST(StatsCounterTest, InitialValue) {
  StatsCounter counter{42};
  EXPECT_EQ(counter.value(), 42);
}

TEST(StatsCounterTest, IncrementOnce) {
  StatsCounter counter;
  counter.Increment();
  EXPECT_EQ(counter.value(), 1);
}

TEST(StatsCounterTest, IncrementTwice) {
  StatsCounter counter;
  counter.Increment();
  counter.Increment();
  EXPECT_EQ(counter.value(), 2);
}

TEST(StatsCounterTest, IncrementInitialValueOnce) {
  StatsCounter counter{42};
  counter.Increment();
  EXPECT_EQ(counter.value(), 43);
}

TEST(StatsCounterTest, IncrementInitialValueTwice) {
  StatsCounter counter{42};
  counter.Increment();
  counter.Increment();
  EXPECT_EQ(counter.value(), 44);
}

TEST(StatsCounterTest, PrefixIncrement) {
  StatsCounter counter{42};
  EXPECT_EQ(++counter, 43);
  EXPECT_EQ(counter.value(), 43);
}

TEST(StatsCounterTest, PostfixIncrement) {
  StatsCounter counter{42};
  EXPECT_EQ(counter++, 42);
  EXPECT_EQ(counter.value(), 43);
}

TEST(StatsCounterTest, IncrementByDeltaOnce) {
  StatsCounter counter;
  counter.IncrementBy(12);
  EXPECT_EQ(counter.value(), 12);
}

TEST(StatsCounterTest, IncrementByDeltaTwice) {
  StatsCounter counter;
  counter.IncrementBy(12);
  counter.IncrementBy(34);
  EXPECT_EQ(counter.value(), 46);
}

TEST(StatsCounterTest, IncrementInitialValueByDeltaOnce) {
  StatsCounter counter{42};
  counter.IncrementBy(12);
  EXPECT_EQ(counter.value(), 54);
}

TEST(StatsCounterTest, IncrementInitialValueByDeltaTwice) {
  StatsCounter counter{42};
  counter.IncrementBy(12);
  counter.IncrementBy(34);
  EXPECT_EQ(counter.value(), 88);
}

TEST(StatsCounterTest, CompoundAssignment) {
  StatsCounter counter{42};
  auto const value = (counter += 24).value();
  EXPECT_EQ(value, 66);
}

TEST(StatsCounterTest, Reset) {
  StatsCounter counter;
  counter.IncrementBy(123);
  counter.Reset();
  EXPECT_EQ(counter.value(), 0);
}

TEST(StatsCounterTest, FetchAndReset) {
  StatsCounter counter;
  counter.IncrementBy(42);
  EXPECT_EQ(counter.FetchAndReset(), 42);
  EXPECT_EQ(counter.value(), 0);
}

TEST(StatsCounterTest, ResetFromInitialValue) {
  StatsCounter counter{42};
  counter.IncrementBy(24);
  counter.Reset();
  EXPECT_EQ(counter.value(), 0);
}

TEST(StatsCounterTest, FetchAndResetFromInitialValue) {
  StatsCounter counter{42};
  counter.IncrementBy(24);
  EXPECT_EQ(counter.FetchAndReset(), 66);
  EXPECT_EQ(counter.value(), 0);
}

TEST(StatsCounterTest, IncrementAfterReset) {
  StatsCounter counter;
  counter.Increment();
  counter.Reset();
  counter.Increment();
  EXPECT_EQ(counter.value(), 1);
}

TEST(StatsCounterTest, IncrementByDeltaAfterReset) {
  StatsCounter counter;
  counter.IncrementBy(12);
  counter.Reset();
  counter.IncrementBy(34);
  EXPECT_EQ(counter.value(), 34);
}

TEST(StatsCounterTest, ConcurrentIncrements) {
  StatsCounter counter{345};
  auto const fn = [&] {
    for (int i = 0; i < 1000; ++i) {
      counter.Increment();
    }
  };
  std::thread thread1{fn};
  std::thread thread2{fn};
  thread1.join();
  thread2.join();
  EXPECT_EQ(counter.value(), 2345);
}

}  // namespace
