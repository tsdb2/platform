#include "common/trie_set.h"

#include "gtest/gtest.h"

namespace {

using tsdb2::common::trie_set;

TEST(TrieSetTest, Foo) {
  trie_set ts{"lorem", "ipsum", "dolor", "amet"};
  EXPECT_TRUE(ts.contains("lorem"));
  EXPECT_TRUE(ts.contains("ipsum"));
  EXPECT_TRUE(ts.contains("dolor"));
  EXPECT_TRUE(ts.contains("amet"));
  EXPECT_FALSE(ts.contains("consectetur"));
  EXPECT_FALSE(ts.contains("adipisci"));
  EXPECT_FALSE(ts.contains("elit"));
}

// TODO

}  // namespace
