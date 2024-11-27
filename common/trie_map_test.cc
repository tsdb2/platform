#include "common/trie_map.h"

#include <cstddef>
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

using trie_map = tsdb2::common::trie_map<int>;

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::tsdb2::common::FingerprintOf;
using ::tsdb2::common::RE;

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

TEST(TrieMapTest, ReverseIteration) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}, {"amet", 78}};
  auto it = tm.rbegin();
  EXPECT_THAT(*it++, Pair("lorem", 12));
  EXPECT_THAT(*it++, Pair("ipsum", 34));
  EXPECT_THAT(*it++, Pair("dolor", 56));
  EXPECT_THAT(*it++, Pair("amet", 78));
  EXPECT_EQ(it, tm.rend());
}

TEST(TrieMapTest, ConstReverseIteration) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}, {"amet", 78}};
  auto it = tm.crbegin();
  EXPECT_THAT(*it++, Pair("lorem", 12));
  EXPECT_THAT(*it++, Pair("ipsum", 34));
  EXPECT_THAT(*it++, Pair("dolor", 56));
  EXPECT_THAT(*it++, Pair("amet", 78));
  EXPECT_EQ(it, tm.crend());
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

TEST(TrieMapTest, Iterators) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}, {"amet", 78}};
  auto it1 = tm.find("lorem");
  auto it2 = tm.find("lorem");
  auto it3 = tm.find("dolor");
  auto const end = tm.end();
  EXPECT_EQ(it1, it2);
  EXPECT_NE(it1, it3);
  EXPECT_NE(it2, it3);
  EXPECT_NE(it1, end);
  EXPECT_NE(it2, end);
  EXPECT_NE(it3, end);
  EXPECT_THAT(*it1, Pair("lorem", 12));
  EXPECT_THAT(*it2, Pair("lorem", 12));
  EXPECT_THAT(*it3, Pair("dolor", 56));
  EXPECT_EQ(it1->first, "lorem");
  EXPECT_EQ(it1->second, 12);
  EXPECT_EQ(it2->first, "lorem");
  EXPECT_EQ(it2->second, 12);
  EXPECT_EQ(it3->first, "dolor");
  EXPECT_EQ(it3->second, 56);
  EXPECT_THAT(*++it3, Pair("ipsum", 34));
  EXPECT_THAT(*++it3, Pair("lorem", 12));
  EXPECT_EQ(it3, it1);
  EXPECT_EQ(++it3, end);
}

TEST(TrieMapTest, Hash) {
  EXPECT_EQ(absl::HashOf(trie_map({})), absl::HashOf(trie_map({})));
  EXPECT_NE(absl::HashOf(trie_map({})), absl::HashOf(trie_map({{"lorem", 12}})));
  EXPECT_EQ(absl::HashOf(trie_map({{"lorem", 12}})), absl::HashOf(trie_map({{"lorem", 12}})));
  EXPECT_NE(absl::HashOf(trie_map({{"lorem", 12}})),
            absl::HashOf(trie_map({{"lorem", 12}, {"ipsum", 12}})));
  EXPECT_EQ(absl::HashOf(trie_map({{"lorem", 12}, {"ipsum", 34}})),
            absl::HashOf(trie_map({{"lorem", 12}, {"ipsum", 34}})));
  EXPECT_EQ(absl::HashOf(trie_map({{"ipsum", 12}, {"lorem", 34}})),
            absl::HashOf(trie_map({{"lorem", 34}, {"ipsum", 12}})));
}

TEST(TrieMapTest, Fingerprint) {
  EXPECT_EQ(FingerprintOf(trie_map({})), FingerprintOf(trie_map({})));
  EXPECT_NE(FingerprintOf(trie_map({})), FingerprintOf(trie_map({{"lorem", 12}})));
  EXPECT_EQ(FingerprintOf(trie_map({{"lorem", 12}})), FingerprintOf(trie_map({{"lorem", 12}})));
  EXPECT_NE(FingerprintOf(trie_map({{"lorem", 12}})),
            FingerprintOf(trie_map({{"lorem", 12}, {"ipsum", 12}})));
  EXPECT_EQ(FingerprintOf(trie_map({{"lorem", 12}, {"ipsum", 34}})),
            FingerprintOf(trie_map({{"lorem", 12}, {"ipsum", 34}})));
  EXPECT_EQ(FingerprintOf(trie_map({{"ipsum", 12}, {"lorem", 34}})),
            FingerprintOf(trie_map({{"lorem", 34}, {"ipsum", 12}})));
}

TEST(TrieMapTest, CompareEmpty) {
  trie_map const tm1;
  trie_map const tm2;
  EXPECT_TRUE(tm1 == tm2);
  EXPECT_FALSE(tm1 != tm2);
  EXPECT_FALSE(tm1 < tm2);
  EXPECT_TRUE(tm1 <= tm2);
  EXPECT_FALSE(tm1 > tm2);
  EXPECT_TRUE(tm1 >= tm2);
}

TEST(TrieMapTest, CompareOneKeySameValue) {
  trie_map const tm1{{"lorem", 42}};
  trie_map const tm2{{"lorem", 42}};
  EXPECT_TRUE(tm1 == tm2);
  EXPECT_FALSE(tm1 != tm2);
  EXPECT_FALSE(tm1 < tm2);
  EXPECT_TRUE(tm1 <= tm2);
  EXPECT_FALSE(tm1 > tm2);
  EXPECT_TRUE(tm1 >= tm2);
}

TEST(TrieMapTest, CompareOneKeyDifferentValues) {
  trie_map const tm1{{"lorem", 42}};
  trie_map const tm2{{"lorem", 43}};
  EXPECT_FALSE(tm1 == tm2);
  EXPECT_TRUE(tm1 != tm2);
  EXPECT_TRUE(tm1 < tm2);
  EXPECT_TRUE(tm1 <= tm2);
  EXPECT_FALSE(tm1 > tm2);
  EXPECT_FALSE(tm1 >= tm2);
}

TEST(TrieMapTest, CompareDifferentKeys) {
  trie_map const tm1{{"lorem", 42}};
  trie_map const tm2{{"ipsum", 42}};
  EXPECT_FALSE(tm1 == tm2);
  EXPECT_TRUE(tm1 != tm2);
  EXPECT_FALSE(tm1 < tm2);
  EXPECT_FALSE(tm1 <= tm2);
  EXPECT_TRUE(tm1 > tm2);
  EXPECT_TRUE(tm1 >= tm2);
}

TEST(TrieMapTest, CompareSeveralKeys) {
  trie_map const tm1{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  trie_map const tm2{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  trie_map const tm3{{"dolor", 56}, {"amet", 78}, {"consectetur", 90}};
  EXPECT_EQ(tm1, tm2);
  EXPECT_NE(tm1, tm3);
  EXPECT_NE(tm2, tm3);
  EXPECT_FALSE(tm1 < tm2);
  EXPECT_FALSE(tm2 < tm3);
  EXPECT_TRUE(tm1 <= tm2);
  EXPECT_FALSE(tm2 <= tm3);
  EXPECT_FALSE(tm1 > tm2);
  EXPECT_TRUE(tm2 > tm3);
  EXPECT_TRUE(tm1 >= tm2);
  EXPECT_TRUE(tm2 > tm3);
}

TEST(TrieMapTest, Clear) {
  trie_map tm{{"lorem", 12}, {"", 34}, {"ipsum", 56}};
  tm.clear();
  EXPECT_THAT(tm, IsEmpty());
  EXPECT_EQ(tm.size(), 0);
  EXPECT_TRUE(tm.empty());
}

TEST(TrieMapTest, Insert) {
  trie_map tm;
  auto const [it, inserted] = tm.insert(std::make_pair("lorem", 42));
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 42)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertEmpty) {
  trie_map tm;
  auto const [it, inserted] = tm.insert(std::make_pair("", 42));
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("", 42));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("", 42)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
}

TEST(TrieMapTest, InsertAnother) {
  trie_map tm;
  auto const [it, inserted] = tm.insert(std::make_pair("ipsum", 43));
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 43));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 43)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertTwo) {
  trie_map tm;
  auto const [it1, inserted1] = tm.insert(std::make_pair("ipsum", 12));
  auto const [it2, inserted2] = tm.insert(std::make_pair("lorem", 34));
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 12), Pair("lorem", 34)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertTwoReverse) {
  trie_map tm;
  auto const [it1, inserted1] = tm.insert(std::make_pair("lorem", 12));
  auto const [it2, inserted2] = tm.insert(std::make_pair("ipsum", 34));
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("ipsum", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertTwoWithEmpty) {
  trie_map tm;
  auto const [it1, inserted1] = tm.insert(std::make_pair("", 12));
  auto const [it2, inserted2] = tm.insert(std::make_pair("lorem", 34));
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 12), Pair("lorem", 34)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertTwoWithEmptyReverse) {
  trie_map tm;
  auto const [it1, inserted1] = tm.insert(std::make_pair("lorem", 12));
  auto const [it2, inserted2] = tm.insert(std::make_pair("", 34));
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, InsertSameTwice) {
  trie_map tm;
  auto const [it1, inserted1] = tm.insert(std::make_pair("lorem", 12));
  auto const [it2, inserted2] = tm.insert(std::make_pair("lorem", 34));
  EXPECT_NE(it1, tm.end());
  EXPECT_THAT(*it1, Pair("lorem", 12));
  EXPECT_TRUE(inserted1);
  EXPECT_NE(it2, tm.end());
  EXPECT_EQ(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 12));
  EXPECT_FALSE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
}

TEST(TrieMapTest, InsertFirstSharedPrefix) {
  trie_map tm;
  tm.insert(std::make_pair("abcd", 12));
  auto const [it, inserted] = tm.insert(std::make_pair("abef", 34));
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("abef", 34));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abef", 34)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("ab"));
  EXPECT_TRUE(tm.contains("abcd"));
  EXPECT_FALSE(tm.contains("cd"));
  EXPECT_TRUE(tm.contains("abef"));
  EXPECT_FALSE(tm.contains("ef"));
}

TEST(TrieMapTest, InsertSecondSharedPrefix) {
  trie_map tm;
  tm.insert(std::make_pair("abcd", 12));
  tm.insert(std::make_pair("abefgh", 34));
  auto const [it, inserted] = tm.insert(std::make_pair("abefij", 56));
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("abefij", 56));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(tm.size(), 3);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 34), Pair("abefij", 56)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("ab"));
  EXPECT_TRUE(tm.contains("abcd"));
  EXPECT_FALSE(tm.contains("cd"));
  EXPECT_FALSE(tm.contains("abef"));
  EXPECT_TRUE(tm.contains("abefgh"));
  EXPECT_TRUE(tm.contains("abefij"));
}

TEST(TrieMapTest, InsertDifferentSharedPrefixBranches) {
  trie_map tm;
  tm.insert(std::make_pair("abcd", 12));
  tm.insert(std::make_pair("abefgh", 23));
  tm.insert(std::make_pair("abefij", 34));
  tm.insert(std::make_pair("cd", 45));
  tm.insert(std::make_pair("efgh", 56));
  tm.insert(std::make_pair("efij", 67));
  EXPECT_EQ(tm.size(), 6);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 23), Pair("abefij", 34),
                              Pair("cd", 45), Pair("efgh", 56), Pair("efij", 67)));
}

TEST(TrieMapTest, InsertFromIterators) {
  std::vector<std::pair<std::string, int>> v{
      {"abcd", 12}, {"abefgh", 23}, {"abefij", 34}, {"cd", 45}, {"efgh", 56}, {"efij", 67},
  };
  trie_map tm;
  tm.insert(v.begin(), v.end());
  EXPECT_EQ(tm.size(), 6);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 23), Pair("abefij", 34),
                              Pair("cd", 45), Pair("efgh", 56), Pair("efij", 67)));
}

TEST(TrieMapTest, InsertFromInitializerList) {
  trie_map tm;
  tm.insert({
      {"abcd", 12},
      {"abefgh", 23},
      {"abefij", 34},
      {"cd", 45},
      {"efgh", 56},
      {"efij", 67},
  });
  EXPECT_EQ(tm.size(), 6);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 23), Pair("abefij", 34),
                              Pair("cd", 45), Pair("efgh", 56), Pair("efij", 67)));
}

TEST(TrieMapTest, TryEmplace) {
  trie_map tm;
  auto const [it, inserted] = tm.try_emplace("lorem", 42);
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 42)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceEmpty) {
  trie_map tm;
  auto const [it, inserted] = tm.try_emplace("", 42);
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("", 42));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("", 42)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
}

TEST(TrieMapTest, TryEmplaceAnother) {
  trie_map tm;
  auto const [it, inserted] = tm.try_emplace("ipsum", 43);
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 43));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 43)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceTwo) {
  trie_map tm;
  auto const [it1, inserted1] = tm.try_emplace("ipsum", 12);
  auto const [it2, inserted2] = tm.try_emplace("lorem", 34);
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 12), Pair("lorem", 34)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceTwoReverse) {
  trie_map tm;
  auto const [it1, inserted1] = tm.try_emplace("lorem", 12);
  auto const [it2, inserted2] = tm.try_emplace("ipsum", 34);
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("ipsum", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_TRUE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("ips"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceTwoWithEmpty) {
  trie_map tm;
  auto const [it1, inserted1] = tm.try_emplace("", 12);
  auto const [it2, inserted2] = tm.try_emplace("lorem", 34);
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 12), Pair("lorem", 34)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceTwoWithEmptyReverse) {
  trie_map tm;
  auto const [it1, inserted1] = tm.try_emplace("lorem", 12);
  auto const [it2, inserted2] = tm.try_emplace("", 34);
  EXPECT_NE(it2, tm.end());
  EXPECT_NE(it1, it2);
  EXPECT_THAT(*it2, Pair("", 34));
  EXPECT_TRUE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
  EXPECT_TRUE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("ipsum"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
  EXPECT_FALSE(tm.contains("ipsumlorem"));
}

TEST(TrieMapTest, TryEmplaceSameTwice) {
  trie_map tm;
  auto const [it1, inserted1] = tm.try_emplace("lorem", 12);
  auto const [it2, inserted2] = tm.try_emplace("lorem", 34);
  EXPECT_NE(it1, tm.end());
  EXPECT_THAT(*it1, Pair("lorem", 12));
  EXPECT_TRUE(inserted1);
  EXPECT_NE(it2, tm.end());
  EXPECT_EQ(it1, it2);
  EXPECT_THAT(*it2, Pair("lorem", 12));
  EXPECT_FALSE(inserted2);
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 1);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_TRUE(tm.contains("lorem"));
  EXPECT_FALSE(tm.contains("lor"));
  EXPECT_FALSE(tm.contains("loremipsum"));
}

TEST(TrieMapTest, TryEmplaceFirstSharedPrefix) {
  trie_map tm;
  tm.try_emplace("abcd", 12);
  auto const [it, inserted] = tm.try_emplace("abef", 34);
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("abef", 34));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(tm.size(), 2);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abef", 34)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("ab"));
  EXPECT_TRUE(tm.contains("abcd"));
  EXPECT_FALSE(tm.contains("cd"));
  EXPECT_TRUE(tm.contains("abef"));
  EXPECT_FALSE(tm.contains("ef"));
}

TEST(TrieMapTest, TryEmplaceSecondSharedPrefix) {
  trie_map tm;
  tm.try_emplace("abcd", 12);
  tm.try_emplace("abefgh", 34);
  auto const [it, inserted] = tm.try_emplace("abefij", 56);
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("abefij", 56));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(tm.size(), 3);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 34), Pair("abefij", 56)));
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("ab"));
  EXPECT_TRUE(tm.contains("abcd"));
  EXPECT_FALSE(tm.contains("cd"));
  EXPECT_FALSE(tm.contains("abef"));
  EXPECT_TRUE(tm.contains("abefgh"));
  EXPECT_TRUE(tm.contains("abefij"));
}

TEST(TrieMapTest, TryEmplaceDifferentSharedPrefixBranches) {
  trie_map tm;
  tm.try_emplace("abcd", 12);
  tm.try_emplace("abefgh", 23);
  tm.try_emplace("abefij", 34);
  tm.try_emplace("cd", 45);
  tm.try_emplace("efgh", 56);
  tm.try_emplace("efij", 67);
  EXPECT_EQ(tm.size(), 6);
  EXPECT_THAT(tm, ElementsAre(Pair("abcd", 12), Pair("abefgh", 23), Pair("abefij", 34),
                              Pair("cd", 45), Pair("efgh", 56), Pair("efij", 67)));
}

TEST(TrieMapTest, FindInEmptyMap) {
  trie_map const tm;
  auto it = tm.find("");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lorem");
  EXPECT_EQ(it, tm.end());
  EXPECT_EQ(tm.count(""), 0);
  EXPECT_EQ(tm.count("lorem"), 0);
  EXPECT_FALSE(tm.contains(""));
  EXPECT_FALSE(tm.contains("lorem"));
}

TEST(TrieMapTest, Find) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"lorips", 56}};
  auto it = tm.find("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  EXPECT_EQ(tm.count("lorem"), 1);
  EXPECT_TRUE(tm.contains("lorem"));
}

TEST(TrieMapTest, NotFound) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"lorips", 56}};
  auto const it = tm.find("dolor");
  EXPECT_EQ(it, tm.end());
  EXPECT_EQ(tm.count("dolor"), 0);
  EXPECT_FALSE(tm.contains("dolor"));
}

TEST(TrieMapTest, FoundButNotLeaf) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"lorips", 56}};
  auto const it = tm.find("lor");
  EXPECT_EQ(it, tm.end());
  EXPECT_EQ(tm.count("lor"), 0);
  EXPECT_FALSE(tm.contains("lor"));
}

TEST(TrieMapTest, FindInSingleElementMap) {
  trie_map const tm{{"lorem", 42}};
  auto it = tm.find("");
  EXPECT_EQ(it, tm.end());
  it = tm.find("ipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lor");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.find("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.find("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, FindInTwoElementMap) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}};
  auto it = tm.find("");
  EXPECT_EQ(it, tm.end());
  it = tm.find("ips");
  EXPECT_EQ(it, tm.end());
  it = tm.find("ipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.find("ipsumdolor");
  EXPECT_EQ(it, tm.end());
  it = tm.find("justo");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lor");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.find("loremipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.find("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.find("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, LowerBoundEmptyMap) {
  trie_map const tm{};
  EXPECT_EQ(tm.lower_bound(""), tm.end());
  EXPECT_EQ(tm.lower_bound("lorem"), tm.end());
}

TEST(TrieMapTest, LowerBoundSingleElementMap) {
  trie_map const tm{{"lorem", 42}};
  auto it = tm.lower_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.lower_bound("ipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.lower_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.lower_bound("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.lower_bound("loramet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.lower_bound("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.lower_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, LowerBoundTwoElementMap) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}};
  auto it = tm.lower_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.lower_bound("ips");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.lower_bound("ipsamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.lower_bound("ipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.lower_bound("ipsumdolor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.lower_bound("justo");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.lower_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.lower_bound("loramet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.lower_bound("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.lower_bound("loremipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.lower_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, LowerBoundSharedPrefix) {
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}};
  auto it = tm.lower_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.lower_bound("amet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.lower_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.lower_bound("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.lower_bound("loremamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.lower_bound("loremametamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.lower_bound("loremdolor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.lower_bound("loremipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.lower_bound("loremipsumipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.lower_bound("loremlorem");
  EXPECT_EQ(it, tm.end());
  it = tm.lower_bound("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.lower_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, UpperBoundEmptyMap) {
  trie_map const tm{};
  EXPECT_EQ(tm.upper_bound(""), tm.end());
  EXPECT_EQ(tm.upper_bound("lorem"), tm.end());
}

TEST(TrieMapTest, UpperBoundSingleElementMap) {
  trie_map const tm{{"lorem", 42}};
  auto it = tm.upper_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.upper_bound("ipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.upper_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.upper_bound("loramet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 42));
  it = tm.upper_bound("lorem");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, UpperBoundTwoElementMap) {
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}};
  auto it = tm.upper_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.upper_bound("ips");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.upper_bound("ipsamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("ipsum", 34));
  it = tm.upper_bound("ipsum");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.upper_bound("ipsumdolor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.upper_bound("justo");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.upper_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.upper_bound("loramet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("lorem", 12));
  it = tm.upper_bound("lorem");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("loremipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, UpperBoundSharedPrefix) {
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}};
  auto it = tm.upper_bound("");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.upper_bound("amet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.upper_bound("lor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.upper_bound("lorem");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremamet", 12));
  it = tm.upper_bound("loremamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.upper_bound("loremametamet");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.upper_bound("loremdolor");
  EXPECT_NE(it, tm.end());
  EXPECT_THAT(*it, Pair("loremipsum", 34));
  it = tm.upper_bound("loremipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("loremipsumipsum");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("loremlorem");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("lorlor");
  EXPECT_EQ(it, tm.end());
  it = tm.upper_bound("sator");
  EXPECT_EQ(it, tm.end());
}

TEST(TrieMapTest, EqualRange) {
  trie_map const tm{{"loremamet", 34}, {"loremipsum", 12}};
  auto const [lb, ub] = tm.equal_range("loremamet");
  EXPECT_NE(lb, tm.end());
  EXPECT_THAT(*lb, Pair("loremamet", 34));
  EXPECT_NE(ub, tm.end());
  EXPECT_THAT(*ub, Pair("loremipsum", 12));
}

TEST(TrieMapTest, EmptyFilteredView) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"adipisci", 90},
  };
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, FilteredViewOfEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{};
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, FilteredViewOfAlmostEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"", 123}};
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, FilteredViewWithEmptyString) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"", 123}};
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()), ElementsAre(Pair("", 123)));
}

TEST(TrieMapTest, FilteredView1) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"adipisci", 90},
  };
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12), Pair("loremamet", 56), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, FilteredView2) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"", 90},
  };
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12), Pair("loremamet", 56), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, FilteredView3) {
  auto status_or_pattern = RE::Create("lorem.+");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"adipisci", 90},
  };
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()),
              ElementsAre(Pair("loremamet", 56), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, FilteredViewOfEmptyTrie) {
  auto status_or_pattern = RE::Create("lorem.*");
  ASSERT_OK(status_or_pattern);
  trie_map const tm;
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, UnfilteredView) {
  auto status_or_pattern = RE::Create(".*");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"adipisci", 90},
  };
  EXPECT_THAT(tm.filter(std::move(status_or_pattern).value()),
              ElementsAre(Pair("adipisci", 90), Pair("consectetur", 78), Pair("lorem", 12),
                          Pair("loremamet", 56), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, UnfilteredPrefixView) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremipsum", 34}, {"loremamet", 56}, {"consectetur", 78}, {"adipisci", 90},
  };
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("adipisci", 90), Pair("consectetur", 78), Pair("lorem", 12),
                          Pair("loremamet", 56), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, EmptyPrefixView) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"adipisci", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("adipisci", 90), Pair("consectetur", 78), Pair("lorem", 12),
                          Pair("loremamet", 34), Pair("loremipsum", 56)));
}

TEST(TrieMapTest, PrefixFilteredViewOfEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, PrefixFilteredViewOfAlmostEmptySet) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"", 123}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, PrefixFilteredViewWithEmptyString) {
  auto status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"", 123}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()), ElementsAre(Pair("", 123)));
}

TEST(TrieMapTest, PrefixFilteredView1) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"adipisci", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12), Pair("loremamet", 34), Pair("loremipsum", 56)));
}

TEST(TrieMapTest, PrefixFilteredView2) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12), Pair("loremamet", 34), Pair("loremipsum", 56)));
}

TEST(TrieMapTest, PrefixFilteredView3) {
  auto status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("loremamet", 12), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, PartialPrefixFilteredView1) {
  auto status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12), Pair("loremamet", 34), Pair("loremipsum", 56)));
}

TEST(TrieMapTest, PartialPrefixFilteredView2) {
  auto status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("loremamet", 12), Pair("loremipsum", 34)));
}

TEST(TrieMapTest, AnyPrefixView) {
  auto status_or_pattern = RE::Create(".*");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"adipisci", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("adipisci", 90), Pair("consectetur", 78), Pair("lorem", 12),
                          Pair("loremamet", 34), Pair("loremipsum", 56)));
}

TEST(TrieMapTest, AnchoredPrefixView1) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{
      {"lorem", 12}, {"loremamet", 34}, {"loremipsum", 56}, {"consectetur", 78}, {"adipisci", 90}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12)));
}

TEST(TrieMapTest, AnchoredPrefixView2) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"lorem", 12}, {"loremipsum", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12)));
}

TEST(TrieMapTest, AnchoredPrefixView3) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"lorem", 12}, {"consectetur", 34}, {"adipisci", 56}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()),
              ElementsAre(Pair("lorem", 12)));
}

TEST(TrieMapTest, AnchoredPrefixView4) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"consectetur", 12}, {"adipisci", 34}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, AnchoredPrefixView5) {
  auto status_or_pattern = RE::Create("lorem$");
  ASSERT_OK(status_or_pattern);
  trie_map const tm{{"loremipsum", 12}, {"consectetur", 34}, {"adipisci", 56}};
  EXPECT_THAT(tm.filter_prefix(std::move(status_or_pattern).value()), ElementsAre());
}

TEST(TrieMapTest, ContainsEmptyPattern) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_map const tm{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  EXPECT_TRUE(tm.contains(pattern));
}

TEST(TrieMapTest, DoesntContainEmptyPattern) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}};
  EXPECT_FALSE(tm.contains(pattern));
}

TEST(TrieMapTest, ContainsDeterministicPattern) {
  auto const status_or_pattern = RE::Create("loremipsum");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_map const tm{{"loremipsum", 12}, {"loremamet", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_TRUE(tm.contains(pattern));
}

TEST(TrieMapTest, ContainsNonDeterministicPattern) {
  auto const status_or_pattern = RE::Create("lore(mipsum|mamet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_map const tm{{"loremipsum", 12}, {"loremamet", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_TRUE(tm.contains(pattern));
}

TEST(TrieMapTest, DoesntContainDeterministicPattern) {
  auto const status_or_pattern = RE::Create("loremipsum");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_map const tm{{"consectetur", 12}, {"adipisci", 34}, {"loremlorem", 56}};
  EXPECT_FALSE(tm.contains(pattern));
}

TEST(TrieMapTest, DoesntContainNonDeterministicPattern) {
  auto const status_or_pattern = RE::Create("lore(mipsum|mamet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_map const tm{{"consectetur", 12}, {"adipisci", 34}, {"loremlorem", 56}};
  EXPECT_FALSE(tm.contains(pattern));
}

TEST(TrieMapTest, ContainsEmptyPrefix) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_map const tm{{"", 12}, {"lorem", 34}, {"ipsum", 56}};
  EXPECT_TRUE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, AlwaysContainsEmptyPrefix) {
  auto const status_or_pattern = RE::Create("");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}};
  EXPECT_TRUE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, ContainsDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_TRUE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, ContainsNonDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_map const tm{
      {"loremametdolor", 12}, {"loremametsit", 34}, {"loremipsumdolor", 56},
      {"loremipsumsit", 78},  {"consectetur", 90},
  };
  EXPECT_TRUE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, ContainsMidKeyPrefix) {
  auto const status_or_pattern = RE::Create("lor");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  trie_map const tm{{"loremamet", 12}, {"loremipsum", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_TRUE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, DoesntContainDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_map const tm{{"ipsum", 12}, {"dolor", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_FALSE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, DoesntContainNonDeterministicPrefix) {
  auto const status_or_pattern = RE::Create("lorem(ipsum|amet)");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_FALSE(pattern.IsDeterministic());
  trie_map const tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}, {"consectetur", 78}};
  EXPECT_FALSE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, DoesntContainPrefixWithFailingBoundaryAssertion) {
  auto const status_or_pattern = RE::Create("lorem\\b");
  ASSERT_OK(status_or_pattern);
  auto const &pattern = status_or_pattern.value();
  ASSERT_TRUE(pattern.IsDeterministic());
  trie_map const tm{{"loremipsum", 12}, {"dolor", 34}, {"consectetur", 56}, {"adipisci", 78}};
  EXPECT_FALSE(tm.contains_prefix(pattern));
}

TEST(TrieMapTest, EraseIteratorFromSingleElementMap) {
  trie_map tm{{"lorem", 42}};
  EXPECT_EQ(tm.erase(tm.find("lorem")), tm.end());
  EXPECT_THAT(tm, ElementsAre());
  EXPECT_EQ(tm.size(), 0);
}

TEST(TrieMapTest, EraseFirstIteratorFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  EXPECT_EQ(tm.erase(tm.find("lorem")), tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseSecondIteratorFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  auto it = tm.erase(tm.find("ipsum"));
  EXPECT_THAT(*it, Pair("lorem", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseFirstIteratorWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  EXPECT_EQ(tm.erase(tm.find("loremipsum")), tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseSecondIteratorWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  auto it = tm.erase(tm.find("loremdolor"));
  EXPECT_THAT(*it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseFirstIteratorFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  EXPECT_EQ(tm.erase(tm.find("lorem")), tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseSecondIteratorFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  auto it = tm.erase(tm.find("ipsum"));
  EXPECT_THAT(*it, Pair("lorem", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseThirdIteratorFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  auto it = tm.erase(tm.find("dolor"));
  EXPECT_THAT(*it, Pair("ipsum", 34));
  EXPECT_THAT(*++it, Pair("lorem", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseFirstIteratorFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  EXPECT_EQ(tm.erase(tm.find("loremipsum")), tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseSecondIteratorFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  auto it = tm.erase(tm.find("loremdolor"));
  EXPECT_THAT(*it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseThirdIteratorFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  auto it = tm.erase(tm.find("consectetur"));
  EXPECT_THAT(*it, Pair("loremdolor", 34));
  EXPECT_THAT(*++it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseFirstIteratorFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  EXPECT_EQ(tm.erase(tm.find("loremipsum")), tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremdolor", 56)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseSecondIteratorFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  auto it = tm.erase(tm.find("lorem"));
  EXPECT_THAT(*it, Pair("loremdolor", 56));
  EXPECT_THAT(*++it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm,
              ElementsAre(Pair("consectetur", 78), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseThirdIteratorFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  auto it = tm.erase(tm.find("loremdolor"));
  EXPECT_THAT(*it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseFourthIteratorFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  auto it = tm.erase(tm.find("consectetur"));
  EXPECT_THAT(*it, Pair("lorem", 34));
  EXPECT_THAT(*++it, Pair("loremdolor", 56));
  EXPECT_THAT(*++it, Pair("loremipsum", 12));
  EXPECT_EQ(++it, tm.end());
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 34), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, FastEraseFromSingleElementMap) {
  trie_map tm{{"lorem", 42}};
  tm.erase_fast(tm.find("lorem"));
  EXPECT_THAT(tm, ElementsAre());
  EXPECT_EQ(tm.size(), 0);
}

TEST(TrieMapTest, FastEraseFirstFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  tm.erase_fast(tm.find("lorem"));
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, FastEraseSecondFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  tm.erase_fast(tm.find("ipsum"));
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, FastEraseFirstWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  tm.erase_fast(tm.find("loremipsum"));
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, FastEraseSecondWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  tm.erase_fast(tm.find("loremdolor"));
  EXPECT_THAT(tm, ElementsAre(Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, FastEraseFirstFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase_fast(tm.find("lorem"));
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseSecondFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase_fast(tm.find("ipsum"));
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseThirdFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase_fast(tm.find("dolor"));
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseFirstFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase_fast(tm.find("loremipsum"));
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseSecondFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase_fast(tm.find("loremdolor"));
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseThirdFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase_fast(tm.find("consectetur"));
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, FastEraseFirstFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase_fast(tm.find("loremipsum"));
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremdolor", 56)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, FastEraseSecondFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase_fast(tm.find("lorem"));
  EXPECT_THAT(tm,
              ElementsAre(Pair("consectetur", 78), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, FastEraseThirdFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase_fast(tm.find("loremdolor"));
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, FastEraseFourthFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase_fast(tm.find("consectetur"));
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 34), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseKeyFromSingleElementMap) {
  trie_map tm{{"lorem", 42}};
  tm.erase("lorem");
  EXPECT_THAT(tm, ElementsAre());
  EXPECT_EQ(tm.size(), 0);
}

TEST(TrieMapTest, EraseFirstKeyFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  tm.erase("lorem");
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseSecondKeyFromTwoElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}};
  tm.erase("ipsum");
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseFirstKeyWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  tm.erase("loremipsum");
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseSecondKeyWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}};
  tm.erase("loremdolor");
  EXPECT_THAT(tm, ElementsAre(Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 1);
}

TEST(TrieMapTest, EraseFirstKeyFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase("lorem");
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("ipsum", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseSecondKeyFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase("ipsum");
  EXPECT_THAT(tm, ElementsAre(Pair("dolor", 56), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseThirdKeyFromThreeElementMap) {
  trie_map tm{{"lorem", 12}, {"ipsum", 34}, {"dolor", 56}};
  tm.erase("dolor");
  EXPECT_THAT(tm, ElementsAre(Pair("ipsum", 34), Pair("lorem", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseFirstKeyFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase("loremipsum");
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremdolor", 34)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseSecondKeyFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase("loremdolor");
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseThirdKeyFromThreeElementMapWithSharedPrefix) {
  trie_map tm{{"loremipsum", 12}, {"loremdolor", 34}, {"consectetur", 56}};
  tm.erase("consectetur");
  EXPECT_THAT(tm, ElementsAre(Pair("loremdolor", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 2);
}

TEST(TrieMapTest, EraseFirstKeyFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase("loremipsum");
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremdolor", 56)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseSecondKeyFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase("lorem");
  EXPECT_THAT(tm,
              ElementsAre(Pair("consectetur", 78), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseThirdKeyFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase("loremdolor");
  EXPECT_THAT(tm, ElementsAre(Pair("consectetur", 78), Pair("lorem", 34), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, EraseFourthKeyFromMapWithTerminalPrefix) {
  trie_map tm{{"loremipsum", 12}, {"lorem", 34}, {"loremdolor", 56}, {"consectetur", 78}};
  tm.erase("consectetur");
  EXPECT_THAT(tm, ElementsAre(Pair("lorem", 34), Pair("loremdolor", 56), Pair("loremipsum", 12)));
  EXPECT_EQ(tm.size(), 3);
}

TEST(TrieMapTest, Swap) {
  trie_map tm1{{"lorem", 12}, {"ipsum", 23}, {"dolor", 34}};
  trie_map tm2{{"dolor", 45}, {"amet", 56}, {"consectetur", 67}, {"adipisci", 78}};
  tm1.swap(tm2);
  EXPECT_THAT(tm1, ElementsAre(Pair("adipisci", 78), Pair("amet", 56), Pair("consectetur", 67),
                               Pair("dolor", 45)));
  EXPECT_EQ(tm1.size(), 4);
  EXPECT_THAT(tm2, ElementsAre(Pair("dolor", 34), Pair("ipsum", 23), Pair("lorem", 12)));
  EXPECT_EQ(tm2.size(), 3);
}

TEST(TrieMapTest, AdlSwap) {
  trie_map tm1{{"lorem", 12}, {"ipsum", 23}, {"dolor", 34}};
  trie_map tm2{{"dolor", 45}, {"amet", 56}, {"consectetur", 67}, {"adipisci", 78}};
  swap(tm1, tm2);
  EXPECT_THAT(tm1, ElementsAre(Pair("adipisci", 78), Pair("amet", 56), Pair("consectetur", 67),
                               Pair("dolor", 45)));
  EXPECT_EQ(tm1.size(), 4);
  EXPECT_THAT(tm2, ElementsAre(Pair("dolor", 34), Pair("ipsum", 23), Pair("lorem", 12)));
  EXPECT_EQ(tm2.size(), 3);
}

}  // namespace
