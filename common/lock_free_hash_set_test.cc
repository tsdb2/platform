#include "common/lock_free_hash_set.h"

#include <tuple>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::lock_free_hash_set;

using ::testing::UnorderedElementsAre;

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
  EXPECT_THAT(hs, UnorderedElementsAre());
  EXPECT_FALSE(hs.contains(42));
}

TEST(LockFreeHashSetTest, OneElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSetTest, TwoElements) {
  lock_free_hash_set<int> hs{42, 43};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
}

// TODO

TEST(LockFreeHashSet, EmplaceOne) {
  lock_free_hash_set<int> hs;
  auto const [it, inserted] = hs.emplace(42);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSet, EmplaceTwo) {
  lock_free_hash_set<int> hs;
  auto [it, inserted] = hs.emplace(42);
  std::tie(it, inserted) = hs.emplace(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
}

// TODO

}  // namespace
