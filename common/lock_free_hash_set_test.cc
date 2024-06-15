#include "common/lock_free_hash_set.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
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

TEST(LockFreeHashSetTest, MaxLoad) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  };
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 24);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                       18, 19, 20, 21, 22, 23));
}

TEST(LockFreeHashSetTest, Grow) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  };
  auto const [it, inserted] = hs.insert(24);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 24);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 25);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                       18, 19, 20, 21, 22, 23, 24));
}

TEST(LockFreeHashSetTest, InsertAfterGrow) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  };
  hs.insert(24);
  auto const [it, inserted] = hs.insert(25);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 25);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 26);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                       18, 19, 20, 21, 22, 23, 24, 25));
}

TEST(LockFreeHashSetTest, EmplaceOne) {
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

TEST(LockFreeHashSetTest, EmplaceTwo) {
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

TEST(LockFreeHashSetTest, EmplaceTwice) {
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

TEST(LockFreeHashSetTest, LookUpFromEmpty) {
  lock_free_hash_set<int> hs;
  EXPECT_FALSE(hs.contains(42));
  EXPECT_EQ(hs.find(42), hs.end());
  EXPECT_EQ(hs.count(42), 0);
}

TEST(LockFreeHashSetTest, LookUpOneElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_TRUE(hs.contains(42));
  EXPECT_NE(hs.find(42), hs.end());
  EXPECT_EQ(hs.count(42), 1);
  EXPECT_FALSE(hs.contains(43));
  EXPECT_EQ(hs.find(43), hs.end());
  EXPECT_EQ(hs.count(43), 0);
}

TEST(LockFreeHashSetTest, LookUpTwoElements) {
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

TEST(LockFreeHashSetTest, TransparentLookup) {
  lock_free_hash_set<std::string> hs{"lorem", "ipsum", "dolor"};
  EXPECT_TRUE(hs.contains(std::string("lorem")));
  EXPECT_TRUE(hs.contains(std::string_view("lorem")));
  EXPECT_TRUE(hs.contains("lorem"));
  EXPECT_NE(hs.find(std::string("lorem")), hs.end());
  EXPECT_NE(hs.find(std::string_view("lorem")), hs.end());
  EXPECT_NE(hs.find("lorem"), hs.end());
  EXPECT_THAT(
      hs, UnorderedElementsAre(std::string("lorem"), std::string("ipsum"), std::string("dolor")));
  EXPECT_THAT(hs, UnorderedElementsAre(std::string_view("lorem"), std::string_view("ipsum"),
                                       std::string_view("dolor")));
  EXPECT_THAT(hs, UnorderedElementsAre("lorem", "ipsum", "dolor"));
}

TEST(LockFreeHashSetTest, LookUpWhileInserting) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.insert(42);
    hs.insert(43);
    hs.insert(44);
    hs.insert(45);
  }};
  std::thread consumer{[&] {
    while (!hs.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, LookUpWhileEmplacing) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.emplace(42);
    hs.emplace(43);
    hs.emplace(44);
    hs.emplace(45);
  }};
  std::thread consumer{[&] {
    while (!hs.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, ClearEmpty) {
  lock_free_hash_set<int> hs;
  hs.clear();
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ClearNonEmpty) {
  lock_free_hash_set<int> hs{42, 43};
  hs.clear();
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, InsertAfterClear) {
  lock_free_hash_set<int> hs{42};
  hs.clear();
  hs.insert(43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(43));
}

TEST(LockFreeHashSetTest, EraseEmpty) {
  lock_free_hash_set<int> hs;
  EXPECT_EQ(hs.erase(42), 0);
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.erase(42), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseElementTwice) {
  lock_free_hash_set<int> hs{42};
  hs.erase(42);
  EXPECT_EQ(hs.erase(42), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseMissingElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.erase(43), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_THAT(hs, UnorderedElementsAre(42));
}

TEST(LockFreeHashSetTest, EraseMissingElementTwice) {
  lock_free_hash_set<int> hs{42};
  hs.erase(43);
  EXPECT_EQ(hs.erase(43), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_THAT(hs, UnorderedElementsAre(42));
}

TEST(LockFreeHashSetTest, InsertAfterErasing) {
  lock_free_hash_set<int> hs{42, 43};
  hs.erase(43);
  auto const [it, inserted] = hs.insert(44);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 44);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, InsertErased) {
  lock_free_hash_set<int> hs{42, 43};
  hs.erase(43);
  auto const [it, inserted] = hs.insert(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
}

TEST(LockFreeHashSetTest, EraseAgain) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.erase(43);
  hs.insert(43);
  EXPECT_EQ(hs.erase(43), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, Shrink) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  };
  ASSERT_EQ(hs.capacity(), 64);
  ASSERT_EQ(hs.size(), 25);
  ASSERT_EQ(hs.erase(0), 1);
  ASSERT_EQ(hs.erase(1), 1);
  ASSERT_EQ(hs.erase(2), 1);
  ASSERT_EQ(hs.erase(3), 1);
  ASSERT_EQ(hs.erase(4), 1);
  ASSERT_EQ(hs.erase(5), 1);
  ASSERT_EQ(hs.erase(6), 1);
  ASSERT_EQ(hs.erase(7), 1);
  ASSERT_EQ(hs.erase(8), 1);
  ASSERT_EQ(hs.erase(9), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 15);
  EXPECT_THAT(hs, UnorderedElementsAre(10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24));
}

TEST(LockFreeHashSetTest, GrowAfterShrinking) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  };
  ASSERT_EQ(hs.capacity(), 64);
  ASSERT_EQ(hs.size(), 25);
  ASSERT_EQ(hs.erase(0), 1);
  ASSERT_EQ(hs.erase(1), 1);
  ASSERT_EQ(hs.erase(2), 1);
  ASSERT_EQ(hs.erase(3), 1);
  ASSERT_EQ(hs.erase(4), 1);
  ASSERT_EQ(hs.erase(5), 1);
  ASSERT_EQ(hs.erase(6), 1);
  ASSERT_EQ(hs.erase(7), 1);
  ASSERT_EQ(hs.erase(8), 1);
  ASSERT_EQ(hs.erase(9), 1);
  ASSERT_EQ(hs.capacity(), 32);
  ASSERT_EQ(hs.size(), 15);
  hs.insert({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 25);
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                       18, 19, 20, 21, 22, 23, 24));
}

TEST(LockFreeHashSetTest, ReserveZeroFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(0);
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveOneFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveTwoFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(2);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve24FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(24);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve25FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(25);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve26FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(26);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveZero) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveOne) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveThree) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(3);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveFour) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(4);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve24) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(24);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve25) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(25);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve26) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(26);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, InsertAfterReserving) {
  lock_free_hash_set<int> hs;
  hs.reserve(3);
  hs.insert(42);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42));
}

TEST(LockFreeHashSetTest, InsertReserved) {
  lock_free_hash_set<int> hs;
  hs.reserve(3);
  hs.insert(42);
  hs.insert(43);
  hs.insert(44);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, InsertMoreThanReserved) {
  lock_free_hash_set<int> hs;
  hs.reserve(3);
  hs.insert(42);
  hs.insert(43);
  hs.insert(44);
  hs.insert(45);
  hs.insert(46);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 5);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44, 45, 46));
}

TEST(LockFreeHashSetTest, GrowAfterReserving) {
  lock_free_hash_set<int> hs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  };
  hs.reserve(24);
  hs.insert(24);
  hs.insert(25);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 26);
  EXPECT_FALSE(hs.empty());
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                       18, 19, 20, 21, 22, 23, 24, 25));
}

TEST(LockFreeHashSetTest, Swap) {
  lock_free_hash_set<int> hs1{1, 2, 3, 4, 5};
  lock_free_hash_set<int> hs2{5, 6, 7};
  hs1.swap(hs2);
  EXPECT_EQ(hs1.size(), 3);
  EXPECT_THAT(hs1, UnorderedElementsAre(5, 6, 7));
  EXPECT_EQ(hs2.size(), 5);
  EXPECT_THAT(hs2, UnorderedElementsAre(1, 2, 3, 4, 5));
}

TEST(LockFreeHashSetTest, StdSwap) {
  lock_free_hash_set<int> hs1{5, 6, 7};
  lock_free_hash_set<int> hs2{1, 2, 3, 4, 5};
  std::swap(hs1, hs2);
  EXPECT_EQ(hs1.size(), 5);
  EXPECT_THAT(hs1, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(hs2.size(), 3);
  EXPECT_THAT(hs2, UnorderedElementsAre(5, 6, 7));
}

TEST(LockFreeHashSetTest, ConcurrentSwap) {
  lock_free_hash_set<int> hs1{1, 2, 3, 4, 5};
  lock_free_hash_set<int> hs2{5, 6, 7};
  std::thread t1{[&] { std::swap(hs1, hs2); }};
  std::thread t2{[&] { std::swap(hs2, hs1); }};
  t1.join();
  t2.join();
  EXPECT_EQ(hs1.size(), 5);
  EXPECT_THAT(hs1, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(hs2.size(), 3);
  EXPECT_THAT(hs2, UnorderedElementsAre(5, 6, 7));
}

// TODO: concurrent benchmarks

}  // namespace
