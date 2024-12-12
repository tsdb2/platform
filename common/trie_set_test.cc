#include "common/trie_set.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "common/fingerprint.h"
#include "common/re.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::tsdb2::common::FingerprintOf;
using ::tsdb2::common::RE;
using ::tsdb2::common::trie_set;

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
  trie_set const ts;
  EXPECT_TRUE(ts.empty());
  EXPECT_EQ(ts.size(), 0);
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieSetTest, OneEmptyElement) {
  trie_set const ts{""};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 1);
  EXPECT_THAT(ts, ElementsAre(""));
  EXPECT_TRUE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieSetTest, OneElement) {
  trie_set const ts{"lorem"};
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
  trie_set const ts{"lorem", "ipsum"};
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
  trie_set const ts{"ipsum", "lorem"};
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
  trie_set const ts{"", "lorem"};
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
  trie_set const ts{"lorem", ""};
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

TEST(TrieSetTest, ReverseIteration) {
  trie_set ts{"lorem", "ipsum", "dolor", "amet"};
  auto it = ts.rbegin();
  EXPECT_THAT(*it++, "lorem");
  EXPECT_THAT(*it++, "ipsum");
  EXPECT_THAT(*it++, "dolor");
  EXPECT_THAT(*it++, "amet");
  EXPECT_EQ(it, ts.rend());
}

TEST(TrieSetTest, ConstReverseIteration) {
  trie_set const ts{"lorem", "ipsum", "dolor", "amet"};
  auto it = ts.crbegin();
  EXPECT_THAT(*it++, "lorem");
  EXPECT_THAT(*it++, "ipsum");
  EXPECT_THAT(*it++, "dolor");
  EXPECT_THAT(*it++, "amet");
  EXPECT_EQ(it, ts.crend());
}

TEST(TrieSetTest, ConstructWithSharedPrefixes) {
  trie_set const ts{"abcd", "abefij", "abefgh", "loremipsum", "loremdolor"};
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
  trie_set const ts{"lorem", "lorem", "ipsum", "ipsum", "dolor"};
  EXPECT_FALSE(ts.empty());
  EXPECT_EQ(ts.size(), 3);
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum", "lorem"));
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_TRUE(ts.contains("dolor"));
}

TEST(TrieSetTest, ConstructFromIterators) {
  std::vector<std::string> v{"lorem", "", "ipsum"};
  trie_set const ts{v.begin(), v.end()};
  EXPECT_THAT(ts, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, CopyConstruct) {
  trie_set const ts1{"", "lorem", "ipsum"};
  trie_set const ts2{ts1};  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_THAT(ts1, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts1.size(), 3);
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, CopyAssign) {
  trie_set const ts1{"", "lorem", "ipsum"};
  trie_set ts2;
  ts2 = ts1;
  EXPECT_THAT(ts1, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts1.size(), 3);
  EXPECT_THAT(ts2, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, SelfCopy) {
  trie_set ts{"", "lorem", "ipsum"};
  ts = ts;  // NOLINT
  EXPECT_THAT(ts, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, MoveConstruct) {
  trie_set ts1{"", "lorem", "ipsum"};
  trie_set const ts2{std::move(ts1)};
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

TEST(TrieSetTest, SelfMove) {
  trie_set ts{"", "lorem", "ipsum"};
  ts = std::move(ts);  // NOLINT
  EXPECT_THAT(ts, ElementsAre("", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, AssignInitializerList) {
  trie_set ts{"lorem", "ipsum"};
  ts = {"lorem", "", "dolor"};
  EXPECT_THAT(ts, ElementsAre("", "dolor", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, Iterators) {
  trie_set const ts{"lorem", "ipsum", "dolor", "amet"};
  auto it1 = ts.find("lorem");
  auto it2 = ts.find("lorem");
  auto it3 = ts.find("dolor");
  auto const end = ts.end();
  EXPECT_EQ(it1, it2);
  EXPECT_NE(it1, it3);
  EXPECT_NE(it2, it3);
  EXPECT_NE(it1, end);
  EXPECT_NE(it2, end);
  EXPECT_NE(it3, end);
  EXPECT_EQ(*it1, "lorem");
  EXPECT_EQ(*it2, "lorem");
  EXPECT_EQ(*it3, "dolor");
  EXPECT_EQ(strcmp(it1->c_str(), "lorem"), 0);
  EXPECT_EQ(strcmp(it2->c_str(), "lorem"), 0);
  EXPECT_EQ(strcmp(it3->c_str(), "dolor"), 0);
  EXPECT_EQ(*++it3, "ipsum");
  EXPECT_EQ(*++it3, "lorem");
  EXPECT_EQ(it3, it1);
  EXPECT_EQ(++it3, end);
}

TEST(TrieSetTest, Hash) {
  EXPECT_EQ(absl::HashOf(trie_set({})), absl::HashOf(trie_set({})));
  EXPECT_NE(absl::HashOf(trie_set({})), absl::HashOf(trie_set({"lorem"})));
  EXPECT_EQ(absl::HashOf(trie_set({"lorem"})), absl::HashOf(trie_set({"lorem"})));
  EXPECT_NE(absl::HashOf(trie_set({"lorem"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(absl::HashOf(trie_set({"lorem", "ipsum"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(absl::HashOf(trie_set({"ipsum", "lorem"})), absl::HashOf(trie_set({"lorem", "ipsum"})));
}

TEST(TrieSetTest, Fingerprint) {
  EXPECT_EQ(FingerprintOf(trie_set({})), FingerprintOf(trie_set({})));
  EXPECT_NE(FingerprintOf(trie_set({})), FingerprintOf(trie_set({"lorem"})));
  EXPECT_EQ(FingerprintOf(trie_set({"lorem"})), FingerprintOf(trie_set({"lorem"})));
  EXPECT_NE(FingerprintOf(trie_set({"lorem"})), FingerprintOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(FingerprintOf(trie_set({"lorem", "ipsum"})),
            FingerprintOf(trie_set({"lorem", "ipsum"})));
  EXPECT_EQ(FingerprintOf(trie_set({"ipsum", "lorem"})),
            FingerprintOf(trie_set({"lorem", "ipsum"})));
}

TEST(TrieSetTest, Compare) {
  trie_set const ts1{"lorem", "ipsum", "dolor"};
  trie_set const ts2{"lorem", "ipsum", "dolor"};
  trie_set const ts3{"dolor", "amet", "consectetur"};
  EXPECT_EQ(ts1, ts2);
  EXPECT_NE(ts1, ts3);
  EXPECT_NE(ts2, ts3);
  EXPECT_FALSE(ts1 < ts2);
  EXPECT_FALSE(ts2 < ts3);
  EXPECT_TRUE(ts1 <= ts2);
  EXPECT_FALSE(ts2 <= ts3);
  EXPECT_FALSE(ts1 > ts2);
  EXPECT_TRUE(ts2 > ts3);
  EXPECT_TRUE(ts1 >= ts2);
  EXPECT_TRUE(ts2 > ts3);
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

TEST(TrieSetTest, FindInEmptySet) {
  trie_set const ts{};
  auto it = ts.find("");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lorem");
  EXPECT_EQ(it, ts.end());
  EXPECT_EQ(ts.count(""), 0);
  EXPECT_EQ(ts.count("lorem"), 0);
  EXPECT_FALSE(ts.contains(""));
  EXPECT_FALSE(ts.contains("lorem"));
}

TEST(TrieSetTest, Find) {
  trie_set const ts{"lorem", "ipsum", "lorips"};
  auto it = ts.find("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  EXPECT_EQ(ts.count("lorem"), 1);
  EXPECT_TRUE(ts.contains("lorem"));
}

TEST(TrieSetTest, NotFound) {
  trie_set const ts{"lorem", "ipsum", "lorips"};
  auto const it = ts.find("dolor");
  EXPECT_EQ(it, ts.end());
  EXPECT_EQ(ts.count("dolor"), 0);
  EXPECT_FALSE(ts.contains("dolor"));
}

TEST(TrieSetTest, FoundButNotLeaf) {
  trie_set const ts{"lorem", "ipsum", "lorips"};
  auto const it = ts.find("lor");
  EXPECT_EQ(it, ts.end());
  EXPECT_EQ(ts.count("lor"), 0);
  EXPECT_FALSE(ts.contains("lor"));
}

TEST(TrieSetTest, FindInSingleElementSet) {
  trie_set const ts{"lorem"};
  auto it = ts.find("");
  EXPECT_EQ(it, ts.end());
  it = ts.find("ipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lor");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.find("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.find("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, FindInTwoElementSet) {
  trie_set const ts{"lorem", "ipsum"};
  auto it = ts.find("");
  EXPECT_EQ(it, ts.end());
  it = ts.find("ips");
  EXPECT_EQ(it, ts.end());
  it = ts.find("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.find("ipsumdolor");
  EXPECT_EQ(it, ts.end());
  it = ts.find("justo");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lor");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.find("loremipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.find("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.find("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, LowerBoundEmptySet) {
  trie_set const ts{};
  EXPECT_EQ(ts.lower_bound(""), ts.end());
  EXPECT_EQ(ts.lower_bound("lorem"), ts.end());
}

TEST(TrieSetTest, LowerBoundSingleElementSet) {
  trie_set const ts{"lorem"};
  auto it = ts.lower_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("loramet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.lower_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, LowerBoundTwoElementSet) {
  trie_set const ts{"lorem", "ipsum"};
  auto it = ts.lower_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.lower_bound("ips");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.lower_bound("ipsamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.lower_bound("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.lower_bound("ipsumdolor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("justo");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("loramet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.lower_bound("loremipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.lower_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, LowerBoundSharedPrefix) {
  trie_set const ts{"loremamet", "loremipsum"};
  auto it = ts.lower_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.lower_bound("amet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.lower_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.lower_bound("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.lower_bound("loremamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.lower_bound("loremametamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.lower_bound("loremdolor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.lower_bound("loremipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.lower_bound("loremipsumipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.lower_bound("loremlorem");
  EXPECT_EQ(it, ts.end());
  it = ts.lower_bound("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.lower_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, UpperBoundEmptySet) {
  trie_set const ts{};
  EXPECT_EQ(ts.upper_bound(""), ts.end());
  EXPECT_EQ(ts.upper_bound("lorem"), ts.end());
}

TEST(TrieSetTest, UpperBoundSingleElementSet) {
  trie_set const ts{"lorem"};
  auto it = ts.upper_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("loramet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("lorem");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, UpperBoundTwoElementSet) {
  trie_set const ts{"lorem", "ipsum"};
  auto it = ts.upper_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.upper_bound("ips");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.upper_bound("ipsamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "ipsum");
  it = ts.upper_bound("ipsum");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("ipsumdolor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("justo");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("loramet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "lorem");
  it = ts.upper_bound("lorem");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("loremipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, UpperBoundSharedPrefix) {
  trie_set const ts{"loremamet", "loremipsum"};
  auto it = ts.upper_bound("");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.upper_bound("amet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.upper_bound("lor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.upper_bound("lorem");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremamet");
  it = ts.upper_bound("loremamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.upper_bound("loremametamet");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.upper_bound("loremdolor");
  EXPECT_NE(it, ts.end());
  EXPECT_EQ(*it, "loremipsum");
  it = ts.upper_bound("loremipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("loremipsumipsum");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("loremlorem");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("lorlor");
  EXPECT_EQ(it, ts.end());
  it = ts.upper_bound("sator");
  EXPECT_EQ(it, ts.end());
}

TEST(TrieSetTest, EqualRange) {
  trie_set const ts{"loremamet", "loremipsum"};
  auto const [lb, ub] = ts.equal_range("loremamet");
  EXPECT_NE(lb, ts.end());
  EXPECT_EQ(*lb, "loremamet");
  EXPECT_NE(ub, ts.end());
  EXPECT_EQ(*ub, "loremipsum");
}

TEST(TrieSetTest, EmptyFilteredView) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, FilteredViewOfEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, FilteredViewOfAlmostEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{""};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, FilteredViewWithEmptyString) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{""};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()), ElementsAre(""));
}

TEST(TrieSetTest, FilteredView1) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()),
              ElementsAre("lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, FilteredView2) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", ""};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()),
              ElementsAre("lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, FilteredView3) {
  auto status_or_pattern = RE::Create("lorem.+");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()),
              ElementsAre("loremamet", "loremipsum"));
}

TEST(TrieSetTest, FilteredViewOfEmptyTrie) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_set const ts;
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, UnfilteredView) {
  auto status_or_pattern = RE::Create(".*");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter(std::move(status_or_pattern).value()),
              ElementsAre("adipisci", "consectetur", "lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, EmptyPrefixView) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("adipisci", "consectetur", "lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, PrefixFilteredViewOfEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, PrefixFilteredViewOfAlmostEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{""};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, PrefixFilteredViewWithEmptyString) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{""};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre(""));
}

TEST(TrieSetTest, PrefixFilteredView1) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, PrefixFilteredView2) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", ""};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, PrefixFilteredView3) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("loremamet", "loremipsum"));
}

TEST(TrieSetTest, PartialPrefixFilteredView1) {
  auto status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", ""};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, PartialPrefixFilteredView2) {
  auto status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("loremamet", "loremipsum"));
}

TEST(TrieSetTest, AnyPrefixView) {
  auto status_or_pattern = RE::Create(".*");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre("adipisci", "consectetur", "lorem", "loremamet", "loremipsum"));
}

TEST(TrieSetTest, AnchoredPrefixView1) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre("lorem"));
}

TEST(TrieSetTest, AnchoredPrefixView2) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre("lorem"));
}

TEST(TrieSetTest, AnchoredPrefixView3) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"lorem", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre("lorem"));
}

TEST(TrieSetTest, AnchoredPrefixView4) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, AnchoredPrefixView5) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_set const ts{"loremipsum", "consectetur", "adipisci"};
  EXPECT_THAT(ts.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieSetTest, ContainsEmptyPattern) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_set const ts{"", "lorem", "ipsum"};
  EXPECT_TRUE(ts.contains(pattern));
}

TEST(TrieSetTest, DoesntContainEmptyPattern) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_set const ts{"lorem", "ipsum"};
  EXPECT_FALSE(ts.contains(pattern));
}

TEST(TrieSetTest, ContainsDeterministicPattern) {
  auto const status_or_pattern = RE::Create("loremipsum");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_TRUE(ts.contains(pattern));
}

TEST(TrieSetTest, ContainsNonDeterministicPattern) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_TRUE(ts.contains(pattern));
}

TEST(TrieSetTest, DoesntContainDeterministicPattern) {
  auto const status_or_pattern = RE::Create("loremipsum");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_set const ts{"consectetur", "adipisci", "loremlorem"};
  EXPECT_FALSE(ts.contains(pattern));
}

TEST(TrieSetTest, DoesntContainNonDeterministicPattern) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_set const ts{"consectetur", "adipisci", "loremlorem"};
  EXPECT_FALSE(ts.contains(pattern));
}

TEST(TrieSetTest, ContainsEmptyPrefix) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_set const ts{"", "lorem", "ipsum"};
  EXPECT_TRUE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, AlwaysContainsEmptyPrefix) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_set const ts{"lorem", "ipsum"};
  EXPECT_TRUE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, ContainsDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_TRUE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, ContainsNonDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_set const ts{
      "loremametdolor", "loremametsit", "loremipsumdolor", "loremipsumsit", "consectetur",
  };
  EXPECT_TRUE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, ContainsMidKeyPrefix) {
  auto const status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_set const ts{"loremamet", "loremipsum", "consectetur", "adipisci"};
  EXPECT_TRUE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, DoesntContainDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_set const ts{"ipsum", "dolor", "consectetur", "adipisci"};
  EXPECT_FALSE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, DoesntContainNonDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_set const ts{"lorem", "ipsum", "dolor", "consectetur", "adipisci"};
  EXPECT_FALSE(ts.contains_prefix(pattern));
}

TEST(TrieSetTest, DoesntContainPrefixWithFailingBoundaryAssertion) {
  auto const status_or_pattern = RE::Create("lorem\\b");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_set const ts{"loremipsum", "dolor", "consectetur", "adipisci"};
  EXPECT_FALSE(ts.contains_prefix(pattern));
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

TEST(TrieSetTest, EraseFirstIteratorFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  EXPECT_EQ(ts.erase(ts.find("loremipsum")), ts.end());
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremdolor"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseSecondIteratorFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  auto it = ts.erase(ts.find("lorem"));
  EXPECT_EQ(*it, "loremdolor");
  EXPECT_EQ(*++it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseThirdIteratorFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  auto it = ts.erase(ts.find("loremdolor"));
  EXPECT_EQ(*it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseFourthIteratorFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  auto it = ts.erase(ts.find("consectetur"));
  EXPECT_EQ(*it, "lorem");
  EXPECT_EQ(*++it, "loremdolor");
  EXPECT_EQ(*++it, "loremipsum");
  EXPECT_EQ(++it, ts.end());
  EXPECT_THAT(ts, ElementsAre("lorem", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, FastEraseFromSingleElementSet) {
  trie_set ts{"lorem"};
  ts.erase_fast(ts.find("lorem"));
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_EQ(ts.size(), 0);
}

TEST(TrieSetTest, FastEraseFirstFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  ts.erase_fast(ts.find("lorem"));
  EXPECT_THAT(ts, ElementsAre("ipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, FastEraseSecondFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  ts.erase_fast(ts.find("ipsum"));
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, FastEraseFirstWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  ts.erase_fast(ts.find("loremipsum"));
  EXPECT_THAT(ts, ElementsAre("loremdolor"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, FastEraseSecondWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  ts.erase_fast(ts.find("loremdolor"));
  EXPECT_THAT(ts, ElementsAre("loremipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, FastEraseFirstFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase_fast(ts.find("lorem"));
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseSecondFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase_fast(ts.find("ipsum"));
  EXPECT_THAT(ts, ElementsAre("dolor", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseThirdFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase_fast(ts.find("dolor"));
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseFirstFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("loremipsum"));
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseSecondFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("loremdolor"));
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseThirdFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("consectetur"));
  EXPECT_THAT(ts, ElementsAre("loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, FastEraseFirstFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("loremipsum"));
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremdolor"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, FastEraseSecondFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("lorem"));
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, FastEraseThirdFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("loremdolor"));
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, FastEraseFourthFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase_fast(ts.find("consectetur"));
  EXPECT_THAT(ts, ElementsAre("lorem", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseKeyFromSingleElementSet) {
  trie_set ts{"lorem"};
  ts.erase("lorem");
  EXPECT_THAT(ts, ElementsAre());
  EXPECT_EQ(ts.size(), 0);
}

TEST(TrieSetTest, EraseFirstKeyFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  ts.erase("lorem");
  EXPECT_THAT(ts, ElementsAre("ipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseSecondKeyFromTwoElementSet) {
  trie_set ts{"lorem", "ipsum"};
  ts.erase("ipsum");
  EXPECT_THAT(ts, ElementsAre("lorem"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseFirstKeyWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  ts.erase("loremipsum");
  EXPECT_THAT(ts, ElementsAre("loremdolor"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseSecondKeyWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor"};
  ts.erase("loremdolor");
  EXPECT_THAT(ts, ElementsAre("loremipsum"));
  EXPECT_EQ(ts.size(), 1);
}

TEST(TrieSetTest, EraseFirstKeyFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase("lorem");
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseSecondKeyFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase("ipsum");
  EXPECT_THAT(ts, ElementsAre("dolor", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseThirdKeyFromThreeElementSet) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.erase("dolor");
  EXPECT_THAT(ts, ElementsAre("ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseFirstKeyFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase("loremipsum");
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseSecondKeyFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase("loremdolor");
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseThirdKeyFromThreeElementSetWithSharedPrefix) {
  trie_set ts{"loremipsum", "loremdolor", "consectetur"};
  ts.erase("consectetur");
  EXPECT_THAT(ts, ElementsAre("loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 2);
}

TEST(TrieSetTest, EraseFirstKeyFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase("loremipsum");
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremdolor"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseSecondKeyFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase("lorem");
  EXPECT_THAT(ts, ElementsAre("consectetur", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseThirdKeyFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase("loremdolor");
  EXPECT_THAT(ts, ElementsAre("consectetur", "lorem", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, EraseFourthKeyFromSetWithTerminalPrefix) {
  trie_set ts{"loremipsum", "lorem", "loremdolor", "consectetur"};
  ts.erase("consectetur");
  EXPECT_THAT(ts, ElementsAre("lorem", "loremdolor", "loremipsum"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, Swap) {
  trie_set ts1{"lorem", "ipsum", "dolor"};
  trie_set ts2{"dolor", "amet", "consectetur", "adipisci"};
  ts1.swap(ts2);
  EXPECT_THAT(ts1, ElementsAre("adipisci", "amet", "consectetur", "dolor"));
  EXPECT_EQ(ts1.size(), 4);
  EXPECT_THAT(ts2, ElementsAre("dolor", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

TEST(TrieSetTest, SelfSwap) {
  trie_set ts{"lorem", "ipsum", "dolor"};
  ts.swap(ts);
  EXPECT_THAT(ts, ElementsAre("dolor", "ipsum", "lorem"));
  EXPECT_EQ(ts.size(), 3);
}

TEST(TrieSetTest, AdlSwap) {
  trie_set ts1{"lorem", "ipsum", "dolor"};
  trie_set ts2{"dolor", "amet", "consectetur", "adipisci"};
  swap(ts1, ts2);
  EXPECT_THAT(ts1, ElementsAre("adipisci", "amet", "consectetur", "dolor"));
  EXPECT_EQ(ts1.size(), 4);
  EXPECT_THAT(ts2, ElementsAre("dolor", "ipsum", "lorem"));
  EXPECT_EQ(ts2.size(), 3);
}

}  // namespace
