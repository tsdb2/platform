#include "tsz/internal/scoped_metric_proxy.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/mock_metric_manager.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::DoubleValue;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Metric;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::ScopedMetricProxy;
using ::tsz::internal::testing::MockMetricManager;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kMetricName = "/foo/bar";
std::string_view constexpr kMetricName1 = "/bar/baz";
std::string_view constexpr kMetricName2 = "/baz/foo";

class ScopedMetricProxyTest : public ::testing::Test {
 protected:
  explicit ScopedMetricProxyTest() {
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(0);
  }

  std::shared_ptr<MockMetricManager> manager_ = std::make_shared<MockMetricManager>();
  MetricConfig const metric_config_{};
};

TEST_F(ScopedMetricProxyTest, EmptyProxy) {
  ScopedMetricProxy proxy;
  EXPECT_TRUE(proxy.empty());
  EXPECT_FALSE(proxy.operator bool());
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  proxy.SetValue(metric_fields, IntValue(42));
  proxy.AddToInt(metric_fields, IntValue(123));
  proxy.AddToDistribution(metric_fields, 3.14, 1);
  EXPECT_FALSE(proxy.DeleteValue(metric_fields));
  EXPECT_FALSE(proxy.Clear());
}

TEST_F(ScopedMetricProxyTest, NonEmptyProxy) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    EXPECT_FALSE(proxy.empty());
    EXPECT_TRUE(proxy.operator bool());
    EXPECT_TRUE(metric->is_pinned());
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
  }
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ScopedMetricProxyTest, DoublePinning) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ASSERT_FALSE(metric->is_pinned());
  {
    ScopedMetricProxy proxy1{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    {
      ScopedMetricProxy proxy2{manager_, metric, absl::Now()};
      EXPECT_TRUE(metric->is_pinned());
    }
    EXPECT_TRUE(metric->is_pinned());
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
  }
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ScopedMetricProxyTest, MoveConstruct) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ASSERT_FALSE(metric->is_pinned());
  {
    ScopedMetricProxy proxy1{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    {
      ScopedMetricProxy proxy2{std::move(proxy1)};
      EXPECT_FALSE(proxy2.empty());
      EXPECT_TRUE(metric->is_pinned());
      EXPECT_TRUE(proxy1.empty());
      EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
    }
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(0);
    EXPECT_FALSE(metric->is_pinned());
  }
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ScopedMetricProxyTest, MoveAssign) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ASSERT_FALSE(metric->is_pinned());
  {
    ScopedMetricProxy proxy1{manager_, metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    {
      ScopedMetricProxy proxy2;
      ASSERT_TRUE(proxy2.empty());
      proxy2 = std::move(proxy1);
      EXPECT_FALSE(proxy2.empty());
      EXPECT_TRUE(metric->is_pinned());
      EXPECT_TRUE(proxy1.empty());
      EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
    }
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(0);
    EXPECT_FALSE(metric->is_pinned());
  }
  EXPECT_FALSE(metric->is_pinned());
}

TEST_F(ScopedMetricProxyTest, Swap) {
  auto const metric1 = std::make_shared<Metric>(manager_.get(), kMetricName1, metric_config_);
  ASSERT_FALSE(metric1->is_pinned());
  auto const metric2 = std::make_shared<Metric>(manager_.get(), kMetricName2, metric_config_);
  ASSERT_FALSE(metric2->is_pinned());
  {
    ScopedMetricProxy proxy1{manager_, metric1, absl::Now()};
    ASSERT_TRUE(metric1->is_pinned());
    {
      ScopedMetricProxy proxy2{manager_, metric2, absl::Now()};
      ASSERT_TRUE(metric2->is_pinned());
      proxy1.swap(proxy2);
      EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName1)).Times(1);
    }
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName1)).Times(0);
    EXPECT_FALSE(metric1->is_pinned());
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName2)).Times(1);
  }
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName2)).Times(0);
  EXPECT_FALSE(metric2->is_pinned());
}

TEST_F(ScopedMetricProxyTest, AdlSwap) {
  auto const metric1 = std::make_shared<Metric>(manager_.get(), kMetricName1, metric_config_);
  ASSERT_FALSE(metric1->is_pinned());
  auto const metric2 = std::make_shared<Metric>(manager_.get(), kMetricName2, metric_config_);
  ASSERT_FALSE(metric2->is_pinned());
  {
    ScopedMetricProxy proxy1{manager_, metric1, absl::Now()};
    ASSERT_TRUE(metric1->is_pinned());
    {
      ScopedMetricProxy proxy2{manager_, metric2, absl::Now()};
      ASSERT_TRUE(metric2->is_pinned());
      swap(proxy1, proxy2);
      EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName1)).Times(1);
    }
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName1)).Times(0);
    EXPECT_FALSE(metric1->is_pinned());
    EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName2)).Times(1);
  }
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName2)).Times(0);
  EXPECT_FALSE(metric2->is_pinned());
}

TEST_F(ScopedMetricProxyTest, SetValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.SetValue(metric_fields, IntValue(42));
  EXPECT_THAT(proxy.GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(ScopedMetricProxyTest, GetValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    proxy.SetValue(metric_fields, IntValue(42));
  }
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    EXPECT_THAT(proxy.GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(42)));
  }
}

TEST_F(ScopedMetricProxyTest, GetMissingValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.SetValue(
      FieldMap{
          {"foo", StringValue("bar")},
          {"baz", BoolValue(true)},
      },
      IntValue(42));
  EXPECT_THAT(proxy.GetValue(FieldMap{
                  {"bar", StringValue("baz")},
                  {"foo", BoolValue(false)},
              }),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ScopedMetricProxyTest, GetMissingValueInAnotherProxy) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    proxy.SetValue(
        FieldMap{
            {"foo", StringValue("bar")},
            {"baz", BoolValue(true)},
        },
        IntValue(42));
  }
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_THAT(proxy.GetValue(FieldMap{
                  {"bar", StringValue("baz")},
                  {"foo", BoolValue(false)},
              }),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ScopedMetricProxyTest, DeleteValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.SetValue(metric_fields, IntValue(42));
  EXPECT_TRUE(proxy.DeleteValue(metric_fields));
  EXPECT_THAT(proxy.GetValue(metric_fields), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

TEST_F(ScopedMetricProxyTest, DeleteMissingValue) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields1{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  FieldMap const metric_fields2{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(false)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.SetValue(metric_fields1, IntValue(42));
  EXPECT_FALSE(proxy.DeleteValue(metric_fields2));
  EXPECT_THAT(proxy.GetValue(metric_fields1), IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(ScopedMetricProxyTest, DeleteValueInAnotherProxy) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    proxy.SetValue(metric_fields, IntValue(42));
  }
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_TRUE(proxy.DeleteValue(metric_fields));
  EXPECT_THAT(proxy.GetValue(metric_fields), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

TEST_F(ScopedMetricProxyTest, AddToInt) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.AddToInt(metric_fields, IntValue(123));
  proxy.AddToInt(metric_fields, IntValue(456));
  EXPECT_THAT(proxy.GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(579)));
}

TEST_F(ScopedMetricProxyTest, AddToDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config);
  FieldMap const metric_fields{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.AddToDistribution(metric_fields, 12, 2);
  proxy.AddToDistribution(metric_fields, 34, 1);
  EXPECT_THAT(proxy.GetValue(metric_fields),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(58, 3)))));
}

TEST_F(ScopedMetricProxyTest, Clear) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields1{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  FieldMap const metric_fields2{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(false)},
  };
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  proxy.SetValue(metric_fields1, IntValue(42));
  proxy.SetValue(metric_fields2, DoubleValue(3.14));
  EXPECT_TRUE(proxy.Clear());
  EXPECT_THAT(proxy.GetValue(metric_fields1), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(proxy.GetValue(metric_fields2), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

TEST_F(ScopedMetricProxyTest, ClearEmptyMetric) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_FALSE(proxy.Clear());
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

TEST_F(ScopedMetricProxyTest, ClearInAnotherProxy) {
  auto const metric = std::make_shared<Metric>(manager_.get(), kMetricName, metric_config_);
  FieldMap const metric_fields1{
      {"foo", StringValue("bar")},
      {"baz", BoolValue(true)},
  };
  FieldMap const metric_fields2{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(false)},
  };
  {
    ScopedMetricProxy proxy{manager_, metric, absl::Now()};
    proxy.SetValue(metric_fields1, IntValue(42));
    proxy.SetValue(metric_fields2, DoubleValue(3.14));
  }
  ScopedMetricProxy proxy{manager_, metric, absl::Now()};
  EXPECT_TRUE(proxy.Clear());
  EXPECT_THAT(proxy.GetValue(metric_fields1), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(proxy.GetValue(metric_fields2), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(*manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

}  // namespace
