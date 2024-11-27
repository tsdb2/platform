#include "tsz/bucketer.h"

#include <functional>

#include "absl/functional/bind_front.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsz::Bucketer;

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
  EXPECT_EQ(bucketer.GetBucketFor(-2), -1);
  EXPECT_EQ(bucketer.GetBucketFor(-1.5), -1);
  EXPECT_EQ(bucketer.GetBucketFor(-1), -1);
  EXPECT_EQ(bucketer.GetBucketFor(-0.5), -1);
  EXPECT_EQ(bucketer.GetBucketFor(0), 0);
  EXPECT_EQ(bucketer.GetBucketFor(0.5), 0);
  EXPECT_EQ(bucketer.GetBucketFor(1), 0);
  EXPECT_EQ(bucketer.GetBucketFor(1.5), 0);
  EXPECT_EQ(bucketer.GetBucketFor(2), 0);
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

}  // namespace
