#include "common/trie_map.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using trie_map = tsdb2::common::trie_map<int>;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(TrieMapTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename trie_map::key_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::value_type, std::pair<std::string const, int>>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::allocator_traits,
                              std::allocator_traits<typename trie_map::allocator_type>>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::reference, std::pair<std::string const, int> &>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::const_reference,
                              std::pair<std::string const, int> const &>));
  EXPECT_TRUE((std::is_same_v<typename trie_map::pointer, std::pair<std::string const, int> *>));
  EXPECT_TRUE((
      std::is_same_v<typename trie_map::const_pointer, std::pair<std::string const, int> const *>));
}

TEST(TrieMapTest, Empty) {
  trie_map const tm;
  EXPECT_TRUE(tm.empty());
  EXPECT_EQ(tm.size(), 0);
  EXPECT_THAT(tm, ElementsAre());
  EXPECT_FALSE(tm.contains("lorem"));
}

TEST(TrieMapTest, OneEmptyElement) {
  trie_map const tm{{"", 42}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 1);
  EXPECT_THAT(tm, ElementsAre(Pair("", 42)));
  EXPECT_TRUE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
}

TEST(TrieMapTest, OneElement) {
  trie_map const tm{{"lorem", 43}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 1);
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 43)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("lor"));
}

TEST(TrieMapTest, TwoDifferentElements) {
  trie_map const tm{{"lorem", 42}, {"ipsum", 43}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 43), Pair("lorem", 42)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("dolor"));
  EXPECT_FALSE(tm.contains("loremdolor"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ipsumdolor"));
  EXPECT_FALSE(tm.contains("ips"));
}

TEST(TrieMapTest, TwoDifferentElementsReverse) {
  trie_map const tm{{"ipsum", 42}, {"lorem", 43}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 42), Pair("lorem", 43)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("dolor"));
  EXPECT_FALSE(tm.contains("loremdolor"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ipsumdolor"));
  EXPECT_FALSE(tm.contains("ips"));
}

TEST(TrieMapTest, TwoElementsOneEmpty) {
  trie_map const tm{{"", 12}, {"lorem", 34}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 12), Pair("lorem", 34)));
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("loremdolor"));
  EXPECT_FALSE(tm.contains("lor"));
}

TEST(TrieMapTest, TwoElementsOneEmptyReverse) {
  trie_map const tm{{"lorem", 12}, {"", 34}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 34), Pair("lorem", 12)));
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("loremdolor"));
  EXPECT_FALSE(tm.contains("lor"));
}

TEST(TrieMapTest, ManyElements) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}, {"amet", 78}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 4);
  EXPECT_THAT(
      tm, ElementsAre(Pair("amet", 78), Pair("dolor", 56), Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_TRUE(tm.contains("dolor"));
  EXPECT_TRUE(tm.contains("amet"));
  EXPECT_FALSE(tm.contains("consectetur"));
  EXPECT_FALSE(tm.contains("adipisci"));
  EXPECT_FALSE(tm.contains("elit"));
  EXPECT_EQ(tm.erase("adipisci"), 0);
  EXPECT_EQ(tm.erase("dolor"), 1);
  EXPECT_EQ(tm.erase("dolor"), 0);
}

TEST(TrieMapTest, ConstructWithSharedPrefixes) {
  trie_map const tm{
      {"abcd", 12}, {"abefij", 34}, {"abefgh", 56}, {"loremipsum", 78}, {"loremdolor", 90},
  };
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 5);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 56), Pair("abefij", 34),
                              Pair("loremdolor", 90), Pair("loremipsum", 78)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("ab"));
  EXPECT_TRUE(tm.contains("abcd"));
  EXPECT_FALSE(tm.contains("abef"));
  EXPECT_TRUE(tm.contains("abefgh"));
  EXPECT_TRUE(tm.contains("abefij"));
  EXPECT_FALSE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("loremdolor"));
  EXPECT_TRUE(tm.contains("loremipsum"));
}

TEST(TrieMapTest, ConstructWithDuplicates) {
  trie_map const tm{{"lorem", 12}, {"lorem", 34}, {"ipsum", 56}, {"ipsum", 78}, {"dolor", 90}};
  EXPECT_FALSE(tm.empty());
  EXPECT_EQ(tm.size(), 3);
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 90), Pair("ipsum", _), Pair("lorem", _)));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_TRUE(tm.contains("dolor"));
}

TEST(TrieMapTest, ConstructFromIterators) {
  std::vector<std::pair<std::string, int>> v{{"lorem", 12}, {"", 34}, {"ipsum", 56}};
  trie_map const tm{v.begin(), v.end()};
  EXPECT_THAT(tm, ElementsAre(Pair("", 34), Pair("ipsum", 56), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, CopyConstruct) {
  trie_map const tm1{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  trie_map const tm2{tm1};
  EXPECT_THAT(tm1, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm1.size(), 3);
  EXPECT_THAT(tm2, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm2.size(), 3);
}

TEST(TrieMapTest, CopyAssign) {
  trie_map const tm1{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  trie_map tm2;
  tm2 = tm1;
  EXPECT_THAT(tm1, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm1.size(), 3);
  EXPECT_THAT(tm2, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm2.size(), 3);
}

TEST(TrieMapTest, MoveConstruct) {
  trie_map tm1{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  trie_map const tm2{std::move(tm1)};
  EXPECT_THAT(tm2, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm2.size(), 3);
}

TEST(TrieMapTest, MoveAssign) {
  trie_map tm1{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  trie_map tm2;
  tm2 = std::move(tm1);
  EXPECT_THAT(tm2, ElementsAre(Pair("", 12), Pair("ipsum", 56), Pair("lorem", 34)));
  EXPECT_EQ(tm2.size(), 3);
}

TEST(TrieMapTest, AssignInitializerList) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  tm = {{"lorem", 56}, {"", 78}, {"dolor", 90}};
  EXPECT_THAT(tm, ElementsAre(Pair("", 78), Pair("dolor", 90), Pair("lorem", 56)));
  EXPECT_EQ(tm.size(), 3);
}

// TODO

}  // namespace
