#include "common/scoped_override.h"

#include <string>
#include <string_view>
#include <utility>

#include "common/overridable.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::tsdb2::common::Overridable;
using ::tsdb2::common::ScopedOverride;

class TestClass {
 public:
  explicit TestClass(std::string_view const label) : label_(label) {}

  std::string_view label() const { return label_; }

 private:
  std::string const label_;
};

TEST(ScopedOverrideTest, ScopedOverridable) {
  TestClass override{"bar"};
  Overridable<TestClass> instance{"foo"};
  {
    ScopedOverride so{&instance, &override};
    EXPECT_EQ(instance->label(), "bar");
  }
  EXPECT_EQ(instance->label(), "foo");
}

TEST(ScopedOverrideDeathTest, NestedScopedOverridable) {
  TestClass o1{"bar"};
  TestClass o2{"baz"};
  Overridable<TestClass> instance{"foo"};
  ScopedOverride so{&instance, &o1};
  EXPECT_DEATH(ScopedOverride(&instance, &o2), _);
}

TEST(ScopedOverrideTest, MoveConstructScopedOverridable) {
  TestClass override{"bar"};
  Overridable<TestClass> instance{"foo"};
  ScopedOverride so1{&instance, &override};
  {
    ScopedOverride so2{std::move(so1)};
    EXPECT_EQ(instance->label(), "bar");
  }
  EXPECT_EQ(instance->label(), "foo");
}

TEST(ScopedOverrideTest, MoveScopedOverridable) {
  TestClass o1{"bar"};
  TestClass o2{""};
  Overridable<TestClass> instance{"foo"};
  ScopedOverride so1{&instance, &o1};
  {
    Overridable<TestClass> dummy{""};
    ScopedOverride so2{&dummy, &o2};
    so2 = std::move(so1);
    EXPECT_EQ(instance->label(), "bar");
  }
  EXPECT_EQ(instance->label(), "foo");
}

}  // namespace
