#include "tsz/internal/exporter.h"

#include <string_view>

#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"
#include "tsz/distribution_testing.h"

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::Not;
using ::testing::VariantWith;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::FieldMap;
using ::tsz::StringValue;
using ::tsz::internal::Exporter;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

using ::tsz::internal::exporter;

std::string_view constexpr kMetricName1 = "/lorem/ipsum1";
std::string_view constexpr kMetricName2 = "/lorem/ipsum2";
std::string_view constexpr kMetricName3 = "/lorem/ipsum3";
std::string_view constexpr kMetricName4 = "/lorem/ipsum4";

class ExporterTest : public ::testing::Test {};

TEST_F(ExporterTest, DefineMetric) {
  EXPECT_OK(exporter->DefineMetric(kMetricName1, /*options=*/{}));
  EXPECT_THAT(exporter->DefineMetric(kMetricName1, /*options=*/{}), Not(IsOk()));
}

TEST_F(ExporterTest, DefineMetricRedundant) {
  auto const status_or_shard1 = exporter->DefineMetricRedundant(kMetricName2, /*options=*/{});
  EXPECT_OK(status_or_shard1);
  auto const status_or_shard2 = exporter->DefineMetricRedundant(kMetricName2, /*options=*/{});
  EXPECT_OK(status_or_shard2);
  auto *const shard1 = status_or_shard1.value();
  auto *const shard2 = status_or_shard2.value();
  EXPECT_EQ(shard1, shard2);
}

TEST_F(ExporterTest, DefineMetricAndGetShard) {
  auto const status_or_shard1 = exporter->DefineMetric(kMetricName3, /*options=*/{});
  ASSERT_OK(status_or_shard1);
  auto const status_or_shard2 = exporter->GetShardForMetric(kMetricName3);
  EXPECT_OK(status_or_shard2);
  auto *const shard1 = status_or_shard1.value();
  auto *const shard2 = status_or_shard2.value();
  EXPECT_EQ(shard1, shard2);
  FieldMap const entity_labels{{"lorem", StringValue("ipsum")}};
  FieldMap const metric_fields{{"foo", StringValue("bar")}};
  shard2->AddToDistribution(entity_labels, kMetricName3, metric_fields, 42, 1);
  EXPECT_THAT(
      shard2->GetValue(entity_labels, kMetricName3, metric_fields),
      IsOkAndHolds(VariantWith<Distribution>(
          AllOf(DistributionBucketerIs(Bucketer::Default()), DistributionSumAndCountAre(42, 1)))));
}

TEST_F(ExporterTest, DefineMetricRedundantAndGetShard) {
  auto const status_or_shard1 = exporter->DefineMetricRedundant(kMetricName4, /*options=*/{});
  ASSERT_OK(status_or_shard1);
  auto const status_or_shard2 = exporter->GetShardForMetric(kMetricName4);
  EXPECT_OK(status_or_shard2);
  auto *const shard1 = status_or_shard1.value();
  auto *const shard2 = status_or_shard2.value();
  EXPECT_EQ(shard1, shard2);
  FieldMap const entity_labels{{"lorem", StringValue("ipsum")}};
  FieldMap const metric_fields{{"foo", StringValue("bar")}};
  shard2->AddToDistribution(entity_labels, kMetricName4, metric_fields, 42, 1);
  EXPECT_THAT(
      shard2->GetValue(entity_labels, kMetricName4, metric_fields),
      IsOkAndHolds(VariantWith<Distribution>(
          AllOf(DistributionBucketerIs(Bucketer::Default()), DistributionSumAndCountAre(42, 1)))));
}

}  // namespace
