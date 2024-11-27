#include "common/lock_free_hash_set.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::UnorderedElementsAre;
using ::tsdb2::common::lock_free_hash_set;

inline double constexpr kEpsilon = 0.0001;

TEST(LockFreeHashSetTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::key_type, int>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::value_type, int>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::hasher, absl::Hash<int>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::key_equal, std::equal_to<int>>));
  EXPECT_TRUE(
      (std::is_same_v<typename lock_free_hash_set<int>::allocator_traits,
                      std::allocator_traits<typename lock_free_hash_set<int>::allocator_type>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::reference, int&>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_set<int>::const_reference, int const&>));
}

TEST(LockFreeHashSetTest, Empty) {
  lock_free_hash_set<int> hs;
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_NEAR(hs.max_load_factor(), 0.5, kEpsilon);
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
  EXPECT_FALSE(hs.contains(42));
}

TEST(LockFreeHashMapTest, Observers) {
  lock_free_hash_set<int> hs;
  EXPECT_EQ(absl::HashOf(42), hs.hash_function()(42));
  EXPECT_EQ(absl::HashOf(43), hs.hash_function()(43));
  EXPECT_TRUE(hs.key_eq()(42, 42));
  EXPECT_FALSE(hs.key_eq()(42, 43));
}

TEST(LockFreeHashSetTest, ConstructWithInitializerList) {
  lock_free_hash_set<int> hs{42, 43, 44};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_FALSE(hs.contains(45));
}

TEST(LockFreeHashSetTest, ConstructWithDuplicates) {
  lock_free_hash_set<int> hs{42, 43, 44, 43};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSetTest, MaxLoad) {
  lock_free_hash_set<int> hs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 16);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
}

TEST(LockFreeHashSetTest, Grow) {
  lock_free_hash_set<int> hs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto const [it, inserted] = hs.insert(16);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 16);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 17);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.265625, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
}

TEST(LockFreeHashSetTest, InsertAfterGrow) {
  lock_free_hash_set<int> hs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  hs.insert(16);
  auto const [it, inserted] = hs.insert(17);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 17);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 18);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.28125, kEpsilon);
  EXPECT_THAT(hs,
              UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17));
}

TEST(LockFreeHashSetTest, InsertingTwiceDoesntGrow) {
  lock_free_hash_set<int> hs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto const [it, inserted] = hs.insert(15);
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 15);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 16);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
}

TEST(LockFreeHashSetTest, InsertFromInitializerList) {
  lock_free_hash_set<int> hs;
  hs.insert({42, 43, 44});
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_FALSE(hs.contains(45));
}

TEST(LockFreeHashSetTest, InsertWithDuplicates) {
  lock_free_hash_set<int> hs;
  hs.insert({42, 43, 44, 43});
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_FALSE(hs.contains(45));
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42));
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
}

TEST(LockFreeHashSetTest, EmplacingTwiceDoesntGrow) {
  lock_free_hash_set<int> hs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto const [it, inserted] = hs.emplace(15);
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 15);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 16);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
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
    hs.insert(46);
  }};
  std::thread consumer{[&] {
    while (!hs.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, GetSizeWhileInserting) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.insert(42);
    hs.insert(43);
    hs.insert(44);
    hs.insert(45);
    hs.insert(46);
  }};
  std::thread consumer{[&] {
    while (hs.size() < 5) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, EraseWhileInserting) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.insert(42);
    hs.insert(43);
    hs.insert(44);
    hs.insert(45);
    hs.insert(46);
  }};
  std::thread consumer{[&] {
    while (hs.erase(44) < 1) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
  EXPECT_FALSE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 45, 46));
}

TEST(LockFreeHashSetTest, LookUpWhileEmplacing) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.emplace(42);
    hs.emplace(43);
    hs.emplace(44);
    hs.emplace(45);
    hs.emplace(46);
  }};
  std::thread consumer{[&] {
    while (!hs.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, GetSizeWhileEmplacing) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.emplace(42);
    hs.emplace(43);
    hs.emplace(44);
    hs.emplace(45);
    hs.emplace(46);
  }};
  std::thread consumer{[&] {
    while (hs.size() < 5) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashSetTest, EraseWhileEmplacing) {
  lock_free_hash_set<int> hs;
  std::thread producer{[&] {
    hs.emplace(42);
    hs.emplace(43);
    hs.emplace(44);
    hs.emplace(45);
    hs.emplace(46);
  }};
  std::thread consumer{[&] {
    while (hs.erase(44) < 1) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
  EXPECT_FALSE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 45, 46));
}

TEST(LockFreeHashSetTest, ClearEmpty) {
  lock_free_hash_set<int> hs;
  hs.clear();
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ClearNonEmpty) {
  lock_free_hash_set<int> hs{42, 43};
  hs.clear();
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, InsertAfterClear) {
  lock_free_hash_set<int> hs{42};
  hs.clear();
  hs.insert(43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(43));
}

TEST(LockFreeHashSetTest, EraseEmpty) {
  lock_free_hash_set<int> hs;
  EXPECT_EQ(hs.erase(42), 0);
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseKey) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.erase(42), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseIterator) {
  lock_free_hash_set<int> hs;
  auto const [it, inserted] = hs.insert(42);
  EXPECT_EQ(hs.erase(it), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseKeyTwice) {
  lock_free_hash_set<int> hs{42};
  hs.erase(42);
  EXPECT_EQ(hs.erase(42), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseIteratorTwice) {
  lock_free_hash_set<int> hs;
  auto const [it, inserted] = hs.insert(42);
  hs.erase(it);
  EXPECT_EQ(hs.erase(it), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_FALSE(hs.contains(42));
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, EraseMissingElement) {
  lock_free_hash_set<int> hs{42};
  EXPECT_EQ(hs.erase(43), 0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_THAT(hs, UnorderedElementsAre(42));
}

TEST(LockFreeHashSetTest, InsertAfterErasingKey) {
  lock_free_hash_set<int> hs{42, 43};
  hs.erase(43);
  auto const [it, inserted] = hs.insert(44);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 44);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, InsertAfterErasingIterator) {
  lock_free_hash_set<int> hs{42};
  auto [it, inserted] = hs.insert(43);
  hs.erase(it);
  std::tie(it, inserted) = hs.insert(44);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 44);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, InsertErasedKey) {
  lock_free_hash_set<int> hs{42, 43};
  hs.erase(43);
  auto const [it, inserted] = hs.insert(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
}

TEST(LockFreeHashSetTest, InsertErasedIterator) {
  lock_free_hash_set<int> hs{42};
  auto [it, inserted] = hs.insert(43);
  hs.erase(it);
  std::tie(it, inserted) = hs.insert(43);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hs.end());
  EXPECT_EQ(*it, 43);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_TRUE(hs.contains(43));
  EXPECT_FALSE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43));
}

TEST(LockFreeHashSetTest, EraseKeyAgain) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.erase(43);
  hs.insert(43);
  EXPECT_EQ(hs.erase(43), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, EraseIteratorAgain) {
  lock_free_hash_set<int> hs{42, 44};
  auto [it, inserted] = hs.insert(43);
  hs.erase(it);
  std::tie(it, inserted) = hs.insert(43);
  EXPECT_EQ(hs.erase(it), 1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 2);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hs.contains(42));
  EXPECT_FALSE(hs.contains(43));
  EXPECT_TRUE(hs.contains(44));
  EXPECT_THAT(hs, UnorderedElementsAre(42, 44));
}

TEST(LockFreeHashSetTest, ReserveZeroFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(0);
  EXPECT_EQ(hs.capacity(), 0);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveOneFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveTwoFromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(2);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve16FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(16);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve17FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(17);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, Reserve18FromEmpty) {
  lock_free_hash_set<int> hs;
  hs.reserve(18);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 0);
  EXPECT_TRUE(hs.empty());
  EXPECT_EQ(hs.load_factor(), 0);
  EXPECT_THAT(hs, UnorderedElementsAre());
}

TEST(LockFreeHashSetTest, ReserveZero) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(0);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveOne) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(1);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveThree) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(3);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, ReserveFour) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(4);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve16) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(16);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve17) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(17);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.046875, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, Reserve18) {
  lock_free_hash_set<int> hs{42, 43, 44};
  hs.reserve(18);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 3);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.046875, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44));
}

TEST(LockFreeHashSetTest, InsertAfterReserving) {
  lock_free_hash_set<int> hs;
  hs.reserve(3);
  hs.insert(42);
  EXPECT_EQ(hs.capacity(), 32);
  EXPECT_EQ(hs.size(), 1);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.03125, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.09375, kEpsilon);
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
  EXPECT_NEAR(hs.load_factor(), 0.15625, kEpsilon);
  EXPECT_THAT(hs, UnorderedElementsAre(42, 43, 44, 45, 46));
}

TEST(LockFreeHashSetTest, GrowAfterReserving) {
  lock_free_hash_set<int> hs;
  hs.reserve(16);
  hs.insert({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
  ASSERT_EQ(hs.capacity(), 32);
  hs.insert(16);
  hs.insert(17);
  EXPECT_EQ(hs.capacity(), 64);
  EXPECT_EQ(hs.size(), 18);
  EXPECT_FALSE(hs.empty());
  EXPECT_NEAR(hs.load_factor(), 0.28125, kEpsilon);
  EXPECT_THAT(hs,
              UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17));
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

TEST(LockFreeHashSetTest, SelfSwap) {
  lock_free_hash_set<int> hs{1, 2, 3, 4, 5};
  hs.swap(hs);
  EXPECT_EQ(hs.size(), 5);
  EXPECT_THAT(hs, UnorderedElementsAre(1, 2, 3, 4, 5));
}

TEST(LockFreeHashSetTest, StdSwap) {
  lock_free_hash_set<int> hs1{5, 6, 7};
  lock_free_hash_set<int> hs2{1, 2, 3, 4, 5};
  swap(hs1, hs2);
  EXPECT_EQ(hs1.size(), 5);
  EXPECT_THAT(hs1, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(hs2.size(), 3);
  EXPECT_THAT(hs2, UnorderedElementsAre(5, 6, 7));
}

TEST(LockFreeHashSetTest, ConcurrentSwap) {
  lock_free_hash_set<int> hs1{1, 2, 3, 4, 5};
  lock_free_hash_set<int> hs2{5, 6, 7};
  std::thread t1{[&] { swap(hs1, hs2); }};
  std::thread t2{[&] { swap(hs2, hs1); }};
  t1.join();
  t2.join();
  EXPECT_EQ(hs1.size(), 5);
  EXPECT_THAT(hs1, UnorderedElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(hs2.size(), 3);
  EXPECT_THAT(hs2, UnorderedElementsAre(5, 6, 7));
}

// TODO: concurrent benchmarks

}  // namespace
