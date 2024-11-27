#include "tsz/internal/metric.h"

#include <memory>
#include <string>
#include <string_view>

#include "absl/hash/hash.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/mock_metric_manager.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/internal/throw_away_metric_proxy.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Metric;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::MetricManager;
using ::tsz::internal::ScopedMetricContext;
using ::tsz::internal::ThrowAwayMetricContext;
using ::tsz::internal::testing::MockMetricManager;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;
using ::tsz::testing::EmptyDistribution;

std::string_view constexpr kMetricName = "/foo/bar";

template <typename MetricContextType>
class MetricTest : public ::testing::Test {
 protected:
  using MetricContext = MetricContextType;

  explicit MetricTest() { EXPECT_CALL(manager_, DeleteMetricInternal(kMetricName)).Times(0); }

  MockMetricManager manager_;
  MetricConfig const metric_config_{};
};

TYPED_TEST_SUITE_P(MetricTest);

TYPED_TEST_P(MetricTest, Name) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  EXPECT_EQ(metric->name(), kMetricName);
  EXPECT_EQ(metric->hash(), absl::HashOf(kMetricName));
}

TYPED_TEST_P(MetricTest, AnotherName) {
  auto const metric = std::make_shared<Metric>(&this->manager_, "/bar/baz", this->metric_config_);
  EXPECT_EQ(metric->name(), "/bar/baz");
  EXPECT_EQ(metric->hash(), absl::HashOf(std::string_view("/bar/baz")));
}

TYPED_TEST_P(MetricTest, GetMissingValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  EXPECT_THAT(metric->GetValue({
                  {"lorem", BoolValue(true)},
                  {"ipsum", IntValue(42)},
                  {"dolor", StringValue("hello")},
              }),
              StatusIs(absl::StatusCode::kNotFound));
}

TYPED_TEST_P(MetricTest, SetValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    EXPECT_TRUE(metric->is_pinned());
    metric->SetValue(&context, metric_fields, IntValue(123));
  }
  EXPECT_FALSE(metric->is_pinned());
}

TYPED_TEST_P(MetricTest, GetValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields, IntValue(123));
  }
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(123)));
}

TYPED_TEST_P(MetricTest, SetAnotherValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields, StringValue("456"));
  }
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<std::string>("456")));
}

TYPED_TEST_P(MetricTest, DeleteValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields, IntValue(123));
  }
  ASSERT_FALSE(metric->is_pinned());
  {
    TypeParam context{metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    EXPECT_CALL(this->manager_, DeleteMetricInternal(kMetricName)).Times(1);
    EXPECT_TRUE(metric->DeleteValue(&context, metric_fields));
  }
  EXPECT_FALSE(metric->is_pinned());
}

TYPED_TEST_P(MetricTest, DeleteMissingValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields1, IntValue(123));
  }
  ASSERT_FALSE(metric->is_pinned());
  {
    TypeParam context{metric, absl::Now()};
    ASSERT_TRUE(metric->is_pinned());
    EXPECT_FALSE(metric->DeleteValue(&context, metric_fields2));
  }
  EXPECT_FALSE(metric->is_pinned());
}

TYPED_TEST_P(MetricTest, GetDeletedValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields, IntValue(123));
  }
  {
    TypeParam context{metric, absl::Now()};
    EXPECT_CALL(this->manager_, DeleteMetricInternal(kMetricName)).Times(1);
    EXPECT_TRUE(metric->DeleteValue(&context, metric_fields));
  }
  EXPECT_THAT(metric->GetValue(metric_fields), StatusIs(absl::StatusCode::kNotFound));
}

TYPED_TEST_P(MetricTest, RemainingValueKeepsMetricAlive) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields1, IntValue(123));
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields2, IntValue(456));
  }
  ASSERT_FALSE(metric->is_pinned());
  {
    TypeParam context{metric, absl::Now()};
    ASSERT_TRUE(metric->DeleteValue(&context, metric_fields1));
  }
  EXPECT_FALSE(metric->is_pinned());
}

TYPED_TEST_P(MetricTest, GetRemainingValue) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields1, IntValue(123));
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields2, IntValue(456));
  }
  {
    TypeParam context{metric, absl::Now()};
    ASSERT_TRUE(metric->DeleteValue(&context, metric_fields1));
  }
  EXPECT_THAT(metric->GetValue(metric_fields1), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(metric->GetValue(metric_fields2), IsOkAndHolds(VariantWith<int64_t>(456)));
}

TYPED_TEST_P(MetricTest, DoubleDeletion) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields1, IntValue(123));
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields2, IntValue(456));
  }
  {
    TypeParam context{metric, absl::Now()};
    ASSERT_TRUE(metric->DeleteValue(&context, metric_fields1));
  }
  {
    TypeParam context{metric, absl::Now()};
    EXPECT_FALSE(metric->DeleteValue(&context, metric_fields1));
  }
  EXPECT_FALSE(metric->is_pinned());
}

TYPED_TEST_P(MetricTest, PinningKeepsMetricAlive) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  TypeParam context1{metric, absl::Now()};
  typename TypeParam::Closure context_closure{&context1};
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context2{metric, absl::Now()};
    metric->SetValue(&context2, metric_fields, IntValue(123));
  }
  {
    TypeParam context2{metric, absl::Now()};
    ASSERT_TRUE(metric->DeleteValue(&context2, metric_fields));
  }
  EXPECT_TRUE(metric->is_pinned());
  EXPECT_CALL(this->manager_, DeleteMetricInternal(kMetricName)).Times(1);
}

TYPED_TEST_P(MetricTest, Clear) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields1, IntValue(123));
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->SetValue(&context, metric_fields2, IntValue(456));
  }
  {
    TypeParam context{metric, absl::Now()};
    EXPECT_CALL(this->manager_, DeleteMetricInternal(kMetricName)).Times(1);
    EXPECT_TRUE(metric->Clear(&context));
  }
  EXPECT_THAT(metric->GetValue(metric_fields1), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(metric->GetValue(metric_fields2), StatusIs(absl::StatusCode::kNotFound));
}

TYPED_TEST_P(MetricTest, ClearEmpty) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  TypeParam context{metric, absl::Now()};
  EXPECT_CALL(this->manager_, DeleteMetricInternal(kMetricName)).Times(1);
  EXPECT_FALSE(metric->Clear(&context));
}

TYPED_TEST_P(MetricTest, AddToInt) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields, 12);
  }
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(12)));
}

TYPED_TEST_P(MetricTest, AddToExistingInt) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields, 12);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields, 34);
  }
  EXPECT_THAT(metric->GetValue(metric_fields), IsOkAndHolds(VariantWith<int64_t>(46)));
}

TYPED_TEST_P(MetricTest, AddToAnotherInt) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields1, 12);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields2, 34);
  }
  EXPECT_THAT(metric->GetValue(metric_fields1), IsOkAndHolds(VariantWith<int64_t>(12)));
  EXPECT_THAT(metric->GetValue(metric_fields2), IsOkAndHolds(VariantWith<int64_t>(34)));
}

TYPED_TEST_P(MetricTest, AddToDistribution) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields, 42, 1);
  }
  EXPECT_THAT(
      metric->GetValue(metric_fields),
      IsOkAndHolds(VariantWith<Distribution>(
          AllOf(DistributionBucketerIs(Bucketer::Default()), DistributionSumAndCountAre(42, 1)))));
}

TYPED_TEST_P(MetricTest, AddToDistributionWithCustomBucketer) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields, 42, 1);
  }
  EXPECT_THAT(metric->GetValue(metric_fields),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(42, 1)))));
}

TYPED_TEST_P(MetricTest, AddToDistributionTwice) {
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, this->metric_config_);
  FieldMap const metric_fields{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields, 12, 2);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields, 34, 1);
  }
  EXPECT_THAT(
      metric->GetValue(metric_fields),
      IsOkAndHolds(VariantWith<Distribution>(
          AllOf(DistributionBucketerIs(Bucketer::Default()), DistributionSumAndCountAre(58, 3)))));
}

TYPED_TEST_P(MetricTest, AddToAnotherDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields1, 12, 3);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields2, 34, 2);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields1, 56, 1);
  }
  EXPECT_THAT(metric->GetValue(metric_fields1),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(92, 4)))));
  EXPECT_THAT(metric->GetValue(metric_fields2),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(68, 2)))));
}

TYPED_TEST_P(MetricTest, ResetIntIfCumulative) {
  MetricConfig const metric_config{.cumulative = true};
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields1, 12);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields2, 34);
  }
  metric->ResetIfCumulative(absl::Now());
  EXPECT_THAT(metric->GetValue(metric_fields1), IsOkAndHolds(VariantWith<int64_t>(0)));
  EXPECT_THAT(metric->GetValue(metric_fields2), IsOkAndHolds(VariantWith<int64_t>(0)));
}

TYPED_TEST_P(MetricTest, DontResetIntIfNotCumulative) {
  MetricConfig const metric_config{.cumulative = false};
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields1, 12);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToInt(&context, metric_fields2, 34);
  }
  metric->ResetIfCumulative(absl::Now());
  EXPECT_THAT(metric->GetValue(metric_fields1), IsOkAndHolds(VariantWith<int64_t>(12)));
  EXPECT_THAT(metric->GetValue(metric_fields2), IsOkAndHolds(VariantWith<int64_t>(34)));
}

TYPED_TEST_P(MetricTest, ResetDistributionIfCumulative) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{
      .cumulative = true,
      .bucketer = &bucketer,
  };
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields1, 12, 1);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields2, 34, 1);
  }
  metric->ResetIfCumulative(absl::Now());
  EXPECT_THAT(metric->GetValue(metric_fields1),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), EmptyDistribution()))));
  EXPECT_THAT(metric->GetValue(metric_fields2),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), EmptyDistribution()))));
}

TYPED_TEST_P(MetricTest, DontResetDistributionIfNotCumulative) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{
      .cumulative = false,
      .bucketer = &bucketer,
  };
  auto const metric = std::make_shared<Metric>(&this->manager_, kMetricName, metric_config);
  FieldMap const metric_fields1{
      {"lorem", BoolValue(true)},
      {"ipsum", IntValue(42)},
      {"dolor", StringValue("hello")},
  };
  FieldMap const metric_fields2{
      {"amet", BoolValue(false)},
      {"consectetur", IntValue(43)},
      {"adipisci", StringValue("olleh")},
  };
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields1, 12, 1);
  }
  {
    TypeParam context{metric, absl::Now()};
    metric->AddToDistribution(&context, metric_fields2, 34, 1);
  }
  metric->ResetIfCumulative(absl::Now());
  EXPECT_THAT(metric->GetValue(metric_fields1),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(12, 1)))));
  EXPECT_THAT(metric->GetValue(metric_fields2),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(34, 1)))));
}

REGISTER_TYPED_TEST_SUITE_P(MetricTest, Name, AnotherName, GetMissingValue, SetValue, GetValue,
                            SetAnotherValue, DeleteValue, DeleteMissingValue, GetDeletedValue,
                            RemainingValueKeepsMetricAlive, GetRemainingValue, DoubleDeletion,
                            PinningKeepsMetricAlive, Clear, ClearEmpty, AddToInt, AddToExistingInt,
                            AddToAnotherInt, AddToDistribution, AddToDistributionWithCustomBucketer,
                            AddToDistributionTwice, AddToAnotherDistribution, ResetIntIfCumulative,
                            DontResetIntIfNotCumulative, ResetDistributionIfCumulative,
                            DontResetDistributionIfNotCumulative);

using MetricContextTypes = ::testing::Types<ScopedMetricContext, ThrowAwayMetricContext>;
INSTANTIATE_TYPED_TEST_SUITE_P(MetricTest, MetricTest, MetricContextTypes);

}  // namespace
