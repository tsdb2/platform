#include "tsz/internal/entity_proxy.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/status/status_matchers.h"
#include "absl/time/clock.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/distribution_testing.h"
#include "tsz/internal/entity.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/mock_entity_manager.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::Return;
using ::testing::VariantWith;
using ::tsz::BoolValue;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Entity;
using ::tsz::internal::EntityProxy;
using ::tsz::internal::MetricConfig;
using ::tsz::internal::ScopedMetricProxy;
using ::tsz::internal::testing::MockEntityManager;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kMetricName = "/foo/bar";

class EntityProxyTest : public ::testing::Test {
 protected:
  explicit EntityProxyTest() {
    ON_CALL(manager_, GetConfigForMetric(kMetricName)).WillByDefault(Return(&metric_config_));
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

TEST_F(EntityProxyTest, EmptyProxy) {
  EntityProxy proxy;
  EXPECT_TRUE(proxy.empty());
  EXPECT_FALSE(proxy.operator bool());
  proxy.SetValue(kMetricName, metric_fields1_, IntValue(42));
  EXPECT_FALSE(proxy.DeleteValue(kMetricName, metric_fields1_));
  proxy.AddToInt(kMetricName, metric_fields1_, 123);
  proxy.AddToDistribution(kMetricName, metric_fields1_, 3.14, 1);
  EXPECT_FALSE(proxy.DeleteMetric(kMetricName));
}

TEST_F(EntityProxyTest, NonEmptyProxy) {
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  EntityProxy proxy{entity, absl::Now()};
  EXPECT_FALSE(proxy.empty());
  EXPECT_TRUE(proxy.operator bool());
}

TEST_F(EntityProxyTest, MoveConstructProxy) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityProxy proxy1{entity, absl::Now()};
    {
      EntityProxy proxy2{std::move(proxy1)};
      EXPECT_TRUE(entity->is_pinned());
      EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
    }
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
    EXPECT_FALSE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
}

TEST_F(EntityProxyTest, MoveProxy) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    EntityProxy proxy1{entity, absl::Now()};
    {
      EntityProxy proxy2;
      proxy2 = std::move(proxy1);
      EXPECT_TRUE(entity->is_pinned());
      EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
    }
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
    EXPECT_FALSE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
}

TEST_F(EntityProxyTest, GetMissingValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    EXPECT_THAT(proxy.GetValue(kMetricName, metric_fields1_),
                StatusIs(absl::StatusCode::kNotFound));
    EXPECT_TRUE(entity->is_pinned());
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_FALSE(entity->is_pinned());
}

TEST_F(EntityProxyTest, SetValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.SetValue(kMetricName, metric_fields1_, IntValue(42));
    EXPECT_TRUE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(EntityProxyTest, GetValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.SetValue(kMetricName, metric_fields1_, IntValue(42));
    EXPECT_THAT(proxy.GetValue(kMetricName, metric_fields1_),
                IsOkAndHolds(VariantWith<int64_t>(42)));
    EXPECT_TRUE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

TEST_F(EntityProxyTest, AddToInt) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.AddToInt(kMetricName, metric_fields1_, 12);
    proxy.AddToInt(kMetricName, metric_fields1_, 34);
    EXPECT_TRUE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(46)));
}

TEST_F(EntityProxyTest, AddToDistribution) {
  auto const& bucketer = Bucketer::PowersOf(3.14);
  MetricConfig const metric_config{.bucketer = &bucketer};
  ON_CALL(manager_, GetConfigForMetric(kMetricName)).WillByDefault(Return(&metric_config));
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.AddToDistribution(kMetricName, metric_fields1_, 12, 2);
    proxy.AddToDistribution(kMetricName, metric_fields1_, 34, 1);
    EXPECT_TRUE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<Distribution>(
                  AllOf(DistributionBucketerIs(bucketer), DistributionSumAndCountAre(58, 3)))));
}

TEST_F(EntityProxyTest, DeleteValue) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.SetValue(kMetricName, metric_fields1_, IntValue(42));
    proxy.SetValue(kMetricName, metric_fields2_, IntValue(43));
    EXPECT_TRUE(proxy.DeleteValue(kMetricName, metric_fields1_));
    EXPECT_TRUE(entity->is_pinned());
  }
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields2_),
              IsOkAndHolds(VariantWith<int64_t>(43)));
}

TEST_F(EntityProxyTest, DeleteMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  {
    ASSERT_FALSE(entity->is_pinned());
    EntityProxy proxy{entity, absl::Now()};
    ASSERT_TRUE(entity->is_pinned());
    proxy.SetValue(kMetricName, metric_fields1_, IntValue(42));
    proxy.SetValue(kMetricName, metric_fields2_, IntValue(43));
    EXPECT_TRUE(proxy.DeleteMetric(kMetricName));
    EXPECT_TRUE(entity->is_pinned());
    EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(1);
  }
  EXPECT_CALL(manager_, DeleteEntityInternal(entity_labels_)).Times(0);
  EXPECT_FALSE(entity->is_pinned());
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields2_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EntityProxyTest, GetPinnedMetric) {
  auto const entity = std::make_shared<Entity>(&manager_, entity_labels_);
  ScopedMetricProxy metric_proxy;
  {
    EntityProxy entity_proxy{entity, absl::Now()};
    auto status_or_proxy = entity_proxy.GetPinnedMetric(kMetricName);
    EXPECT_OK(status_or_proxy);
    metric_proxy = std::move(status_or_proxy).value();
  }
  ASSERT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              StatusIs(absl::StatusCode::kNotFound));
  metric_proxy.SetValue(metric_fields1_, IntValue(42));
  EXPECT_THAT(entity->GetValue(kMetricName, metric_fields1_),
              IsOkAndHolds(VariantWith<int64_t>(42)));
}

}  // namespace
