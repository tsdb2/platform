#include "common/no_destructor.h"

#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::NoDestructor;

class TestClass final {
 public:
  explicit TestClass(bool *const flag, int const field) : flag_(flag), field_(field) {
    *flag_ = true;
  }

  ~TestClass() { *flag_ = false; }

  int field() const { return field_; }
  void set_field(int const value) { field_ = value; }

 private:
  bool *const flag_;
  int field_;
};

TEST(NoDestructorTraitsTest, IsTriviallyDestructible) {
  ASSERT_FALSE(std::is_trivially_destructible_v<TestClass>);
  EXPECT_TRUE(std::is_trivially_destructible_v<NoDestructor<TestClass>>);
}

class NoDestructorTest : public ::testing::Test {
 protected:
  bool flag_ = false;
};

TEST_F(NoDestructorTest, Construction) {
  NoDestructor<TestClass> const instance{&flag_, 42};
  EXPECT_TRUE(flag_);
  EXPECT_EQ(instance->field(), 42);
}

TEST_F(NoDestructorTest, Modification) {
  NoDestructor<TestClass> instance{&flag_, 42};
  EXPECT_TRUE(flag_);
  instance->set_field(123);
  EXPECT_EQ(instance->field(), 123);
}

TEST_F(NoDestructorTest, NoDestruction) {
  { NoDestructor<TestClass>{&flag_, 42}; }
  EXPECT_TRUE(flag_);
}

}  // namespace
