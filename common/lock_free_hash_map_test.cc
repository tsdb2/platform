#include "common/lock_free_hash_map.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::AnyOf;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::tsdb2::common::lock_free_hash_map;

inline double constexpr kEpsilon = 0.0001;

TEST(LockFreeHashMapTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::key_type, int>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::value_type,
                              std::pair<int const, std::string>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string const>::value_type,
                              std::pair<int const, std::string const>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::size_type, size_t>));
  EXPECT_TRUE(
      (std::is_same_v<typename lock_free_hash_map<int, std::string>::difference_type, ptrdiff_t>));
  EXPECT_TRUE(
      (std::is_same_v<typename lock_free_hash_map<int, std::string>::hasher, absl::Hash<int>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::key_equal,
                              std::equal_to<int>>));
  EXPECT_TRUE(
      (std::is_same_v<
          typename lock_free_hash_map<int, std::string>::allocator_traits,
          std::allocator_traits<typename lock_free_hash_map<int, std::string>::allocator_type>>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::reference,
                              std::pair<int const, std::string>&>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string const>::reference,
                              std::pair<int const, std::string const>&>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::const_reference,
                              std::pair<int const, std::string> const&>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string const>::const_reference,
                              std::pair<int const, std::string const> const&>));
}

TEST(LockFreeHashMapTest, Empty) {
  lock_free_hash_map<int, std::string> hm;
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_NEAR(hm.max_load_factor(), 0.5, kEpsilon);
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
  EXPECT_FALSE(hm.contains(42));
}

TEST(LockFreeHashMapTest, Observers) {
  lock_free_hash_map<int, std::string> hm;
  EXPECT_EQ(absl::HashOf(42), hm.hash_function()(42));
  EXPECT_EQ(absl::HashOf(43), hm.hash_function()(43));
  EXPECT_TRUE(hm.key_eq()(42, 42));
  EXPECT_FALSE(hm.key_eq()(42, 43));
}

TEST(LockFreeHashMapTest, ConstructWithInitializerList) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashMapTest, ConstructWithDuplicates) {
  lock_free_hash_map<int, std::string> hm{
      {42, "lorem"}, {43, "ipsum"}, {44, "dolor"}, {43, "amet"}};
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, AnyOf("ipsum", "amet")),
                                       Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashMapTest, ConstructFromIterators) {
  std::vector<std::pair<int const, std::string>> v{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  lock_free_hash_map<int, std::string> hm{v.begin(), v.end()};
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashMapTest, InsertOneElement) {
  lock_free_hash_map<int, std::string> hm;
  auto [it, inserted] = hm.insert(std::make_pair(42, "lorem"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, InsertAnotherElement) {
  lock_free_hash_map<int, std::string> hm;
  auto [it, inserted] = hm.insert(std::make_pair(43, "ipsum"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(43, "ipsum")));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(42));
}

TEST(LockFreeHashMapTest, InsertTwoElements) {
  lock_free_hash_map<int, std::string> hm;
  hm.insert(std::make_pair(42, "lorem"));
  auto [it, inserted] = hm.insert(std::make_pair(43, "ipsum"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
}

TEST(LockFreeHashMapTest, InsertTwice) {
  lock_free_hash_map<int, std::string> hm;
  hm.insert(std::make_pair(42, "lorem"));
  auto [it, inserted] = hm.insert(std::make_pair(42, "ipsum"));
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, MaxLoad) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 16);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p")));
}

TEST(LockFreeHashMapTest, Grow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto const [it, inserted] = hm.insert(std::make_pair(16, "q"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(16, "q"));
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 17);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.265625, kEpsilon);
  EXPECT_THAT(
      hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                               Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                               Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"),
                               Pair(14, "o"), Pair(15, "p"), Pair(16, "q")));
}

TEST(LockFreeHashMapTest, InsertAfterGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  hm.insert(std::make_pair(16, "q"));
  auto const [it, inserted] = hm.insert(std::make_pair(17, "r"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(17, "r"));
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 18);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.28125, kEpsilon);
  EXPECT_THAT(
      hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                               Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                               Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"),
                               Pair(14, "o"), Pair(15, "p"), Pair(16, "q"), Pair(17, "r")));
}

TEST(LockFreeHashMapTest, InsertingTwiceDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto const [it, inserted] = hm.insert(std::make_pair(15, "q"));
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(15, "p"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 16);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p")));
}

TEST(LockFreeHashMapTest, InsertFromInitializerList) {
  lock_free_hash_map<int, std::string> hm;
  hm.insert({{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}});
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashMapTest, InsertWithDuplicates) {
  lock_free_hash_map<int, std::string> hm;
  hm.insert({{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}, {43, "amet"}});
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, AnyOf("ipsum", "amet")),
                                       Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashMapTest, EmplaceOne) {
  lock_free_hash_map<int, std::string> hm;
  auto const [it, inserted] = hm.emplace(42, "lorem");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, EmplaceTwo) {
  lock_free_hash_map<int, std::string> hm;
  hm.emplace(42, "lorem");
  auto [it, inserted] = hm.emplace(43, "ipsum");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
}

TEST(LockFreeHashMapTest, EmplaceTwice) {
  lock_free_hash_map<int, std::string> hm;
  hm.emplace(42, "lorem");
  auto [it, inserted] = hm.emplace(42, "ipsum");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, EmplacingTwiceDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto const [it, inserted] = hm.emplace(15, "q");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(15, "p"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 16);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p")));
}

TEST(LockFreeHashMapTest, TryEmplaceOne) {
  lock_free_hash_map<int, std::string> hm;
  auto const [it, inserted] = hm.try_emplace(42, "lorem");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, TryEmplaceTwo) {
  lock_free_hash_map<int, std::string> hm;
  hm.try_emplace(42, "lorem");
  auto [it, inserted] = hm.try_emplace(43, "ipsum");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
}

TEST(LockFreeHashMapTest, TryEmplaceTwice) {
  lock_free_hash_map<int, std::string> hm;
  hm.try_emplace(42, "lorem");
  auto [it, inserted] = hm.try_emplace(42, "ipsum");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, TryingToEmplaceTwiceDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto const [it, inserted] = hm.try_emplace(15, "q");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(15, "p"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 16);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p")));
}

TEST(LockFreeHashMapTest, LookUpFromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  EXPECT_FALSE(hm.contains(42));
  EXPECT_EQ(hm.find(42), hm.end());
  EXPECT_EQ(hm.count(42), 0);
}

TEST(LockFreeHashMapTest, LookUpOneElement) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  EXPECT_TRUE(hm.contains(42));
  EXPECT_NE(hm.find(42), hm.end());
  EXPECT_EQ(hm.count(42), 1);
  EXPECT_FALSE(hm.contains(43));
  EXPECT_EQ(hm.find(43), hm.end());
  EXPECT_EQ(hm.count(43), 0);
  EXPECT_EQ(hm.at(42), "lorem");
  EXPECT_EQ(hm[42], "lorem");
}

TEST(LockFreeHashMapTest, LookUpTwoElements) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}};
  EXPECT_TRUE(hm.contains(42));
  auto it = hm.find(42);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(hm.count(42), 1);
  EXPECT_EQ(hm.at(42), "lorem");
  EXPECT_EQ(hm[42], "lorem");
  EXPECT_TRUE(hm.contains(43));
  it = hm.find(43);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(hm.count(43), 1);
  EXPECT_EQ(hm.at(43), "ipsum");
  EXPECT_EQ(hm[43], "ipsum");
  EXPECT_FALSE(hm.contains(44));
  EXPECT_EQ(hm.find(44), hm.end());
  EXPECT_EQ(hm.count(44), 0);
}

TEST(LockFreeHashMapTest, TransparentLookup) {
  lock_free_hash_map<std::string, int> hm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  EXPECT_TRUE(hm.contains(std::string("lorem")));
  EXPECT_TRUE(hm.contains(std::string_view("lorem")));
  EXPECT_TRUE(hm.contains("lorem"));
  auto it = hm.find(std::string("lorem"));
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = hm.find(std::string_view("lorem"));
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = hm.find("lorem");
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  EXPECT_THAT(hm,
              UnorderedElementsAre(Pair(std::string("lorem"), 12), Pair(std::string("ipsum"), 34),
                                   Pair(std::string("dolor"), 56)));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(std::string_view("lorem"), 12),
                                       Pair(std::string_view("ipsum"), 34),
                                       Pair(std::string_view("dolor"), 56)));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair("lorem", 12), Pair("ipsum", 34), Pair("dolor", 56)));
}

TEST(LockFreeHashMapTest, LookUpWhileInserting) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.insert(std::make_pair(42, "lorem"));
    hm.insert(std::make_pair(43, "ipsum"));
    hm.insert(std::make_pair(44, "dolor"));
    hm.insert(std::make_pair(45, "amet"));
    hm.insert(std::make_pair(46, "consectetur"));
  }};
  std::thread consumer{[&] {
    while (!hm.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashMapTest, GetSizeWhileInserting) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.insert(std::make_pair(42, "lorem"));
    hm.insert(std::make_pair(43, "ipsum"));
    hm.insert(std::make_pair(44, "dolor"));
    hm.insert(std::make_pair(45, "amet"));
    hm.insert(std::make_pair(46, "consectetur"));
  }};
  std::thread consumer{[&] {
    while (hm.size() < 5) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashMapTest, EraseWhileInserting) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.insert(std::make_pair(42, "lorem"));
    hm.insert(std::make_pair(43, "ipsum"));
    hm.insert(std::make_pair(44, "dolor"));
    hm.insert(std::make_pair(45, "amet"));
    hm.insert(std::make_pair(46, "consectetur"));
  }};
  std::thread consumer{[&] {
    while (hm.erase(44) < 1) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
  EXPECT_FALSE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(45, "amet"),
                                       Pair(46, "consectetur")));
}

TEST(LockFreeHashMapTest, LookUpWhileEmplacing) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.try_emplace(42, "lorem");
    hm.try_emplace(43, "ipsum");
    hm.try_emplace(44, "dolor");
    hm.try_emplace(45, "amet");
    hm.try_emplace(46, "consectetur");
  }};
  std::thread consumer{[&] {
    while (!hm.contains(45)) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashMapTest, GetSizeWhileEmplacing) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.try_emplace(42, "lorem");
    hm.try_emplace(43, "ipsum");
    hm.try_emplace(44, "dolor");
    hm.try_emplace(45, "amet");
    hm.try_emplace(46, "consectetur");
  }};
  std::thread consumer{[&] {
    while (hm.size() < 5) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
}

TEST(LockFreeHashMapTest, EraseWhileEmplacing) {
  lock_free_hash_map<int, std::string const> hm;
  std::thread producer{[&] {
    hm.try_emplace(42, "lorem");
    hm.try_emplace(43, "ipsum");
    hm.try_emplace(44, "dolor");
    hm.try_emplace(45, "amet");
    hm.try_emplace(46, "consectetur");
  }};
  std::thread consumer{[&] {
    while (hm.erase(44) < 1) {
      // loop
    }
  }};
  producer.join();
  consumer.join();
  EXPECT_FALSE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(45, "amet"),
                                       Pair(46, "consectetur")));
}

TEST(LockFreeHashMapTest, InsertOrAssignNew) {
  lock_free_hash_map<int, std::string> hm;
  auto [it, inserted] = hm.insert_or_assign(42, "lorem");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "lorem"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, InsertOrAssignTransparent) {
  lock_free_hash_map<std::string, int> hm;
  auto [it, inserted] = hm.insert_or_assign(std::string_view("lorem"), 42);
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair("lorem", 42)));
  EXPECT_TRUE(hm.contains("lorem"));
  EXPECT_FALSE(hm.contains("ipsum"));
}

TEST(LockFreeHashMapTest, InsertOrAssignExisting) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  auto [it, inserted] = hm.insert_or_assign(42, "ipsum");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "ipsum"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, InsertOrAssignAnotherNewElement) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  auto [it, inserted] = hm.insert_or_assign(43, "ipsum");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "ipsum"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
}

TEST(LockFreeHashMapTest, InsertOrAssignAnotherExistingElement) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}};
  auto [it, inserted] = hm.insert_or_assign(43, "dolor");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "dolor"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
}

TEST(LockFreeHashMapTest, InsertOrAssignErased) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  hm.erase(42);
  auto [it, inserted] = hm.insert_or_assign(42, "ipsum");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(42, "ipsum"));
  EXPECT_EQ(++it, hm.end());
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "ipsum")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, InsertOrAssignGrows) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto [it, inserted] = hm.insert_or_assign(16, "q");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(16, "q"));
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 17);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.265625, kEpsilon);
  EXPECT_THAT(
      hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                               Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                               Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"),
                               Pair(14, "o"), Pair(15, "p"), Pair(16, "q")));
}

TEST(LockFreeHashMapTest, InsertOrAssignExistingDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"}, {1, "b"}, {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"}, {9, "j"}, {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
  };
  auto [it, inserted] = hm.insert_or_assign(15, "q");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(15, "q"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 16);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.5, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "q")));
}

TEST(LockFreeHashMapTest, ClearEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.clear();
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, ClearNonEmpty) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}};
  hm.clear();
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EmplaceAfterClear) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  hm.clear();
  hm.try_emplace(43, "ipsum");
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(43, "ipsum")));
}

TEST(LockFreeHashMapTest, EraseEmpty) {
  lock_free_hash_map<int, std::string> hm;
  EXPECT_EQ(hm.erase(42), 0);
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_FALSE(hm.contains(42));
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EraseKey) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  EXPECT_EQ(hm.erase(42), 1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_FALSE(hm.contains(42));
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EraseIterator) {
  lock_free_hash_map<int, std::string> hm;
  auto const [it, inserted] = hm.try_emplace(42, "lorem");
  EXPECT_EQ(hm.erase(it), 1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_FALSE(hm.contains(42));
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EraseKeyTwice) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  hm.erase(42);
  EXPECT_EQ(hm.erase(42), 0);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_FALSE(hm.contains(42));
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EraseIteratorTwice) {
  lock_free_hash_map<int, std::string> hm;
  auto const [it, inserted] = hm.try_emplace(42, "lorem");
  hm.erase(it);
  EXPECT_EQ(hm.erase(it), 0);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_FALSE(hm.contains(42));
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, EraseMissingElement) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  EXPECT_EQ(hm.erase(43), 0);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
}

TEST(LockFreeHashMapTest, EraseMissingElementTwice) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  hm.erase(43);
  EXPECT_EQ(hm.erase(43), 0);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
}

TEST(LockFreeHashMapTest, EmplaceAfterErasingKey) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}};
  hm.erase(43);
  auto const [it, inserted] = hm.try_emplace(44, "dolor");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(44, "dolor"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, EmplaceAfterErasingIterator) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  auto [it, inserted] = hm.try_emplace(43, "ipsum");
  hm.erase(it);
  std::tie(it, inserted) = hm.try_emplace(44, "dolor");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(44, "dolor"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, EmplaceErasedKey) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}};
  hm.erase(43);
  auto const [it, inserted] = hm.try_emplace(43, "dolor");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "dolor"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "dolor")));
}

TEST(LockFreeHashMapTest, EmplaceErasedIterator) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}};
  auto [it, inserted] = hm.try_emplace(43, "ipsum");
  hm.erase(it);
  std::tie(it, inserted) = hm.try_emplace(43, "dolor");
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(43, "dolor"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_FALSE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "dolor")));
}

TEST(LockFreeHashMapTest, EraseKeyAgain) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.erase(43);
  hm.try_emplace(43, "amet");
  EXPECT_EQ(hm.erase(43), 1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, EraseIteratorAgain) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {44, "ipsum"}};
  auto [it, inserted] = hm.try_emplace(43, "dolor");
  hm.erase(it);
  std::tie(it, inserted) = hm.try_emplace(43, "amet");
  EXPECT_EQ(hm.erase(it), 1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 2);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.0625, kEpsilon);
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(44, "ipsum")));
}

TEST(LockFreeHashMapTest, ReserveZeroFromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(0);
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, ReserveOneFromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, ReserveTwoFromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(2);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, Reserve16FromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(16);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, Reserve17FromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(17);
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, Reserve18FromEmpty) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(18);
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_EQ(hm.load_factor(), 0);
  EXPECT_THAT(hm, UnorderedElementsAre());
}

TEST(LockFreeHashMapTest, ReserveZero) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(0);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, ReserveOne) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(1);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, ReserveThree) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(3);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, ReserveFour) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(4);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, Reserve16) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(16);
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, Reserve17) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(17);
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.046875, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, Reserve18) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  hm.reserve(18);
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.046875, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, EmplaceAfterReserving) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(3);
  hm.try_emplace(42, "lorem");
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 1);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.03125, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
}

TEST(LockFreeHashMapTest, EmplaceReserved) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(3);
  hm.try_emplace(42, "lorem");
  hm.try_emplace(43, "ipsum");
  hm.try_emplace(44, "dolor");
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.09375, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
}

TEST(LockFreeHashMapTest, EmplaceMoreThanReserved) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(3);
  hm.try_emplace(42, "lorem");
  hm.try_emplace(43, "ipsum");
  hm.try_emplace(44, "dolor");
  hm.try_emplace(45, "amet");
  hm.try_emplace(46, "consectetur");
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 5);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.15625, kEpsilon);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor"),
                                       Pair(45, "amet"), Pair(46, "consectetur")));
}

TEST(LockFreeHashMapTest, GrowAfterReserving) {
  lock_free_hash_map<int, std::string> hm;
  hm.reserve(16);
  hm.insert({
      {0, "a"},
      {1, "b"},
      {2, "c"},
      {3, "d"},
      {4, "e"},
      {5, "f"},
      {6, "g"},
      {7, "h"},
      {8, "i"},
      {9, "j"},
      {10, "k"},
      {11, "l"},
      {12, "m"},
      {13, "n"},
      {14, "o"},
      {15, "p"},
  });
  ASSERT_EQ(hm.capacity(), 32);
  hm.try_emplace(16, "q");
  hm.try_emplace(17, "r");
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 18);
  EXPECT_FALSE(hm.empty());
  EXPECT_NEAR(hm.load_factor(), 0.28125, kEpsilon);
  EXPECT_THAT(
      hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                               Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                               Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"),
                               Pair(14, "o"), Pair(15, "p"), Pair(16, "q"), Pair(17, "r")));
}

TEST(LockFreeHashMapTest, Swap) {
  lock_free_hash_map<int, std::string> hm1{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  lock_free_hash_map<int, std::string> hm2{{5, "f"}, {6, "g"}, {7, "h"}};
  hm1.swap(hm2);
  EXPECT_EQ(hm1.size(), 3);
  EXPECT_THAT(hm1, UnorderedElementsAre(Pair(5, "f"), Pair(6, "g"), Pair(7, "h")));
  EXPECT_EQ(hm2.size(), 5);
  EXPECT_THAT(hm2, UnorderedElementsAre(Pair(1, "a"), Pair(2, "b"), Pair(3, "c"), Pair(4, "d"),
                                        Pair(5, "e")));
}

TEST(LockFreeHashMapTest, SelfSwap) {
  lock_free_hash_map<int, std::string> hm{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  hm.swap(hm);
  EXPECT_EQ(hm.size(), 5);
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(1, "a"), Pair(2, "b"), Pair(3, "c"), Pair(4, "d"),
                                       Pair(5, "e")));
}

TEST(LockFreeHashMapTest, AdlSwap) {
  lock_free_hash_map<int, std::string> hm1{{5, "f"}, {6, "g"}, {7, "h"}};
  lock_free_hash_map<int, std::string> hm2{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  swap(hm1, hm2);
  EXPECT_EQ(hm1.size(), 5);
  EXPECT_THAT(hm1, UnorderedElementsAre(Pair(1, "a"), Pair(2, "b"), Pair(3, "c"), Pair(4, "d"),
                                        Pair(5, "e")));
  EXPECT_EQ(hm2.size(), 3);
  EXPECT_THAT(hm2, UnorderedElementsAre(Pair(5, "f"), Pair(6, "g"), Pair(7, "h")));
}

TEST(LockFreeHashMapTest, ConcurrentSwap) {
  lock_free_hash_map<int, std::string> hm1{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  lock_free_hash_map<int, std::string> hm2{{5, "f"}, {6, "g"}, {7, "h"}};
  std::thread t1{[&] { swap(hm1, hm2); }};
  std::thread t2{[&] { swap(hm2, hm1); }};
  t1.join();
  t2.join();
  EXPECT_EQ(hm1.size(), 5);
  EXPECT_THAT(hm1, UnorderedElementsAre(Pair(1, "a"), Pair(2, "b"), Pair(3, "c"), Pair(4, "d"),
                                        Pair(5, "e")));
  EXPECT_EQ(hm2.size(), 3);
  EXPECT_THAT(hm2, UnorderedElementsAre(Pair(5, "f"), Pair(6, "g"), Pair(7, "h")));
}

// TODO: concurrent benchmarks

}  // namespace
