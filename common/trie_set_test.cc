#include "common/trie_set.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::trie_set;

using ::testing::ElementsAre;

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

// TODO: other tests

// TODO: remove this test.
TEST(TrieSetTest, Foo) {
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

// TODO

}  // namespace
