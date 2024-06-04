#include "common/lock_free_hash_set.h"

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
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
  EXPECT_FALSE(hs.contains(42));
}

TEST(LockFreeHashSetTest, ConstructWithInitializerList) {
  lock_free_hash_set<int> hs{42, 43, 44};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_FALSE(hs.contains(45));
}

TEST(LockFreeHashSetTest, ConstructFromIterators) {
  std::vector<int> v{42, 43, 44};
  lock_free_hash_set<int> hs{v.begin(), v.end()};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_FALSE(hs.contains(45));
}

TEST(LockFreeHashSetTest, InsertOneElement) {
  lock_free_hash_set<int> hs;
  auto [it, inserted] = hs.insert(42);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(++it, hs.end());
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSetTest, InsertAnotherElement) {
  lock_free_hash_set<int> hs;
  auto [it, inserted] = hs.insert(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(++it, hs.end());
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(43));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(42));
}

TEST(LockFreeHashSetTest, InsertTwoElements) {
  lock_free_hash_set<int> hs;
  hs.insert(42);
  auto [it, inserted] = hs.insert(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
}

TEST(LockFreeHashSetTest, InsertTwice) {
  lock_free_hash_set<int> hs;
  hs.insert(42);
  auto [it, inserted] = hs.insert(42);
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(++it, hs.end());
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSet, EmplaceOne) {
  lock_free_hash_set<int> hs;
  auto const [it, inserted] = hs.emplace(42);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSet, EmplaceTwo) {
  lock_free_hash_set<int> hs;
  hs.emplace(42);
  auto [it, inserted] = hs.emplace(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
}

TEST(LockFreeHashSet, EmplaceTwice) {
  lock_free_hash_set<int> hs;
  hs.emplace(42);
  auto [it, inserted] = hs.emplace(42);
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(++it, hs.end());
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSet, LookUpFromEmpty) {
  lock_free_hash_set<int> hs;
  EXPECT_FALSE(hs.contains(42));
  EXPECT_EQ(hs.find(42), hs.end());
  EXPECT_EQ(hs.count(42), 0);
}

TEST(LockFreeHashSet, LookUpOneElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_TRUE(hs.contains(42));
  EXPECT_NE(hs.find(42), hs.end());
  EXPECT_EQ(hs.count(42), 1);
  EXPECT_FALSE(hs.contains(43));
  EXPECT_EQ(hs.find(43), hs.end());
  EXPECT_EQ(hs.count(43), 0);
}

TEST(LockFreeHashSet, LookUpTwoElements) {
  lock_free_hash_set<int> hs{42, 43};
  EXPECT_TRUE(hs.contains(42));
  EXPECT_NE(hs.find(42), hs.end());
  EXPECT_EQ(hs.count(42), 1);
  EXPECT_TRUE(hs.contains(43));
  EXPECT_NE(hs.find(43), hs.end());
  EXPECT_EQ(hs.count(43), 1);
  EXPECT_FALSE(hs.contains(44));
  EXPECT_EQ(hs.find(44), hs.end());
  EXPECT_EQ(hs.count(44), 0);
}

}  // namespace
