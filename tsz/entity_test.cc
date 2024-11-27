#include "tsz/entity.h"

#include <cstdint>
#include <string>
#include <thread>

#include "absl/hash/hash.h"
#include "absl/synchronization/notification.h"
#include "common/fingerprint.h"
#include "common/reffed_ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::tsdb2::common::FingerprintOf;
using ::tsz::Entity;
using ::tsz::Field;
using ::tsz::GetDefaultEntity;

char constexpr kLabelName1[] = "lorem";
char constexpr kLabelName2[] = "ipsum";
char constexpr kLabelName3[] = "dolor";

using TestEntity =
    Entity<Field<int, kLabelName1>, Field<std::string, kLabelName2>, Field<bool, kLabelName3>>;

TEST(EntityTest, Descriptor) {
  TestEntity const entity{42, "foo", true};
  EXPECT_THAT(entity.descriptor().names(), ElementsAre(kLabelName1, kLabelName2, kLabelName3));
}

TEST(EntityTest, Labels) {
  TestEntity const entity{42, "foo", true};
  EXPECT_THAT(entity.labels(),
              UnorderedElementsAre(Pair(kLabelName1, VariantWith<int64_t>(42)),
                                   Pair(kLabelName2, VariantWith<std::string>("foo")),
                                   Pair(kLabelName3, VariantWith<bool>(true))));
}

TEST(EntityTest, OtherLabels) {
  TestEntity const entity{43, "bar", false};
  EXPECT_THAT(entity.labels(),
              UnorderedElementsAre(Pair(kLabelName1, VariantWith<int64_t>(43)),
                                   Pair(kLabelName2, VariantWith<std::string>("bar")),
                                   Pair(kLabelName3, VariantWith<bool>(false))));
}

TEST(EntityTest, CompareEqual) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{42, "foo", true};
  EXPECT_TRUE(entity1 == entity2);
  EXPECT_FALSE(entity1 != entity2);
  EXPECT_FALSE(entity1 < entity2);
  EXPECT_TRUE(entity1 <= entity2);
  EXPECT_FALSE(entity1 > entity2);
  EXPECT_TRUE(entity1 >= entity2);
}

TEST(EntityTest, CompareDifferent) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{43, "bar", false};
  EXPECT_FALSE(entity1 == entity2);
  EXPECT_TRUE(entity1 != entity2);
  EXPECT_FALSE(entity1 < entity2);
  EXPECT_FALSE(entity1 <= entity2);
  EXPECT_TRUE(entity1 > entity2);
  EXPECT_TRUE(entity1 >= entity2);
}

TEST(EntityTest, HashEqual) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{42, "foo", true};
  EXPECT_EQ(absl::HashOf(entity1), absl::HashOf(entity2));
}

TEST(EntityTest, HashDifferent) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{43, "bar", false};
  EXPECT_NE(absl::HashOf(entity1), absl::HashOf(entity2));
}

TEST(EntityTest, FingerprintEqual) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{42, "foo", true};
  EXPECT_EQ(FingerprintOf(entity1), FingerprintOf(entity2));
}

TEST(EntityTest, FingerprintDifferent) {
  TestEntity const entity1{42, "foo", true};
  TestEntity const entity2{43, "bar", false};
  EXPECT_NE(FingerprintOf(entity1), FingerprintOf(entity2));
}

TEST(EntityTest, ReferenceCount) {
  TestEntity const entity{42, "foo", true};
  EXPECT_EQ(entity.ref_count(), 0);
  entity.Ref();
  EXPECT_EQ(entity.ref_count(), 1);
  entity.Ref();
  EXPECT_EQ(entity.ref_count(), 2);
  EXPECT_FALSE(entity.Unref());
  EXPECT_EQ(entity.ref_count(), 1);
  EXPECT_TRUE(entity.Unref());
  EXPECT_EQ(entity.ref_count(), 0);
}

TEST(EntityTest, BlockingDestruction) {
  absl::Notification started;
  absl::Notification finished;
  tsdb2::common::reffed_ptr<TestEntity const> ptr;
  std::thread thread{[&] {
    {
      TestEntity const entity{42, "foo", true};
      ptr = &entity;
      started.Notify();
    }
    finished.Notify();
  }};
  started.WaitForNotification();
  EXPECT_THAT(ptr->labels(),
              UnorderedElementsAre(Pair(kLabelName1, VariantWith<int64_t>(42)),
                                   Pair(kLabelName2, VariantWith<std::string>("foo")),
                                   Pair(kLabelName3, VariantWith<bool>(true))));
  EXPECT_FALSE(finished.HasBeenNotified());
  ptr.reset();
  thread.join();
}

TEST(EntityTest, DefaultEntity) {
  auto const& default_entity = GetDefaultEntity();
  EXPECT_EQ(default_entity, Entity<>());
  EXPECT_THAT(default_entity.labels(), ElementsAre());
}

}  // namespace
