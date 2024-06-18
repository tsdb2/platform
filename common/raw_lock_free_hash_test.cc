#include <atomic>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

struct TriviallyDestructible {
  int foo;
};

class NonTriviallyDestructible {
 public:
  explicit NonTriviallyDestructible() : foo_(new int()) {}
  ~NonTriviallyDestructible() { delete foo_; }

 private:
  int* foo_;
};

TEST(LockFreeHashSetTest, Assumptions) {
  EXPECT_TRUE(std::is_trivially_destructible_v<std::atomic<int*>>);
  EXPECT_TRUE(std::is_trivially_destructible_v<TriviallyDestructible>);
  EXPECT_TRUE(std::is_trivially_destructible_v<std::atomic<TriviallyDestructible*>>);
  EXPECT_FALSE(std::is_trivially_destructible_v<NonTriviallyDestructible>);
  EXPECT_TRUE(std::is_trivially_destructible_v<std::atomic<NonTriviallyDestructible*>>);
}

}  // namespace
