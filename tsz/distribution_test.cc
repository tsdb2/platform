#include "tsz/distribution.h"

#include <cstddef>
#include <utility>

#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/bucketer.h"

namespace {

using ::tsz::Bucketer;
using ::tsz::Distribution;

TEST(DistributionTest, Bucketer) {
  auto const& bucketer = Bucketer::Custom(1, 2, 0.5, 20);
  Distribution distribution{bucketer};
  EXPECT_EQ(distribution.bucketer(), bucketer);
  EXPECT_EQ(distribution.num_finite_buckets(), bucketer.num_finite_buckets());
}

TEST(DistributionTest, DefaultBucketer) {
  Distribution distribution;
  EXPECT_EQ(distribution.bucketer(), Bucketer::Default());
  EXPECT_EQ(distribution.num_finite_buckets(), Bucketer::Default().num_finite_buckets());
}

TEST(DistributionTest, InitialState) {
  Distribution distribution;
  for (size_t i = 0; i < distribution.num_finite_buckets(); ++i) {
    EXPECT_EQ(distribution.bucket(i), 0);
  }
  EXPECT_EQ(distribution.underflow(), 0);
  EXPECT_EQ(distribution.overflow(), 0);
  EXPECT_EQ(distribution.sum(), 0);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution.count(), 0);
  EXPECT_TRUE(distribution.empty());
}

TEST(DistributionTest, RecordOneSample) {
  Distribution distribution;
  distribution.Record(42);
  EXPECT_EQ(distribution.bucket(3), 1);
  EXPECT_EQ(distribution.sum(), 42);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution.count(), 1);
  EXPECT_FALSE(distribution.empty());
  EXPECT_EQ(distribution.mean(), 42);
}

TEST(DistributionTest, RecordTwoSamples) {
  Distribution distribution;
  distribution.Record(1);
  distribution.Record(5);
  EXPECT_EQ(distribution.bucket(1), 1);
  EXPECT_EQ(distribution.bucket(2), 1);
  EXPECT_EQ(distribution.sum(), 6);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution.count(), 2);
  EXPECT_FALSE(distribution.empty());
  EXPECT_EQ(distribution.mean(), 3);
}

TEST(DistributionTest, RecordOneSampleManyTimes) {
  Distribution distribution;
  distribution.Record(1);
  distribution.RecordMany(5, 3);
  EXPECT_EQ(distribution.bucket(1), 1);
  EXPECT_EQ(distribution.bucket(2), 3);
  EXPECT_EQ(distribution.sum(), 16);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 12);
  EXPECT_EQ(distribution.count(), 4);
  EXPECT_FALSE(distribution.empty());
  EXPECT_EQ(distribution.mean(), 4);
}

TEST(DistributionTest, AddEmptyToEmpty) {
  Distribution distribution1, distribution2;
  EXPECT_OK(distribution1.Add(distribution2));
  EXPECT_EQ(distribution1.num_finite_buckets(), Bucketer::Default().num_finite_buckets());
  for (size_t i = 0; i < distribution1.num_finite_buckets(); ++i) {
    EXPECT_EQ(distribution1.bucket(i), 0);
  }
  EXPECT_EQ(distribution1.underflow(), 0);
  EXPECT_EQ(distribution1.overflow(), 0);
  EXPECT_EQ(distribution1.count(), 0);
  EXPECT_EQ(distribution1.sum(), 0);
  EXPECT_EQ(distribution1.mean(), 0);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 0);
}

TEST(DistributionTest, AddEmpty) {
  Distribution distribution1;
  distribution1.Record(2);
  distribution1.Record(4);
  distribution1.Record(6);
  distribution1.Record(8);
  distribution1.Record(10);
  Distribution distribution2;
  EXPECT_OK(distribution1.Add(distribution2));
  EXPECT_EQ(distribution1.num_finite_buckets(), Bucketer::Default().num_finite_buckets());
  EXPECT_EQ(distribution1.bucket(0), 0);
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 4);
  for (size_t i = 3; i < distribution1.num_finite_buckets(); ++i) {
    EXPECT_EQ(distribution1.bucket(i), 0);
  }
  EXPECT_EQ(distribution1.sum(), 30);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 40);
  EXPECT_EQ(distribution1.count(), 5);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 6);
}

TEST(DistributionTest, AddToEmpty) {
  Distribution distribution1;
  Distribution distribution2;
  distribution2.Record(2);
  distribution2.Record(4);
  distribution2.Record(6);
  distribution2.Record(8);
  distribution2.Record(10);
  EXPECT_OK(distribution1.Add(distribution2));
  EXPECT_EQ(distribution1.num_finite_buckets(), Bucketer::Default().num_finite_buckets());
  EXPECT_EQ(distribution1.bucket(0), 0);
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 4);
  for (size_t i = 3; i < distribution1.num_finite_buckets(); ++i) {
    EXPECT_EQ(distribution1.bucket(i), 0);
  }
  EXPECT_EQ(distribution1.sum(), 30);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 40);
  EXPECT_EQ(distribution1.count(), 5);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 6);
}

TEST(DistributionTest, Add) {
  Distribution distribution1;
  distribution1.Record(2);
  distribution1.Record(4);
  distribution1.Record(6);
  distribution1.Record(8);
  distribution1.Record(10);
  Distribution distribution2;
  distribution2.Record(1);
  distribution2.Record(3);
  distribution2.Record(5);
  distribution2.Record(7);
  distribution2.Record(9);
  distribution2.Record(11);
  EXPECT_OK(distribution1.Add(distribution2));
  EXPECT_EQ(distribution1.num_finite_buckets(), Bucketer::Default().num_finite_buckets());
  EXPECT_EQ(distribution1.bucket(0), 0);
  EXPECT_EQ(distribution1.bucket(1), 3);
  EXPECT_EQ(distribution1.bucket(2), 8);
  for (size_t i = 3; i < distribution1.num_finite_buckets(); ++i) {
    EXPECT_EQ(distribution1.bucket(i), 0);
  }
  EXPECT_EQ(distribution1.sum(), 66);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 110);
  EXPECT_EQ(distribution1.count(), 11);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 6);
}

TEST(DistributionTest, Clear) {
  Distribution distribution;
  distribution.Record(1);
  distribution.Record(5);
  distribution.Clear();
  EXPECT_EQ(distribution.bucket(1), 0);
  EXPECT_EQ(distribution.bucket(2), 0);
  EXPECT_EQ(distribution.sum(), 0);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution.count(), 0);
  EXPECT_TRUE(distribution.empty());
}

TEST(DistributionTest, RecordAfterClearing) {
  Distribution distribution;
  distribution.Record(1);
  distribution.Record(5);
  distribution.Clear();
  distribution.Record(42);
  EXPECT_EQ(distribution.bucket(3), 1);
  EXPECT_EQ(distribution.sum(), 42);
  EXPECT_EQ(distribution.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution.count(), 1);
  EXPECT_FALSE(distribution.empty());
  EXPECT_EQ(distribution.mean(), 42);
}

TEST(DistributionTest, Copy) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2;
  distribution2.Record(1);
  distribution2.Record(5);
  distribution1 = distribution2;
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 1);
  EXPECT_EQ(distribution1.sum(), 6);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution1.count(), 2);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 3);
  EXPECT_EQ(distribution2.bucket(1), 1);
  EXPECT_EQ(distribution2.bucket(2), 1);
  EXPECT_EQ(distribution2.sum(), 6);
  EXPECT_EQ(distribution2.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution2.count(), 2);
  EXPECT_FALSE(distribution2.empty());
  EXPECT_EQ(distribution2.mean(), 3);
}

TEST(DistributionTest, CopyConstruct) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2{distribution1};
  EXPECT_EQ(distribution1.bucket(3), 1);
  EXPECT_EQ(distribution1.sum(), 42);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution1.count(), 1);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 42);
  EXPECT_EQ(distribution2.bucket(3), 1);
  EXPECT_EQ(distribution2.sum(), 42);
  EXPECT_EQ(distribution2.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution2.count(), 1);
  EXPECT_FALSE(distribution2.empty());
  EXPECT_EQ(distribution2.mean(), 42);
}

TEST(DistributionTest, Move) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2;
  distribution2.Record(1);
  distribution2.Record(5);
  distribution1 = std::move(distribution2);
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 1);
  EXPECT_EQ(distribution1.sum(), 6);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution1.count(), 2);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 3);
}

TEST(DistributionTest, MoveConstruct) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2{std::move(distribution1)};
  EXPECT_EQ(distribution2.bucket(3), 1);
  EXPECT_EQ(distribution2.sum(), 42);
  EXPECT_EQ(distribution2.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution2.count(), 1);
  EXPECT_FALSE(distribution2.empty());
  EXPECT_EQ(distribution2.mean(), 42);
}

TEST(DistributionTest, Swap) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2;
  distribution2.Record(1);
  distribution2.Record(5);
  distribution1.swap(distribution2);
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 1);
  EXPECT_EQ(distribution1.sum(), 6);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution1.count(), 2);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 3);
  EXPECT_EQ(distribution2.bucket(3), 1);
  EXPECT_EQ(distribution2.sum(), 42);
  EXPECT_EQ(distribution2.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution2.count(), 1);
  EXPECT_FALSE(distribution2.empty());
  EXPECT_EQ(distribution2.mean(), 42);
}

TEST(DistributionTest, AdlSwap) {
  Distribution distribution1;
  distribution1.Record(42);
  Distribution distribution2;
  distribution2.Record(1);
  distribution2.Record(5);
  swap(distribution1, distribution2);
  EXPECT_EQ(distribution1.bucket(1), 1);
  EXPECT_EQ(distribution1.bucket(2), 1);
  EXPECT_EQ(distribution1.sum(), 6);
  EXPECT_EQ(distribution1.sum_of_squared_deviations(), 8);
  EXPECT_EQ(distribution1.count(), 2);
  EXPECT_FALSE(distribution1.empty());
  EXPECT_EQ(distribution1.mean(), 3);
  EXPECT_EQ(distribution2.bucket(3), 1);
  EXPECT_EQ(distribution2.sum(), 42);
  EXPECT_EQ(distribution2.sum_of_squared_deviations(), 0);
  EXPECT_EQ(distribution2.count(), 1);
  EXPECT_FALSE(distribution2.empty());
  EXPECT_EQ(distribution2.mean(), 42);
}

}  // namespace
