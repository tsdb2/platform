#include "common/singleton.h"

#include <type_traits>
#include <utility>

#include "common/scoped_override.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::Singleton;

class TestSingleton final {
 public:
  explicit TestSingleton(bool *const flag, int const field) : flag_(flag), field_(field) {
    *flag_ = true;
  }

  ~TestSingleton() { *flag_ = false; }

  int field() const { return field_; }
  void set_field(int const value) { field_ = value; }

 private:
  TestSingleton(TestSingleton const &) = delete;
  TestSingleton &operator=(TestSingleton const &) = delete;
  TestSingleton(TestSingleton &&) = delete;
  TestSingleton &operator=(TestSingleton &&) = delete;

  bool *const flag_;
  int field_;
};

class SingletonTest : public ::testing::Test {
 protected:
  bool flag_ = false;
};

TEST_F(SingletonTest, TriviallyDestructible) {
  EXPECT_TRUE(std::is_trivially_destructible_v<Singleton<TestSingleton>>);
}

TEST_F(SingletonTest, DeferConstruction) {
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  EXPECT_FALSE(flag_);
}

TEST_F(SingletonTest, DeferFactoryConstruction) {
  Singleton<TestSingleton> s{[&] { return new TestSingleton(&flag_, 42); }};
  EXPECT_FALSE(flag_);
}

TEST_F(SingletonTest, Retrieve) {
  Singleton<TestSingleton> const s{std::in_place, &flag_, 42};
  EXPECT_EQ(s->field(), 42);
  EXPECT_TRUE(flag_);
}

TEST_F(SingletonTest, RetrieveFromFactory) {
  Singleton<TestSingleton> const s{[&] { return new TestSingleton(&flag_, 42); }};
  EXPECT_EQ(s->field(), 42);
  EXPECT_TRUE(flag_);
}

TEST_F(SingletonTest, NotConst) {
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s->set_field(123);
  EXPECT_EQ(s->field(), 123);
}

TEST_F(SingletonTest, Dereference) {
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  EXPECT_EQ((*s).field(), 42);
  EXPECT_TRUE(flag_);
}

TEST_F(SingletonTest, NoDestructor) {
  {
    Singleton<TestSingleton> s{std::in_place, &flag_, 42};
    s.Get();
  }
  EXPECT_TRUE(flag_);
}

TEST_F(SingletonTest, Override) {
  bool flag = false;
  TestSingleton ts{&flag, 123};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s.Override(&ts);
  EXPECT_FALSE(flag_);
  EXPECT_EQ(s->field(), 123);
  EXPECT_FALSE(flag_);
}

TEST_F(SingletonTest, OverrideAgain) {
  bool flag = false;
  TestSingleton ts1{&flag, 123};
  TestSingleton ts2{&flag, 456};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s.Override(&ts1);
  s.Override(&ts2);
  EXPECT_FALSE(flag_);
  EXPECT_EQ(s->field(), 456);
  EXPECT_FALSE(flag_);
}

TEST_F(SingletonTest, OverrideOrDie) {
  bool flag = false;
  TestSingleton ts{&flag, 123};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s.OverrideOrDie(&ts);
  EXPECT_FALSE(flag_);
  EXPECT_EQ(s->field(), 123);
  EXPECT_FALSE(flag_);
}

TEST_F(SingletonTest, OverrideButDie) {
  bool flag = false;
  TestSingleton ts1{&flag, 123};
  TestSingleton ts2{&flag, 456};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s.Override(&ts1);
  EXPECT_DEATH(s.OverrideOrDie(&ts2), _);
}

TEST_F(SingletonTest, Restore) {
  bool flag = false;
  TestSingleton ts{&flag, 123};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  s.Override(&ts);
  s.Restore();
  EXPECT_EQ(s->field(), 42);
  EXPECT_TRUE(flag_);
}

TEST_F(SingletonTest, ScopedOverride) {
  bool flag = false;
  TestSingleton ts{&flag, 123};
  Singleton<TestSingleton> s{std::in_place, &flag_, 42};
  {
    ScopedOverride so{&s, &ts};
    EXPECT_EQ(s->field(), 123);
  }
  EXPECT_EQ(s->field(), 42);
}

}  // namespace
