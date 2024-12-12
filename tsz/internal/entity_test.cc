#include "tsz/internal/entity.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/clock.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/mock_entity_manager.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::Return;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Entity;
using ::tsz::internal::EntityContext;
using ::tsz::internal::EntityManager;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::ScopedMetricProxy;
using ::tsz::internal::testing::MockEntityManager;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kMetricName1 = "/foo/bar";
std::string_view constexpr kMetricName2 = "/bar/baz";
std::string_view constexpr kUndefinedMetricName = "/baz/foo";

class EntityTest : public ::testing::Test {
 protected:
  explicit EntityTest() {
    ON_CALL(manager_, GetConfigForMetric(kMetricName1)).WillByDefault(Return(&metric_config_));
    ON_CALL(manager_, GetConfigForMetric(kMetricName2)).WillByDefault(Return(&metric_config_));
    ON_CALL(manager_, GetConfigForMetric(kUndefinedMetricName))
        .WillByDefault(Return(absl::NotFoundError(kUndefinedMetricName)));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  }

  MockEntityManager manager_;
  MetricConfig const metric_config_;

  FieldMap const entity_labels_{
      {"lorem", IntValue(123)},
      {"ipsum", StringValue("456")},
      {"dolor", BoolValue(true)},
  };
  FieldMap const metric_fields1_{
      {"foo", IntValue(42)},
      {"bar", BoolValue(false)},
  };
  FieldMap const metric_fields2_{
      {"bar", StringValue("baz")},
      {"foo", BoolValue(true)},
  };
};

TEST_F(EntityTest, Labels) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  EXPECT_EQ(entity->labels(), entity_labels_);
  EXPECT_EQ(entity->hash(), absl::HashOf(entity_labels_));
}

TEST_F(EntityTest, OtherLabels) {
  FieldMap const entity_labels{
      {"amet", StringValue("123")},
      {"consectetur", BoolValue(true)},
      {"adipisci", IntValue(789)},
  };
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels)).Times(0);
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels);
  EXPECT_EQ(entity->labels(), entity_labels);
  EXPECT_EQ(entity->hash(), absl::HashOf(entity_labels));
}

TEST_F(EntityTest, EntityContext) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  EXPECT_FALSE(entity->is_pinned());
  {
    EntityContext context{entity, absl::Now()};
    EXPECT_TRUE(entity->is_pinned());
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_FALSE(entity->is_pinned());
}

TEST_F(EntityTest, NestedEntityContext) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  ASSERT_FALSE(entity->is_pinned());
  {
    EntityContext context1{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    {
      EntityContext context2{entity, absl::Now()};
      EXPECT_TRUE(entity->is_pinned());
    }
    EXPECT_TRUE(entity->is_pinned());
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_FALSE(entity->is_pinned());
}

TEST_F(EntityTest, GetMissingValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, SetValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(EntityTest, SetValueAgain) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(43));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(43)));
}

TEST_F(EntityTest, SetAnotherValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName2, metric_fields2_, StringValue("42"));
  }
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              IsOkAndHolds(VariantWith<std::string>("42")));
}

TEST_F(EntityTest, SetTwoMetrics) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(12));
    entity->SetValue(context, kMetricName1, metric_fields2_, IntValue(34));
    entity->SetValue(context, kMetricName2, metric_fields2_, StringValue("56"));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(12)));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(34)));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              IsOkAndHolds(VariantWith<std::string>("56")));
}

TEST_F(EntityTest, AddToInt) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToInt(context, kMetricName1, metric_fields1_, 12);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(12)));
}

TEST_F(EntityTest, AddToIntAgain) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToInt(context, kMetricName1, metric_fields1_, 12);
    entity->AddToInt(context, kMetricName1, metric_fields1_, 34);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(46)));
}

TEST_F(EntityTest, AddToAnotherInt) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToInt(context, kMetricName1, metric_fields1_, 12);
    entity->AddToInt(context, kMetricName1, metric_fields2_, 34);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(12)));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(34)));
}

TEST_F(EntityTest, AddToManyInts) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToInt(context, kMetricName1, metric_fields1_, 12);
    entity->AddToInt(context, kMetricName1, metric_fields2_, 34);
    entity->AddToInt(context, kMetricName2, metric_fields2_, 56);
    entity->AddToInt(context, kMetricName2, metric_fields2_, 78);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(12)));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(34)));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(134)));
}

TEST_F(EntityTest, AddToDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  ON_CALL(manager_, GetConfigForMetric(kMetricName1)).WillByDefault(Return(&metric_config));
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToDistribution(context, kMetricName1, metric_fields1_, 12, 1);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(12, 1)))));
}

TEST_F(EntityTest, AddToDistributionAgain) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  ON_CALL(manager_, GetConfigForMetric(kMetricName1)).WillByDefault(Return(&metric_config));
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToDistribution(context, kMetricName1, metric_fields1_, 12, 2);
    entity->AddToDistribution(context, kMetricName1, metric_fields1_, 34, 1);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(58, 3)))));
}

TEST_F(EntityTest, AddToAnotherDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  ON_CALL(manager_, GetConfigForMetric(kMetricName1)).WillByDefault(Return(&metric_config));
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToDistribution(context, kMetricName1, metric_fields1_, 12, 1);
    entity->AddToDistribution(context, kMetricName1, metric_fields2_, 34, 1);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(12, 1)))));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(34, 1)))));
}

TEST_F(EntityTest, AddToManyDistributions) {
  auto const& bucketer1 = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config1{.bucketer = &bucketer1};
  ON_CALL(manager_, GetConfigForMetric(kMetricName1)).WillByDefault(Return(&metric_config1));
  auto const& bucketer2 = Bucketer::PowersOf(2.78);
  MetricConfig const metric_config2{.bucketer = &bucketer2};
  ON_CALL(manager_, GetConfigForMetric(kMetricName2)).WillByDefault(Return(&metric_config2));
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->AddToDistribution(context, kMetricName1, metric_fields1_, 12, 4);
    entity->AddToDistribution(context, kMetricName1, metric_fields2_, 34, 3);
    entity->AddToDistribution(context, kMetricName2, metric_fields2_, 56, 2);
    entity->AddToDistribution(context, kMetricName2, metric_fields2_, 78, 1);
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer1), DistributionSumAndCountAre(48, 4)))));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer1), DistributionSumAndCountAre(102, 3)))));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer2), DistributionSumAndCountAre(190, 3)))));
}

TEST_F(EntityTest, DeleteValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    EXPECT_TRUE(entity->DeleteValue(context, kMetricName1, metric_fields1_));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteValueAgain) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    ASSERT_TRUE(entity->DeleteValue(context, kMetricName1, metric_fields1_));
    EXPECT_FALSE(entity->DeleteValue(context, kMetricName1, metric_fields1_));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteMissingValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    EXPECT_FALSE(entity->DeleteValue(context, kMetricName1, metric_fields2_));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteValueFromEmptyEntity) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    EXPECT_FALSE(entity->DeleteValue(context, kMetricName1, metric_fields1_));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteValueInAnotherMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    entity->SetValue(context, kMetricName2, metric_fields2_, IntValue(43));
    EXPECT_TRUE(entity->DeleteValue(context, kMetricName2, metric_fields2_));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    entity->SetValue(context, kMetricName1, metric_fields2_, IntValue(43));
    EXPECT_TRUE(entity->DeleteMetric(context, kMetricName1));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteMetricAgain) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    entity->SetValue(context, kMetricName1, metric_fields2_, IntValue(43));
    ASSERT_TRUE(entity->DeleteMetric(context, kMetricName1));
    EXPECT_FALSE(entity->DeleteMetric(context, kMetricName1));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteAnotherMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    entity->SetValue(context, kMetricName1, metric_fields1_, IntValue(42));
    entity->SetValue(context, kMetricName1, metric_fields2_, IntValue(43));
    entity->SetValue(context, kMetricName2, metric_fields1_, IntValue(44));
    entity->SetValue(context, kMetricName2, metric_fields2_, IntValue(45));
    EXPECT_TRUE(entity->DeleteMetric(context, kMetricName2));
  }
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(43)));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(entity->GetValue(kMetricName2, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, DeleteMetricFromEmptyEntity) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityContext context{entity, absl::Now()};
    EXPECT_FALSE(entity->DeleteMetric(context, kMetricName1));
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityTest, GetPinnedMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  ScopedMetricProxy proxy;
  {
    EntityContext context{entity, absl::Now()};
    auto status_or_proxy = entity->GetPinnedMetric(context, kMetricName1);
    EXPECT_OK(status_or_proxy);
    proxy = std::move(status_or_proxy).value();
  }
  ASSERT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  proxy.SetValue(metric_fields1_, IntValue(42));
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(EntityTest, ClearPinnedMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  ScopedMetricProxy proxy;
  {
    EntityContext context{entity, absl::Now()};
    auto status_or_proxy = entity->GetPinnedMetric(context, kMetricName1);
    EXPECT_OK(status_or_proxy);
    proxy = std::move(status_or_proxy).value();
  }
  proxy.SetValue(metric_fields1_, IntValue(42));
  proxy.Clear();
  EXPECT_THAT(entity->GetValue(kMetricName1, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
}

TEST_F(EntityTest, PinUndefinedMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  EntityContext context{entity, absl::Now()};
  EXPECT_THAT(entity->GetPinnedMetric(context, kUndefinedMetricName),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
}

}  // namespace
