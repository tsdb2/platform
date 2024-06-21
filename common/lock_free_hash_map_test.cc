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
using ::tsdb2::common::lock_free_hash_map;

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

TEST(LockFreeHashMapTest, ConstructFromIterators) {
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
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, MaxLoad) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"},  {1, "b"},  {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"},  {9, "j"},  {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
      {16, "q"}, {17, "r"}, {18, "s"}, {19, "t"}, {20, "u"}, {21, "v"}, {22, "w"}, {23, "x"},
  };
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 24);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p"),
                                       Pair(16, "q"), Pair(17, "r"), Pair(18, "s"), Pair(19, "t"),
                                       Pair(20, "u"), Pair(21, "v"), Pair(22, "w"), Pair(23, "x")));
}

TEST(LockFreeHashMapTest, Grow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"},  {1, "b"},  {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"},  {9, "j"},  {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
      {16, "q"}, {17, "r"}, {18, "s"}, {19, "t"}, {20, "u"}, {21, "v"}, {22, "w"}, {23, "x"},
  };
  auto const [it, inserted] = hm.insert(std::make_pair(24, "y"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(24, "y"));
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 25);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(
                      Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                      Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                      Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"), Pair(14, "o"),
                      Pair(15, "p"), Pair(16, "q"), Pair(17, "r"), Pair(18, "s"), Pair(19, "t"),
                      Pair(20, "u"), Pair(21, "v"), Pair(22, "w"), Pair(23, "x"), Pair(24, "y")));
}

TEST(LockFreeHashMapTest, InsertAfterGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"},  {1, "b"},  {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"},  {9, "j"},  {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
      {16, "q"}, {17, "r"}, {18, "s"}, {19, "t"}, {20, "u"}, {21, "v"}, {22, "w"}, {23, "x"},
  };
  hm.insert(std::make_pair(24, "y"));
  auto const [it, inserted] = hm.insert(std::make_pair(25, "z"));
  EXPECT_TRUE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(25, "z"));
  EXPECT_EQ(hm.capacity(), 64);
  EXPECT_EQ(hm.size(), 26);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(
      hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"), Pair(4, "e"),
                               Pair(5, "f"), Pair(6, "g"), Pair(7, "h"), Pair(8, "i"), Pair(9, "j"),
                               Pair(10, "k"), Pair(11, "l"), Pair(12, "m"), Pair(13, "n"),
                               Pair(14, "o"), Pair(15, "p"), Pair(16, "q"), Pair(17, "r"),
                               Pair(18, "s"), Pair(19, "t"), Pair(20, "u"), Pair(21, "v"),
                               Pair(22, "w"), Pair(23, "x"), Pair(24, "y"), Pair(25, "z")));
}

TEST(LockFreeHashMapTest, InsertingTwiceDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"},  {1, "b"},  {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"},  {9, "j"},  {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
      {16, "q"}, {17, "r"}, {18, "s"}, {19, "t"}, {20, "u"}, {21, "v"}, {22, "w"}, {23, "x"},
  };
  auto const [it, inserted] = hm.insert(std::make_pair(23, "y"));
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(23, "x"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 24);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p"),
                                       Pair(16, "q"), Pair(17, "r"), Pair(18, "s"), Pair(19, "t"),
                                       Pair(20, "u"), Pair(21, "v"), Pair(22, "w"), Pair(23, "x")));
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
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(42, "lorem")));
  EXPECT_TRUE(hm.contains(42));
  EXPECT_FALSE(hm.contains(43));
}

TEST(LockFreeHashMapTest, EmplacingTwiceDoesntGrow) {
  lock_free_hash_map<int, std::string> hm{
      {0, "a"},  {1, "b"},  {2, "c"},  {3, "d"},  {4, "e"},  {5, "f"},  {6, "g"},  {7, "h"},
      {8, "i"},  {9, "j"},  {10, "k"}, {11, "l"}, {12, "m"}, {13, "n"}, {14, "o"}, {15, "p"},
      {16, "q"}, {17, "r"}, {18, "s"}, {19, "t"}, {20, "u"}, {21, "v"}, {22, "w"}, {23, "x"},
  };
  auto const [it, inserted] = hm.emplace(23, "y");
  EXPECT_FALSE(inserted);
  EXPECT_NE(it, hm.end());
  EXPECT_THAT(*it, Pair(23, "x"));
  EXPECT_EQ(hm.capacity(), 32);
  EXPECT_EQ(hm.size(), 24);
  EXPECT_FALSE(hm.empty());
  EXPECT_THAT(hm, UnorderedElementsAre(Pair(0, "a"), Pair(1, "b"), Pair(2, "c"), Pair(3, "d"),
                                       Pair(4, "e"), Pair(5, "f"), Pair(6, "g"), Pair(7, "h"),
                                       Pair(8, "i"), Pair(9, "j"), Pair(10, "k"), Pair(11, "l"),
                                       Pair(12, "m"), Pair(13, "n"), Pair(14, "o"), Pair(15, "p"),
                                       Pair(16, "q"), Pair(17, "r"), Pair(18, "s"), Pair(19, "t"),
                                       Pair(20, "u"), Pair(21, "v"), Pair(22, "w"), Pair(23, "x")));
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

// TODO

}  // namespace
