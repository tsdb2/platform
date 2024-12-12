#include "tsz/internal/throw_away_metric_proxy.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/clock.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/metric.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/mock_metric_manager.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Metric;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::ThrowAwayMetricProxy;
using ::tsz::internal::testing::MockMetricManager;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kMetricName = "/foo/bar";

class ThrowAwayMetricProxyTest : public ::testing::Test {
 protected:
  explicit ThrowAwayMetricProxyTest() {
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(0);
  }

  std::shared_ptr<MockMetricManager> manager_ = std::make_shared<MockMetricManager>();
  MetricConfig const metric_config_{};
};

TEST_F(ThrowAwayMetricProxyTest, SetValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ASSERT_FALSE(metric->is_pinned());
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    EXPECT_FALSE(proxy.empty());
    EXPECT_TRUE(proxy.operator bool());
    EXPECT_TRUE(metric->is_pinned());
    proxy.SetValue(metric_fields, IntValue(42));
  }
  EXPECT_FALSE(metric->is_pinned());
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(ThrowAwayMetricProxyTest, GetValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ASSERT_FALSE(metric->is_pinned());
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    proxy.SetValue(metric_fields, IntValue(42));
    ASSERT_FALSE(metric->is_pinned());
  }
  ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_TRUE(metric->is_pinned());
  EXPECT_THAT(proxy.GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(42)));
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ThrowAwayMetricProxyTest, DoublePinning) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ASSERT_FALSE(metric->is_pinned());
  ThrowAwayMetricProxy proxy1{manager_, metric, absl::Now()};
  ASSERT_TRUE(metric->is_pinned());
  {
    ThrowAwayMetricProxy proxy2{manager_, metric, absl::Now()};
    EXPECT_TRUE(metric->is_pinned());
    proxy2.SetValue(metric_fields, IntValue(42));
    EXPECT_TRUE(metric->is_pinned());
  }
  EXPECT_TRUE(metric->is_pinned());
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
  proxy1.DeleteValue(metric_fields);
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ThrowAwayMetricProxyTest, GetMissingValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    proxy.SetValue(
        FieldMap{
            {"foo", StringValue("bar")},
            {"baz", BoolValue(true)},
        },
        IntValue(42));
  }
  ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_THAT(proxy.GetValue(FieldMap{
                  {"bar", StringValue("baz")},
                  {"foo", BoolValue(false)},
              }),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ThrowAwayMetricProxyTest, AddToInt) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    proxy.AddToInt(metric_fields, 42);
    EXPECT_FALSE(metric->is_pinned());
  }
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    proxy.AddToInt(metric_fields, 43);
    EXPECT_FALSE(metric->is_pinned());
  }
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(85)));
}

TEST_F(ThrowAwayMetricProxyTest, AddToDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    proxy.AddToDistribution(metric_fields, 12, 2);
    EXPECT_FALSE(metric->is_pinned());
  }
  {
    ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    proxy.AddToDistribution(metric_fields, 34, 1);
    EXPECT_FALSE(metric->is_pinned());
  }
  EXPECT_THAT(metric->GetValue(metric_fields),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(58, 3)))));
}

TEST_F(ThrowAwayMetricProxyTest, Clear) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields1{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  FieldMap const metric_fields2{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(false)},
  };
  ASSERT_FALSE(metric->is_pinned());
  ThrowAwayMetricProxy proxy1{manager_, metric, absl::Now()};
  ASSERT_TRUE(metric->is_pinned());
  {
    ThrowAwayMetricProxy proxy2{manager_, metric, absl::Now()};
    proxy2.SetValue(metric_fields1, IntValue(42));
    ASSERT_TRUE(metric->is_pinned());
  }
  {
    ThrowAwayMetricProxy proxy3{manager_, metric, absl::Now()};
    proxy3.SetValue(metric_fields2, IntValue(42));
    ASSERT_TRUE(metric->is_pinned());
  }
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
  EXPECT_TRUE(proxy1.Clear());
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ThrowAwayMetricProxyTest, ClearEmptyMetric) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ASSERT_FALSE(metric->is_pinned());
  ThrowAwayMetricProxy proxy{manager_, metric, absl::Now()};
  ASSERT_TRUE(metric->is_pinned());
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
  EXPECT_FALSE(proxy.Clear());
  EXPECT_FALSE(metric->is_pinned());
}

}  // namespace
