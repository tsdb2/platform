#include "common/trie_map.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::trie_map;

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(TrieMapTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::key_type, std::string>));
  EXPECT_TRUE(
      (std::is_same_v<typename trie_map<int>::value_type, std::pair<std::string const, int>>));
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::allocator_traits,
                              std::allocator_traits<typename trie_map<int>::allocator_type>>));
  EXPECT_TRUE(
      (std::is_same_v<typename trie_map<int>::reference, std::pair<std::string const, int> &>));
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::const_reference,
                              std::pair<std::string const, int> const &>));
  EXPECT_TRUE(
      (std::is_same_v<typename trie_map<int>::pointer, std::pair<std::string const, int> *>));
  EXPECT_TRUE((std::is_same_v<typename trie_map<int>::const_pointer,
                              std::pair<std::string const, int> const *>));
}

TEST(TrieMapTest, Empty) {
  trie_map<int> const ts;
  EXPECT_TRUE(ts.empty());
  EXPECT_EQ(ts.size(), 0);
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieMapTest, OneEmptyElement) {
  trie_map<int> const ts{{"", 42}};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 1);
  EXPECT_THAT(ts, ElementsAre(Pair("", 42)));
  EXPECT_TRUE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieMapTest, OneElement) {
  trie_map<int> const ts{{"lorem", 43}};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 1);
  EXPECT_THAT(ts, ElementsAre(Pair("lorem", 43)));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("lor"));
}

// TODO

}  // namespace
