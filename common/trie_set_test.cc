#include "common/trie_set.h"

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::trie_set;

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(TrieSetTest, Traits) {
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::key_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::value_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::allocator_traits,
                              std::allocator_traits<typename trie_set<>::allocator_type>>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::reference, std::string &>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::const_reference, std::string const &>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::pointer, std::string *>));
  EXPECT_TRUE((std::is_same_v<typename trie_set<>::const_pointer, std::string const *>));
}

TEST(TrieSetTest, Empty) {
  trie_set ts;
  EXPECT_TRUE(ts.empty());
  EXPECT_EQ(ts.size(), 0);
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieSetTest, OneEmptyElement) {
  trie_set ts{""};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 1);
  EXPECT_THAT(ts, ElementsAre(""));
  EXPECT_TRUE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieSetTest, OneElement) {
  trie_set ts{"lorem"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 1);
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("lor"));
}

TEST(TrieSetTest, TwoDifferentElements) {
  trie_set ts{"lorem", "ipsum"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 2);
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("dolor"));
  EXPECT_FALSE(ts.contains("loremdolor"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("ipsumdolor"));
  EXPECT_FALSE(ts.contains("ips"));
}

TEST(TrieSetTest, TwoDifferentElementsReverse) {
  trie_set ts{"ipsum", "lorem"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 2);
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("dolor"));
  EXPECT_FALSE(ts.contains("loremdolor"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("ipsumdolor"));
  EXPECT_FALSE(ts.contains("ips"));
}

TEST(TrieSetTest, TwoElementsOneEmpty) {
  trie_set ts{"", "lorem"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 2);
  EXPECT_THAT(ts, ElementsAre("", "lorem"));
  EXPECT_TRUE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("loremdolor"));
  EXPECT_FALSE(ts.contains("lor"));
}

TEST(TrieSetTest, TwoElementsOneEmptyReverse) {
  trie_set ts{"lorem", ""};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 2);
  EXPECT_THAT(ts, ElementsAre("", "lorem"));
  EXPECT_TRUE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("loremdolor"));
  EXPECT_FALSE(ts.contains("lor"));
}

TEST(TrieSetTest, ManyElements) {
  trie_set ts{"lorem", "ipsum", "dolor", "amet"};
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_TRUE(ts.contains("dolor"));
  EXPECT_TRUE(ts.contains("amet"));
  EXPECT_FALSE(ts.contains("consectetur"));
  EXPECT_FALSE(ts.contains("adipisci"));
  EXPECT_FALSE(ts.contains("elit"));
  EXPECT_EQ(ts.erase("adipisci"), 0);
  EXPECT_EQ(ts.erase("dolor"), 1);
  EXPECT_EQ(ts.erase("dolor"), 0);
}

TEST(TrieSetTest, ConstructWithSharedPrefixes) {
  trie_set ts{"abcd", "abefij", "abefgh", "loremipsum", "loremdolor"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 5);
  EXPECT_THAT(ts, ElementsAre("abcd", "abefgh", "abefij", "loremdolor", "loremipsum"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_FALSE(ts.contains("ab"));
  EXPECT_TRUE(ts.contains("abcd"));
  EXPECT_FALSE(ts.contains("abef"));
  EXPECT_TRUE(ts.contains("abefgh"));
  EXPECT_TRUE(ts.contains("abefij"));
  EXPECT_FALSE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("loremdolor"));
  EXPECT_TRUE(ts.contains("loremipsum"));
}

TEST(TrieSetTest, ConstructWithDuplicates) {
  trie_set ts{"lorem", "lorem", "ipsum", "ipsum", "dolor"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 3);
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum", "lorem"));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_TRUE(ts.contains("dolor"));
}

TEST(TrieSetTest, ConstructFromIterators) {
  std::vector<std::string> v{"lorem", "", "ipsum"};
  trie_set ts{v.begin(), v.end()};
  EXPECT_THAT(ts, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, CopyConstruct) {
  trie_set ts1{"", "lorem", "ipsum"};
  trie_set ts2{ts1};
  EXPECT_THAT(ts1, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts1.size(), 3);
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, CopyAssign) {
  trie_set ts1{"", "lorem", "ipsum"};
  trie_set ts2;
  ts2 = ts1;
  EXPECT_THAT(ts1, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts1.size(), 3);
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, MoveConstruct) {
  trie_set ts1{"", "lorem", "ipsum"};
  trie_set ts2{std::move(ts1)};
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, MoveAssign) {
  trie_set ts1{"", "lorem", "ipsum"};
  trie_set ts2;
  ts2 = std::move(ts1);
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, AssignInitializerList) {
  trie_set ts;
  ts = {"lorem", "", "ipsum"};
  EXPECT_THAT(ts, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, Hash) {
  EXPECT_EQ(absl::HashOf(trie_set({})), absl::HashOf(trie_set({})));
  EXPECT_NE(absl::HashOf(trie_set({})), absl::HashOf(trie_set({"lorem"})));
  EXPECT_EQ(absl::HashOf(trie_set({"lorem"})), absl::HashOf(trie_set({"lorem"})));
  EXPECT_NE(absl::HashOf(trie_set({"lorem"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(absl::HashOf(trie_set({"lorem", "ipsum"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(absl::HashOf(trie_set({"ipsum", "lorem"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
}

TEST(TrieSetTest, Clear) {
  trie_set ts{"lorem", "", "ipsum"};
  ts.clear();
  EXPECT_THAT(ts, IsEmpty());
  EXPECT_EQ(ts.size(), 0);
  EXPECT_TRUE(ts.empty());
}

TEST(TrieSetTest, Insert) {
  trie_set ts;
  auto const [it, inserted] = ts.insert("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  EXPECT_TRUE(inserted);
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_EQ(ts.size(), 1);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertEmpty) {
  trie_set ts;
  auto const [it, inserted] = ts.insert("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "");
  EXPECT_TRUE(inserted);
  EXPECT_THAT(ts, ElementsAre(""));
  EXPECT_EQ(ts.size(), 1);
  EXPECT_TRUE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
}

TEST(TrieSetTest, InsertAnother) {
  trie_set ts;
  auto const [it, inserted] = ts.insert("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  EXPECT_TRUE(inserted);
  EXPECT_THAT(ts, ElementsAre("ipsum"));
  EXPECT_EQ(ts.size(), 1);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("ips"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertTwo) {
  trie_set ts;
  auto const [it1, inserted1] = ts.insert("ipsum");
  auto const [it2, inserted2] = ts.insert("lorem");
  EXPECT_NE(it2, ts.end());
  EXPECT_NE(it1, it2);
  EXPECT_EQ(*it2, "lorem");
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 2);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("ips"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertTwoReverse) {
  trie_set ts;
  auto const [it1, inserted1] = ts.insert("lorem");
  auto const [it2, inserted2] = ts.insert("ipsum");
  EXPECT_NE(it2, ts.end());
  EXPECT_NE(it1, it2);
  EXPECT_EQ(*it2, "ipsum");
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 2);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("ips"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertTwoWithEmpty) {
  trie_set ts;
  auto const [it1, inserted1] = ts.insert("");
  auto const [it2, inserted2] = ts.insert("lorem");
  EXPECT_NE(it2, ts.end());
  EXPECT_NE(it1, it2);
  EXPECT_EQ(*it2, "lorem");
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(ts, ElementsAre("", "lorem"));
  EXPECT_EQ(ts.size(), 2);
  EXPECT_TRUE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertTwoWithEmptyReverse) {
  trie_set ts;
  auto const [it1, inserted1] = ts.insert("lorem");
  auto const [it2, inserted2] = ts.insert("");
  EXPECT_NE(it2, ts.end());
  EXPECT_NE(it1, it2);
  EXPECT_EQ(*it2, "");
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(ts, ElementsAre("", "lorem"));
  EXPECT_EQ(ts.size(), 2);
  EXPECT_TRUE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("ipsum"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("loremipsum"));
  EXPECT_FALSE(ts.contains("ipsumlorem"));
}

TEST(TrieSetTest, InsertSameTwice) {
  trie_set ts;
  auto const [it1, inserted1] = ts.insert("lorem");
  auto const [it2, inserted2] = ts.insert("lorem");
  EXPECT_NE(it1, ts.end());
  EXPECT_EQ(*it1, "lorem");
  EXPECT_TRUE(inserted1);
  EXPECT_NE(it2, ts.end());
  EXPECT_EQ(it1, it2);
  EXPECT_EQ(*it2, "lorem");
  EXPECT_FALSE(inserted2);
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_EQ(ts.size(), 1);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_FALSE(ts.contains("lor"));
  EXPECT_FALSE(ts.contains("loremipsum"));
}

TEST(TrieSetTest, InsertFirstSharedPrefix) {
  trie_set ts;
  ts.insert("abcd");
  auto const [it, inserted] = ts.insert("abef");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "abef");
  EXPECT_TRUE(inserted);
  EXPECT_EQ(ts.size(), 2);
  EXPECT_THAT(ts, ElementsAre("abcd", "abef"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_FALSE(ts.contains("ab"));
  EXPECT_TRUE(ts.contains("abcd"));
  EXPECT_FALSE(ts.contains("cd"));
  EXPECT_TRUE(ts.contains("abef"));
  EXPECT_FALSE(ts.contains("ef"));
}

TEST(TrieSetTest, InsertSecondSharedPrefix) {
  trie_set ts;
  ts.insert("abcd");
  ts.insert("abefgh");
  auto const [it, inserted] = ts.insert("abefij");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "abefij");
  EXPECT_TRUE(inserted);
  EXPECT_EQ(ts.size(), 3);
  EXPECT_THAT(ts, ElementsAre("abcd", "abefgh", "abefij"));
  EXPECT_FALSE(ts.contains(""));
  EXPECT_FALSE(ts.contains("ab"));
  EXPECT_TRUE(ts.contains("abcd"));
  EXPECT_FALSE(ts.contains("cd"));
  EXPECT_FALSE(ts.contains("abef"));
  EXPECT_TRUE(ts.contains("abefgh"));
  EXPECT_TRUE(ts.contains("abefij"));
}

TEST(TrieSetTest, InsertDifferentSharedPrefixBranches) {
  trie_set ts;
  ts.insert("abcd");
  ts.insert("abefgh");
  ts.insert("abefij");
  ts.insert("cd");
  ts.insert("efgh");
  ts.insert("efij");
  EXPECT_EQ(ts.size(), 6);
  EXPECT_THAT(ts, ElementsAre("abcd", "abefgh", "abefij", "cd", "efgh", "efij"));
}

TEST(TrieSetTest, InsertFromIterators) {
  std::vector<std::string> v{"abcd", "abefgh", "abefij", "cd", "efgh", "efij"};
  trie_set ts;
  ts.insert(v.begin(), v.end());
  EXPECT_EQ(ts.size(), 6);
  EXPECT_THAT(ts, ElementsAre("abcd", "abefgh", "abefij", "cd", "efgh", "efij"));
}

TEST(TrieSetTest, InsertFromInitializerList) {
  trie_set ts;
  ts.insert({"abcd", "abefgh", "abefij", "cd", "efgh", "efij"});
  EXPECT_EQ(ts.size(), 6);
  EXPECT_THAT(ts, ElementsAre("abcd", "abefgh", "abefij", "cd", "efgh", "efij"));
}

TEST(TrieSetTest, Find) {
  trie_set ts{"lorem", "ipsum", "lorips"};
  auto const it = ts.find("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  EXPECT_EQ(ts.count("lorem"), 1);
  EXPECT_TRUE(ts.contains("lorem"));
}

TEST(TrieSetTest, NotFound) {
  trie_set ts{"lorem", "ipsum", "lorips"};
  auto const it = ts.find("dolor");
  EXPECT_EQ(it, ts.end());
  EXPECT_EQ(ts.count("dolor"), 0);
  EXPECT_FALSE(ts.contains("dolor"));
}

TEST(TrieSetTest, FoundButNotLeaf) {
  trie_set ts{"lorem", "ipsum", "lorips"};
  auto const it = ts.find("lor");
  EXPECT_EQ(it, ts.end());
  EXPECT_EQ(ts.count("lor"), 0);
  EXPECT_FALSE(ts.contains("lor"));
}

TEST(TrieSetTest, EraseIteratorFromSingleElementSet) {
  trie_set ts{"lorem"};
  EXPECT_EQ(ts.erase(ts.find("lorem")), ts.end());
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_EQ(ts.size(), 0);
}

TEST(TrieSetTest, EraseFirstIteratorFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  EXPECT_EQ(ts.erase(ts.find("lorem")), ts.end());
  EXPECT_THAT(ts, ElementsAre("ipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseSecondIteratorFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  auto it = ts.erase(ts.find("ipsum"));
  EXPECT_EQ(*it, "lorem");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseFirstIteratorWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  EXPECT_EQ(ts.erase(ts.find("loremipsum")), ts.end());
  EXPECT_THAT(ts, ElementsAre("loremdolor"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseSecondIteratorWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  auto it = ts.erase(ts.find("loremdolor"));
  EXPECT_EQ(*it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("loremipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseFirstIteratorFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  EXPECT_EQ(ts.erase(ts.find("lorem")), ts.end());
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseSecondIteratorFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  auto it = ts.erase(ts.find("ipsum"));
  EXPECT_EQ(*it, "lorem");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("dolor", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseThirdIteratorFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  auto it = ts.erase(ts.find("dolor"));
  EXPECT_EQ(*it, "ipsum");
  EXPECT_EQ(*++it, "lorem");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseFirstIteratorFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  EXPECT_EQ(ts.erase(ts.find("loremipsum")), ts.end());
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseSecondIteratorFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  auto it = ts.erase(ts.find("loremdolor"));
  EXPECT_EQ(*it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseThirdIteratorFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  auto it = ts.erase(ts.find("consectetur"));
  EXPECT_EQ(*it, "loremdolor");
  EXPECT_EQ(*++it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

// TODO: other tests.

}  // namespace
