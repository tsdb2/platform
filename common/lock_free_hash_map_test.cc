#include "common/lock_free_hash_map.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::tsdb2::common::KnownThreadSafe;
using ::tsdb2::common::lock_free_hash_map;

TEST(LockFreeHashMapTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::key_type, int>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::value_type,
                              std::pair<int const, std::string const>>));
  EXPECT_TRUE(
      (std::is_same_v<typename lock_free_hash_map<int, KnownThreadSafe<std::string>>::value_type,
                      std::pair<int const, std::string>>));
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
                              std::pair<int const, std::string const>&>));
  EXPECT_TRUE(
      (std::is_same_v<typename lock_free_hash_map<int, KnownThreadSafe<std::string>>::reference,
                      std::pair<int const, std::string>&>));
  EXPECT_TRUE((std::is_same_v<typename lock_free_hash_map<int, std::string>::const_reference,
                              std::pair<int const, std::string const> const&>));
  EXPECT_TRUE((std::is_same_v<
               typename lock_free_hash_map<int, KnownThreadSafe<std::string>>::const_reference,
               std::pair<int const, std::string> const&>));
}

TEST(LockFreeHashMapTest, Empty) {
  lock_free_hash_map<int, std::string> hm;
  EXPECT_EQ(hm.capacity(), 0);
  EXPECT_EQ(hm.size(), 0);
  EXPECT_TRUE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre());
  EXPECT_FALSE(hm.contains(42));
}

TEST(LockFreeHashMapTest, ConstructWithInitializerList) {
  lock_free_hash_map<int, std::string> hm{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

TEST(LockFreeHashSetTest, ConstructFromIterators) {
  std::vector<std::pair<int const, std::string>> v{{42, "lorem"}, {43, "ipsum"}, {44, "dolor"}};
  lock_free_hash_map<int, std::string> hm{v.begin(), v.end()};
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 3);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem"), Pair(43, "ipsum"), Pair(44, "dolor")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_TRUE(hm.contains(43));
  EXPECT_TRUE(hm.contains(44));
  EXPECT_FALSE(hm.contains(45));
}

// TODO

}  // namespace
