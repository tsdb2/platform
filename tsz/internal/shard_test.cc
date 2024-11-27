#include "tsz/internal/shard.h"

#include <cstdint>
#include <string_view>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/metric_config.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::DoubleNear;
using ::testing::Not;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::DoubleValue;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::Shard;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kIntMetricName = "/foo/bar/int";
std::string_view constexpr kDoubleMetricName = "/foo/bar/double";
std::string_view constexpr kDistributionMetricName = "/foo/bar/distribution";
std::string_view constexpr kUndefinedMetricName = "/foo/bar/baz";

class ShardTest : public ::testing::Test {
 protected:
  explicit ShardTest() {
    DefineMetric(kIntMetricName);
    DefineMetric(kDoubleMetricName);
    DefineMetric(kDistributionMetricName);
  }

  Bucketer const& bucketer_ = Bucketer::PowersOf(2.78);
  MetricConfig const metric_config_{.bucketer = &bucketer_};

  Shard shard_;

  FieldMap const entity_labels1_{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const entity_labels2_{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  FieldMap const metric_fields1_{
      {"foo", IntValue(42)},
      {"bar", BoolValue(false)},
  };
  FieldMap const metric_fields2_{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(true)},
  };

 private:
  void DefineMetric(std::string_view const metric_name) {
    auto const status = shard_.DefineMetric(metric_name, metric_config_);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to define metric \"" << metric_name << "\": " << status;
    }
  }
};

TEST_F(ShardTest, RedefineMetric) {
  EXPECT_THAT(shard_.DefineMetric(kIntMetricName, metric_config_),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST_F(ShardTest, DefineAnotherMetric) {
  EXPECT_OK(shard_.DefineMetric(kUndefinedMetricName, metric_config_));
}

TEST_F(ShardTest, InvalidMetricNameFormat) {
  EXPECT_THAT(shard_.DefineMetric("Lorem ipsum dolor amet.", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(shard_.DefineMetric("lorem/ipsum", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(shard_.DefineMetric("/lorem ipsum/dolor", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ShardTest, InvalidRedundantMetricNameFormat) {
  EXPECT_THAT(shard_.DefineMetricRedundant("Lorem ipsum dolor amet.", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(shard_.DefineMetricRedundant("lorem/ipsum", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(shard_.DefineMetricRedundant("/lorem ipsum/dolor", metric_config_),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(ShardTest, GetMissingValue) {
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ShardTest, SetValue) {
  shard_.SetValue(entity_labels1_, kIntMetricName, metric_fields1_, IntValue(42));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(ShardTest, SetAnotherValue) {
  shard_.SetValue(entity_labels2_, kDoubleMetricName, metric_fields2_, DoubleValue(3.14));
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kDoubleMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<double>(DoubleNear(3.14, 0.001))));
}

TEST_F(ShardTest, GetAnotherValue) {
  shard_.SetValue(entity_labels1_, kIntMetricName, metric_fields1_, IntValue(42));
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kDoubleMetricName, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ShardTest, AddToInt) {
  shard_.AddToInt(entity_labels1_, kIntMetricName, metric_fields1_, 12);
  shard_.AddToInt(entity_labels1_, kIntMetricName, metric_fields1_, 34);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(46)));
}

TEST_F(ShardTest, AddToAnotherInt) {
  shard_.AddToInt(entity_labels2_, kIntMetricName, metric_fields2_, 56);
  shard_.AddToInt(entity_labels2_, kIntMetricName, metric_fields2_, 78);
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kIntMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(134)));
}

TEST_F(ShardTest, AddToDistribution) {
  shard_.AddToDistribution(entity_labels1_, kDistributionMetricName, metric_fields1_, 12, 1);
  shard_.AddToDistribution(entity_labels1_, kDistributionMetricName, metric_fields1_, 34, 1);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kDistributionMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer_), DistributionSumAndCountAre(46, 2)))));
}

TEST_F(ShardTest, AddManyToDistribution) {
  shard_.AddToDistribution(entity_labels1_, kDistributionMetricName, metric_fields1_, 12, 3);
  shard_.AddToDistribution(entity_labels1_, kDistributionMetricName, metric_fields1_, 34, 2);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kDistributionMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer_), DistributionSumAndCountAre(104, 5)))));
}

TEST_F(ShardTest, AddToAnotherDistribution) {
  shard_.AddToDistribution(entity_labels2_, kDistributionMetricName, metric_fields2_, 56, 1);
  shard_.AddToDistribution(entity_labels2_, kDistributionMetricName, metric_fields2_, 78, 1);
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kDistributionMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer_), DistributionSumAndCountAre(134, 2)))));
}

TEST_F(ShardTest, DeleteValue) {
  shard_.SetValue(entity_labels1_, kIntMetricName, metric_fields1_, IntValue(42));
  shard_.DeleteValue(entity_labels1_, kIntMetricName, metric_fields1_);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ShardTest, DeleteIntValue) {
  shard_.AddToInt(entity_labels1_, kIntMetricName, metric_fields1_, 12);
  shard_.DeleteValue(entity_labels1_, kIntMetricName, metric_fields1_);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ShardTest, DeleteDistributionValue) {
  shard_.AddToDistribution(entity_labels1_, kDistributionMetricName, metric_fields1_, 3.14, 1);
  shard_.DeleteValue(entity_labels1_, kDistributionMetricName, metric_fields1_);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kDistributionMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ShardTest, DeleteMetric) {
  shard_.SetValue(entity_labels1_, kIntMetricName, metric_fields1_, IntValue(42));
  shard_.SetValue(entity_labels1_, kIntMetricName, metric_fields2_, IntValue(43));
  shard_.SetValue(entity_labels2_, kIntMetricName, metric_fields2_, IntValue(44));
  shard_.SetValue(entity_labels1_, kDoubleMetricName, metric_fields1_, DoubleValue(3.14));
  shard_.SetValue(entity_labels2_, kDoubleMetricName, metric_fields2_, DoubleValue(2.78));
  shard_.DeleteMetric(entity_labels1_, kIntMetricName);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kIntMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(44)));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kDoubleMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<double>(DoubleNear(3.14, 0.001))));
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kDoubleMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<double>(DoubleNear(2.78, 0.001))));
}

TEST_F(ShardTest, GetPinnedMetric) {
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels1_, kIntMetricName));
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels2_, kIntMetricName));
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels1_, kDoubleMetricName));
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels2_, kDoubleMetricName));
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels1_, kDistributionMetricName));
  EXPECT_OK(shard_.GetPinnedMetric(entity_labels2_, kDistributionMetricName));
  EXPECT_THAT(shard_.GetPinnedMetric(entity_labels1_, kUndefinedMetricName), Not(IsOk()));
  EXPECT_THAT(shard_.GetPinnedMetric(entity_labels2_, kUndefinedMetricName), Not(IsOk()));
}

TEST_F(ShardTest, SetValueInPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels1_, kIntMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.SetValue(metric_fields1_, IntValue(42));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(ShardTest, SetAnotherValueInPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels2_, kDoubleMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.SetValue(metric_fields2_, DoubleValue(3.14));
  EXPECT_THAT(shard_.GetValue(entity_labels2_, kDoubleMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<double>(DoubleNear(3.14, 0.001))));
}

TEST_F(ShardTest, AddToIntInPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels1_, kIntMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.AddToInt(metric_fields1_, IntValue(12));
  metric.AddToInt(metric_fields1_, IntValue(34));
  metric.AddToInt(metric_fields2_, IntValue(56));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(46)));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(56)));
}

TEST_F(ShardTest, AddToDistributionInPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels1_, kDistributionMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.AddToDistribution(metric_fields1_, 12, 1);
  metric.AddToDistribution(metric_fields1_, 34, 1);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kDistributionMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer_), DistributionSumAndCountAre(46, 2)))));
}

TEST_F(ShardTest, DeleteValueInPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels1_, kIntMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.SetValue(metric_fields1_, IntValue(42));
  metric.SetValue(metric_fields2_, IntValue(43));
  metric.DeleteValue(metric_fields1_);
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(43)));
}

TEST_F(ShardTest, ClearPinnedMetric) {
  auto status_or_metric = shard_.GetPinnedMetric(entity_labels1_, kIntMetricName);
  ASSERT_OK(status_or_metric);
  auto& metric = status_or_metric.value();
  metric.SetValue(metric_fields1_, IntValue(42));
  metric.SetValue(metric_fields2_, IntValue(43));
  metric.Clear();
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(shard_.GetValue(entity_labels1_, kIntMetricName, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

}  // namespace
