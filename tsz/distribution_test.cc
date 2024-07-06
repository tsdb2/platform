#include "tsz/distribution.h"

#include <cstdint>
#include <functional>

#include "absl/functional/bind_front.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::tsz::Bucketer;
using ::tsdb2::tsz::Distribution;

class CanonicalBucketerTest : public ::testing::TestWithParam<std::function<Bucketer const&()>> {};

TEST_P(CanonicalBucketerTest, Canonical) {
  auto const& bucketer1 = GetParam()();
  auto const& bucketer2 = GetParam()();
  EXPECT_EQ(&bucketer1, &bucketer2);
}

INSTANTIATE_TEST_SUITE_P(CanonicalBucketerTest, CanonicalBucketerTest,
                         ::testing::Values(absl::bind_front(&Bucketer::FixedWidth, 1, 10),
                                           absl::bind_front(&Bucketer::PowersOf, 2.0),
                                           absl::bind_front(&Bucketer::ScaledPowersOf, 2, 3, 1e6),
                                           absl::bind_front(&Bucketer::Custom, 1, 2, 0.5, 20),
                                           absl::bind_front(&Bucketer::Default),
                                           absl::bind_front(&Bucketer::None)));

TEST(BucketerTest, Custom) {
  auto const& bucketer = Bucketer::Custom(1, 2, 0.5, 20);
  EXPECT_EQ(bucketer.width(), 1.0);
  EXPECT_EQ(bucketer.growth_factor(), 2.0);
  EXPECT_EQ(bucketer.scale_factor(), 0.5);
  EXPECT_EQ(bucketer.num_finite_buckets(), 20);
}

TEST(BucketerTest, Default) { EXPECT_EQ(Bucketer::Default(), Bucketer::PowersOf(4)); }

TEST(BucketerTest, None) {
  auto const& bucketer = Bucketer::None();
  EXPECT_EQ(bucketer.width(), 0);
  EXPECT_EQ(bucketer.growth_factor(), 0);
  EXPECT_EQ(bucketer.scale_factor(), 0);
  EXPECT_EQ(bucketer.num_finite_buckets(), 0);
}

class GetBucketTest : public ::testing::Test {
 protected:
  Bucketer const& bucketer_ = Bucketer::Custom(1, 0, 1, 5);
};

TEST_F(GetBucketTest, Underflow) {
  EXPECT_EQ(bucketer_.GetBucketFor(-0.1), -1);
  EXPECT_EQ(bucketer_.GetBucketFor(-1), -1);
  EXPECT_EQ(bucketer_.GetBucketFor(-1.5), -1);
  EXPECT_EQ(bucketer_.GetBucketFor(-2), -1);
}

TEST_F(GetBucketTest, Buckets) {
  EXPECT_EQ(bucketer_.GetBucketFor(0), 0);
  EXPECT_EQ(bucketer_.GetBucketFor(0.5), 0);
  EXPECT_EQ(bucketer_.GetBucketFor(0.9), 0);
  EXPECT_EQ(bucketer_.GetBucketFor(1), 1);
  EXPECT_EQ(bucketer_.GetBucketFor(1.5), 1);
  EXPECT_EQ(bucketer_.GetBucketFor(1.9), 1);
  EXPECT_EQ(bucketer_.GetBucketFor(2), 2);
  EXPECT_EQ(bucketer_.GetBucketFor(2.5), 2);
  EXPECT_EQ(bucketer_.GetBucketFor(2.9), 2);
  EXPECT_EQ(bucketer_.GetBucketFor(3), 3);
  EXPECT_EQ(bucketer_.GetBucketFor(3.5), 3);
  EXPECT_EQ(bucketer_.GetBucketFor(3.9), 3);
  EXPECT_EQ(bucketer_.GetBucketFor(4), 4);
  EXPECT_EQ(bucketer_.GetBucketFor(4.5), 4);
  EXPECT_EQ(bucketer_.GetBucketFor(4.9), 4);
}

TEST_F(GetBucketTest, Overflow) {
  EXPECT_EQ(bucketer_.GetBucketFor(5), 5);
  EXPECT_EQ(bucketer_.GetBucketFor(5.5), 5);
  EXPECT_EQ(bucketer_.GetBucketFor(6), 5);
  EXPECT_EQ(bucketer_.GetBucketFor(7), 5);
}

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

}  // namespace
