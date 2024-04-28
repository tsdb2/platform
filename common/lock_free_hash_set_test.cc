#include "common/lock_free_hash_set.h"

#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::lock_free_hash_set;

using ::testing::ElementsAre;

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

TEST(LockFreeHashSetTest, Empty) {
  lock_free_hash_set<int> hs;
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_THAT(hs, ElementsAre());
}

TEST(LockFreeHashSetTest, OneElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_THAT(hs, ElementsAre(42));
}

// TODO

}  // namespace
