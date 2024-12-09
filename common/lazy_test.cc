#include "common/lazy.h"

#include <string>
#include <string_view>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::AllOf;
using ::testing::Property;
using ::tsdb2::common::Lazy;

class TestConstructible {
 public:
  explicit TestConstructible(bool *const constructed, bool *const destroyed,
                             std::string_view const str_arg, int const int_arg)
      : destroyed_(destroyed), str_(str_arg), int_(int_arg) {
    *constructed = true;
  }

  ~TestConstructible() { *destroyed_ = true; }

  std::string_view get_str() const { return str_; }
  int get_int() const { return int_; }

 private:
  bool *destroyed_;
  std::string str_;
  int int_;
};

TEST(LazyTest, Factory) {
  bool constructed = false;
  bool destroyed = false;
  {
    Lazy<TestConstructible> lazy{
        [&] { return TestConstructible(&constructed, &destroyed, "foo", 42); }};
    EXPECT_FALSE(constructed);
    EXPECT_THAT(*lazy, AllOf(Property(&TestConstructible::get_str, "foo"),
                             Property(&TestConstructible::get_int, 42)));
    EXPECT_TRUE(constructed);
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

TEST(LazyTest, InPlaceConstruction) {
  bool constructed = false;
  bool destroyed = false;
  {
    Lazy<TestConstructible> lazy{std::in_place, &constructed, &destroyed, "bar", 43};
    EXPECT_FALSE(constructed);
    EXPECT_THAT(*lazy, AllOf(Property(&TestConstructible::get_str, "bar"),
                             Property(&TestConstructible::get_int, 43)));
    EXPECT_TRUE(constructed);
    EXPECT_FALSE(destroyed);
  }
  EXPECT_TRUE(destroyed);
}

}  // namespace
