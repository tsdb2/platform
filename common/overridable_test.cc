#include "common/overridable.h"

#include <string>
#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::tsdb2::common::Overridable;

class TestClass {
 public:
  explicit TestClass(std::string_view const label) : label_(label) {}

  std::string_view label() const { return label_; }
  void set_label(std::string_view const value) { label_ = value; }

 private:
  std::string label_;
};

TEST(OverridableTest, NotOverridden) {
  Overridable<TestClass> const instance{"foo"};
  EXPECT_EQ(instance->label(), "foo");
}

TEST(OverridableTest, NotConst) {
  Overridable<TestClass> instance{"foo"};
  EXPECT_EQ(instance->label(), "foo");
  instance->set_label("bar");
  EXPECT_EQ(instance->label(), "bar");
}

TEST(OverridableTest, Overridden) {
  TestClass override{"bar"};
  Overridable<TestClass> instance{"foo"};
  instance.Override(&override);
  EXPECT_EQ(instance->label(), "bar");
}

TEST(OverridableTest, OverriddenAgain) {
  TestClass o1{"bar"};
  TestClass o2{"baz"};
  Overridable<TestClass> instance{"foo"};
  instance.Override(&o1);
  instance.Override(&o2);
  EXPECT_EQ(instance->label(), "baz");
}

TEST(OverridableTest, OverrideOrDie) {
  TestClass override{"bar"};
  Overridable<TestClass> instance{"foo"};
  instance.OverrideOrDie(&override);
  EXPECT_EQ(instance->label(), "bar");
}

TEST(OverridableTest, OverrideButDie) {
  TestClass o1{"bar"};
  TestClass o2{"baz"};
  Overridable<TestClass> instance{"foo"};
  instance.Override(&o1);
  EXPECT_DEATH(instance.OverrideOrDie(&o2), _);
}

TEST(OverridableTest, Restored) {
  TestClass override{"bar"};
  Overridable<TestClass> instance{"foo"};
  instance.Override(&override);
  instance.Restore();
  EXPECT_EQ(instance->label(), "foo");
}

}  // namespace
