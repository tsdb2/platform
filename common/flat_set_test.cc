#include "common/flat_set.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <iosfwd>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "common/fingerprint.h"
#include "common/fixed.h"
#include "common/flat_container_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Ge;
using ::testing::Pair;
using ::testing::Property;
using ::tsdb2::common::FingerprintOf;
using ::tsdb2::common::fixed_flat_set_of;
using ::tsdb2::common::flat_set;
using ::tsdb2::testing::OtherTestKey;
using ::tsdb2::testing::ReverseTestCompare;
using ::tsdb2::testing::TestAllocator;
using ::tsdb2::testing::TestCompare;
using ::tsdb2::testing::TestKey;
using ::tsdb2::testing::TestKeyEq;
using ::tsdb2::testing::TestRepresentation;
using ::tsdb2::testing::TransparentTestCompare;

template <typename FlatSet, typename... Inner>
class TestKeysMatcher;

template <typename Key, typename Compare, typename Representation, typename... Inner>
class TestKeysMatcher<flat_set<Key, Compare, Representation>, Inner...>
    : public ::testing::MatcherInterface<flat_set<Key, Compare, Representation> const&> {
 public:
  using is_gtest_matcher = void;

  using FlatSet = flat_set<Key, Compare, Representation>;
  using Tuple = std::tuple<tsdb2::common::FixedT<TestKey, Inner>...>;

  explicit TestKeysMatcher(Inner&&... inner) : inner_{std::forward<Inner>(inner)...} {}
  ~TestKeysMatcher() override = default;

  TestKeysMatcher(TestKeysMatcher const&) = default;
  TestKeysMatcher& operator=(TestKeysMatcher const&) = default;
  TestKeysMatcher(TestKeysMatcher&&) noexcept = default;
  TestKeysMatcher& operator=(TestKeysMatcher&&) noexcept = default;

  bool MatchAndExplain(FlatSet const& value,
                       ::testing::MatchResultListener* const listener) const override {
    auto it = value.begin();
    Tuple values{TestKey(tsdb2::common::FixedV<Inner>(*it++))...};
    return ::testing::MatcherCast<Tuple>(inner_).MatchAndExplain(values, listener);
  }

  void DescribeTo(std::ostream* const os) const override {
    ::testing::MatcherCast<Tuple>(inner_).DescribeTo(os);
  }

 private:
  std::tuple<Inner...> inner_;
};

template <typename Representation, typename... Inner>
auto TestKeysAre(Inner&&... inner) {
  return TestKeysMatcher<flat_set<TestKey, TestCompare, Representation>, Inner...>(
      std::forward<Inner>(inner)...);
}

template <typename Representation, typename... Inner>
auto TransparentTestKeysAre(Inner&&... inner) {
  return TestKeysMatcher<flat_set<TestKey, TransparentTestCompare, Representation>, Inner...>(
      std::forward<Inner>(inner)...);
}

TEST(FlatSetTest, Traits) {
  using flat_set = flat_set<TestKey, TestCompare, TestRepresentation>;
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::node::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::representation_type, std::deque<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reference, TestKey&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reference, TestKey const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::pointer, TestKey*>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_pointer, TestKey const*>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::iterator, typename TestRepresentation::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_iterator,
                              typename TestRepresentation::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reverse_iterator,
                              typename TestRepresentation::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reverse_iterator,
                              typename TestRepresentation::const_reverse_iterator>));
}

TEST(FlatSetTest, DefaultRepresentation) {
  using flat_set = flat_set<TestKey, TestCompare>;
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::node::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::representation_type, std::vector<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reference, TestKey&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reference, TestKey const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::pointer, TestKey*>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_pointer, TestKey const*>));
  EXPECT_TRUE(
      (std::is_same_v<typename flat_set::iterator, typename std::vector<TestKey>::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_iterator,
                              typename std::vector<TestKey>::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reverse_iterator,
                              typename std::vector<TestKey>::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reverse_iterator,
                              typename std::vector<TestKey>::const_reverse_iterator>));
}

TEST(FlatSetTest, DefaultComparator) {
  using flat_set = flat_set<TestKey>;
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::node::value_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::representation_type, std::vector<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::key_compare, std::less<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::value_compare, std::less<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reference, TestKey&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reference, TestKey const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::pointer, TestKey*>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_pointer, TestKey const*>));
  EXPECT_TRUE(
      (std::is_same_v<typename flat_set::iterator, typename std::vector<TestKey>::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_iterator,
                              typename std::vector<TestKey>::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::reverse_iterator,
                              typename std::vector<TestKey>::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_set::const_reverse_iterator,
                              typename std::vector<TestKey>::const_reverse_iterator>));
}

template <typename Representation>
class FlatSetWithRepresentationTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(FlatSetWithRepresentationTest);

TYPED_TEST_P(FlatSetWithRepresentationTest, Construct) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{TestCompare()};
  EXPECT_THAT(fs1, TestKeysAre<TypeParam>());
  flat_set<TestKey, TestCompare, TypeParam> fs2{};
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>());
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstructWithIterators) {
  std::vector<TestKey> keys{-2, -3, 4, -1, -2, 1, 5, -3};

  flat_set<TestKey, TestCompare, TypeParam> fs1{keys.begin(), keys.end(), TestCompare()};
  EXPECT_THAT(fs1, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));

  flat_set<TestKey, TestCompare, TypeParam> fs2{keys.begin(), keys.end()};
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstructWithInitializerList) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, AssignInitializerList) {
  flat_set<TestKey, TestCompare, TypeParam> fs{1, 2, 3};
  fs = {-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Deduplication) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-3, -2, -1, 1, 4, 5};
  EXPECT_EQ(fs1, fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, GetAllocator) {
  flat_set<TestKey, TestCompare, TypeParam> fs;
  EXPECT_TRUE((std::is_same_v<typename TypeParam::allocator_type, decltype(fs.get_allocator())>));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, CompareEqual) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_TRUE(fs1 == fs2);
  EXPECT_FALSE(fs1 != fs2);
  EXPECT_FALSE(fs1 < fs2);
  EXPECT_TRUE(fs1 <= fs2);
  EXPECT_FALSE(fs1 > fs2);
  EXPECT_TRUE(fs1 >= fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, CompareLHSLessThanRHS) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-3, 4, -1, 1, 5, -3};
  EXPECT_FALSE(fs1 == fs2);
  EXPECT_TRUE(fs1 != fs2);
  EXPECT_TRUE(fs1 < fs2);
  EXPECT_TRUE(fs1 <= fs2);
  EXPECT_FALSE(fs1 > fs2);
  EXPECT_FALSE(fs1 >= fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, CompareLHSGreaterThanRHS) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-3, 4, -1, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_FALSE(fs1 == fs2);
  EXPECT_TRUE(fs1 != fs2);
  EXPECT_FALSE(fs1 < fs2);
  EXPECT_FALSE(fs1 <= fs2);
  EXPECT_TRUE(fs1 > fs2);
  EXPECT_TRUE(fs1 >= fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ReverseCompareLHSLessThanRHS) {
  flat_set<TestKey, ReverseTestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, ReverseTestCompare, TypeParam> fs2{-3, 4, -1, 1, 5, -3};
  EXPECT_FALSE(fs1 == fs2);
  EXPECT_TRUE(fs1 != fs2);
  EXPECT_FALSE(fs1 < fs2);
  EXPECT_FALSE(fs1 <= fs2);
  EXPECT_TRUE(fs1 > fs2);
  EXPECT_TRUE(fs1 >= fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ReverseCompareLHSGreaterThanRHS) {
  flat_set<TestKey, ReverseTestCompare, TypeParam> fs1{-3, 4, -1, 1, 5, -3};
  flat_set<TestKey, ReverseTestCompare, TypeParam> fs2{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_FALSE(fs1 == fs2);
  EXPECT_TRUE(fs1 != fs2);
  EXPECT_TRUE(fs1 < fs2);
  EXPECT_TRUE(fs1 <= fs2);
  EXPECT_FALSE(fs1 > fs2);
  EXPECT_FALSE(fs1 >= fs2);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, CopyConstruct) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{fs1};
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Copy) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2;
  fs2 = fs1;
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, MoveConstruct) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{std::move(fs1)};
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Move) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2;
  fs2 = std::move(fs1);
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Empty) {
  flat_set<TestKey, TestCompare, TypeParam> fs;
  ASSERT_THAT(fs, TestKeysAre<TypeParam>());
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(fs.size(), 0);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, NotEmpty) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_FALSE(fs.empty());
  EXPECT_EQ(fs.size(), 6);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Hash) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-2, -3, 4, -1, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs3{-3, 4, -1, 1, 5, -3};
  EXPECT_EQ(absl::HashOf(fs1), absl::HashOf(fs2));
  EXPECT_NE(absl::HashOf(fs1), absl::HashOf(fs3));
};

TYPED_TEST_P(FlatSetWithRepresentationTest, Fingerprint) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{-2, -3, 4, -1, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs3{-3, 4, -1, 1, 5, -3};
  EXPECT_EQ(FingerprintOf(fs1), FingerprintOf(fs2));
  EXPECT_NE(FingerprintOf(fs1), FingerprintOf(fs3));
};

TYPED_TEST_P(FlatSetWithRepresentationTest, Clear) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  fs.clear();
  EXPECT_THAT(fs, TestKeysAre<TypeParam>());
  EXPECT_TRUE(fs.empty());
  EXPECT_EQ(fs.size(), 0);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Insert) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  TestKey const value{6};
  auto const [it, inserted] = fs.insert(value);
  EXPECT_EQ(value, *it);
  EXPECT_TRUE(inserted);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5, 6));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, MoveInsert) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  TestKey value{6};
  auto const [it, inserted] = fs.insert(std::move(value));  // NOLINT(performance-move-const-arg)
  EXPECT_EQ(it->field, 6);
  EXPECT_TRUE(inserted);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5, 6));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertCollision) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  TestKey const value{5};
  auto const [it, inserted] = fs.insert(value);
  EXPECT_EQ(value, *it);
  EXPECT_FALSE(inserted);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, MoveInsertCollision) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  TestKey value{5};
  auto const [it, inserted] = fs.insert(std::move(value));  // NOLINT(performance-move-const-arg)
  EXPECT_EQ(it->field, 5);
  EXPECT_FALSE(inserted);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertFromIterators) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1};
  std::vector<TestKey> v{-2, 1, 5, -3};
  fs.insert(v.begin(), v.end());
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertFromInitializerList) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1};
  fs.insert({-2, 1, 5, -3});
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertNode) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto node1 = fs.extract(TestKey{1});
  auto const [it, inserted, node2] = fs.insert(std::move(node1));
  EXPECT_EQ(it->field, 1);
  EXPECT_TRUE(inserted);
  EXPECT_TRUE(node2.empty());
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertNodeCollision) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto node1 = fs.extract(TestKey{1});
  fs.insert(1);
  auto const [it, inserted, node2] = fs.insert(std::move(node1));
  EXPECT_EQ(it->field, 1);
  EXPECT_FALSE(inserted);
  EXPECT_FALSE(node2.empty());
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, InsertEmptyNode) {
  using flat_set = flat_set<TestKey, TestCompare, TypeParam>;
  flat_set fs{-2, -3, 4, -1, -2, 1, 5, -3};
  typename flat_set::node_type node1;
  EXPECT_TRUE(node1.empty());
  EXPECT_TRUE(!node1);
  auto const [it, inserted, node2] = fs.insert(std::move(node1));
  EXPECT_EQ(it, fs.end());
  EXPECT_FALSE(inserted);
  EXPECT_TRUE(node2.empty());
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Emplace) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const [it, inserted] = fs.emplace(6);
  EXPECT_EQ(it->field, 6);
  EXPECT_TRUE(inserted);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EmplaceCollision) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const [it, inserted] = fs.emplace(4);
  EXPECT_EQ(it->field, 4);
  EXPECT_FALSE(inserted);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EraseIterator) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.erase(fs.begin() + 2);
  EXPECT_EQ(it->field, 1);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EraseRange) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.erase(fs.begin() + 1, fs.begin() + 3);
  EXPECT_EQ(it->field, 1);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EraseKey) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_EQ(fs.erase(TestKey(1)), 1);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EraseNotFound) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_EQ(fs.erase(TestKey(7)), 0);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EraseKeyTransparent) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_EQ(fs.erase(OtherTestKey{1}), 1);
  EXPECT_THAT(fs, TransparentTestKeysAre<TypeParam>(-3, -2, -1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Swap) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{2, 3, -4, 1, 2, -1, -5, 3};
  fs1.swap(fs2);
  EXPECT_THAT(fs1, TestKeysAre<TypeParam>(-5, -4, -1, 1, 2, 3));
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, AdlSwap) {
  flat_set<TestKey, TestCompare, TypeParam> fs1{-2, -3, 4, -1, -2, 1, 5, -3};
  flat_set<TestKey, TestCompare, TypeParam> fs2{2, 3, -4, 1, 2, -1, -5, 3};
  swap(fs1, fs2);
  EXPECT_THAT(fs1, TestKeysAre<TypeParam>(-5, -4, -1, 1, 2, 3));
  EXPECT_THAT(fs2, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ExtractIterator) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const node = fs.extract(fs.begin() + 2);
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.value(), -1);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ExtractKey) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const node = fs.extract(TestKey(-1));
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.value(), -1);
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ExtractMissing) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const node = fs.extract(TestKey(7));
  EXPECT_TRUE(node.empty());
  EXPECT_FALSE(node.operator bool());
  EXPECT_THAT(fs, TestKeysAre<TypeParam>(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ExtractKeyTransparent) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const node = fs.extract(OtherTestKey{-1});
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.value(), -1);
  EXPECT_THAT(fs, TransparentTestKeysAre<TypeParam>(-3, -2, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Representation) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const& rep = fs.rep();
  EXPECT_THAT(rep, ElementsAre(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ExtractRep) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const rep = std::move(fs).ExtractRep();
  EXPECT_THAT(rep, ElementsAre(-3, -2, -1, 1, 4, 5));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Count) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_EQ(fs.count(-2), 1);
  EXPECT_EQ(fs.count(6), 0);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Find) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(4));
  EXPECT_EQ(it->field, 4);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, FindTransparent) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(OtherTestKey{4});
  EXPECT_EQ(it->field, 4);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, FindMissing) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(7));
  EXPECT_EQ(it, fs.end());
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstFind) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(4));
  EXPECT_EQ(it->field, 4);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstFindTransparent) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(OtherTestKey{4});
  EXPECT_EQ(it->field, 4);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstFindMissing) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(7));
  EXPECT_EQ(it, fs.end());
}

TYPED_TEST_P(FlatSetWithRepresentationTest, Contains) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_TRUE(fs.contains(TestKey(4)));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ContainsTransparent) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_TRUE(fs.contains(OtherTestKey{4}));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ContainsMissing) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  EXPECT_FALSE(fs.contains(TestKey(7)));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, EqualRange) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(1));
  EXPECT_THAT(fs.equal_range(TestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, TransparentEqualRange) {
  flat_set<TestKey, TransparentTestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(1));
  EXPECT_THAT(fs.equal_range(OtherTestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstEqualRange) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.find(TestKey(1));
  EXPECT_THAT(fs.equal_range(TestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatSetWithRepresentationTest, LowerBoundExclusive) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.lower_bound(TestKey(0));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstLowerBoundExclusive) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.lower_bound(TestKey(0));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, LowerBoundInclusive) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.lower_bound(TestKey(1));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstLowerBoundInclusive) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.lower_bound(TestKey(1));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, UpperBoundExclusive) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.upper_bound(TestKey(0));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstUpperBoundExclusive) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.upper_bound(TestKey(0));
  EXPECT_EQ(it->field, 1);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, UpperBoundInclusive) {
  flat_set<TestKey, TestCompare, TypeParam> fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.upper_bound(TestKey(1));
  EXPECT_EQ(it->field, 4);
}

TYPED_TEST_P(FlatSetWithRepresentationTest, ConstUpperBoundInclusive) {
  flat_set<TestKey, TestCompare, TypeParam> const fs{-2, -3, 4, -1, -2, 1, 5, -3};
  auto const it = fs.upper_bound(TestKey(1));
  EXPECT_EQ(it->field, 4);
}

REGISTER_TYPED_TEST_SUITE_P(
    FlatSetWithRepresentationTest, Construct, ConstructWithIterators, ConstructWithInitializerList,
    AssignInitializerList, Deduplication, GetAllocator, CompareEqual, CompareLHSLessThanRHS,
    CompareLHSGreaterThanRHS, ReverseCompareLHSLessThanRHS, ReverseCompareLHSGreaterThanRHS,
    CopyConstruct, Copy, MoveConstruct, Move, Empty, NotEmpty, Hash, Fingerprint, Clear, Insert,
    MoveInsert, InsertCollision, MoveInsertCollision, InsertFromIterators,
    InsertFromInitializerList, InsertNode, InsertNodeCollision, InsertEmptyNode, Emplace,
    EmplaceCollision, EraseIterator, EraseRange, EraseKey, EraseNotFound, EraseKeyTransparent, Swap,
    AdlSwap, ExtractIterator, ExtractKey, ExtractMissing, ExtractKeyTransparent, Representation,
    ExtractRep, Count, Find, FindTransparent, FindMissing, ConstFind, ConstFindTransparent,
    ConstFindMissing, Contains, ContainsTransparent, ContainsMissing, EqualRange,
    TransparentEqualRange, ConstEqualRange, LowerBoundExclusive, ConstLowerBoundExclusive,
    LowerBoundInclusive, ConstLowerBoundInclusive, UpperBoundExclusive, ConstUpperBoundExclusive,
    UpperBoundInclusive, ConstUpperBoundInclusive);

using RepresentationTypes =
    ::testing::Types<std::vector<TestKey>, std::deque<TestKey>, absl::InlinedVector<TestKey, 1>,
                     absl::InlinedVector<TestKey, 10>,
                     std::vector<TestKey, TestAllocator<TestKey>>>;
INSTANTIATE_TYPED_TEST_SUITE_P(FlatSetWithRepresentationTest, FlatSetWithRepresentationTest,
                               RepresentationTypes);

TEST(FlatSetWithAllocatorTest, DefaultAllocator) {
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs;
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator<int>::tag, 0));
}

TEST(FlatSetWithAllocatorTest, CustomAllocator) {
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs{alloc};
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetWithAllocatorTest, CustomComparatorAndAllocator) {
  std::less<int> comp;
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs{comp, alloc};
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetWithAllocatorTest, IteratorsAndAllocator) {
  std::vector<int> v{1, 2, 3};
  std::less<int> comp;
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs{v.begin(), v.end(), comp,
                                                                         alloc};
  EXPECT_THAT(fs, ElementsAre(1, 2, 3));
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetWithAllocatorTest, InitializerListAndAllocator) {
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs{{1, 2, 3}, alloc};
  EXPECT_THAT(fs, ElementsAre(1, 2, 3));
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetWithAllocatorTest, CopyAllocator) {
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs1{alloc};
  auto fs2 = fs1;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_THAT(fs2.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetWithAllocatorTest, MoveAllocator) {
  TestAllocator<int> alloc{42};
  flat_set<int, std::less<int>, std::vector<int, TestAllocator<int>>> fs1{alloc};
  auto fs2 = std::move(fs1);
  EXPECT_THAT(fs2.get_allocator(), Property(&TestAllocator<int>::tag, 42));
}

TEST(FlatSetCapacityTest, InitialCapacity) {
  flat_set<int> fs;
  EXPECT_EQ(fs.capacity(), 0);
  EXPECT_EQ(fs.size(), 0);
}

TEST(FlatSetCapacityTest, CapacityAfterInsert) {
  flat_set<int> fs;
  fs.insert(2);
  fs.insert(3);
  fs.insert(1);
  EXPECT_THAT(fs.capacity(), Ge(3));
  EXPECT_EQ(fs.size(), 3);
}

TEST(FlatSetCapacityTest, Reserve) {
  flat_set<int> fs;
  fs.reserve(3);
  EXPECT_EQ(fs.capacity(), 3);
  EXPECT_EQ(fs.size(), 0);
}

TEST(FlatSetCapacityTest, ReserveAndInsert) {
  flat_set<int> fs;
  fs.reserve(3);
  fs.insert(2);
  fs.insert(3);
  fs.insert(1);
  EXPECT_EQ(fs.capacity(), 3);
  EXPECT_THAT(fs, ElementsAre(1, 2, 3));
}

TEST(FlatSetCapacityTest, InsertMoreThanReserved) {
  flat_set<int> fs;
  fs.reserve(3);
  fs.insert(2);
  fs.insert(3);
  fs.insert(1);
  fs.insert(5);
  fs.insert(4);
  EXPECT_THAT(fs.capacity(), Ge(5));
  EXPECT_THAT(fs, ElementsAre(1, 2, 3, 4, 5));
}

TEST(FixedFlatSetTest, Empty) {
  auto constexpr fs = fixed_flat_set_of<int>({});
  EXPECT_TRUE(fs.empty());
}

TEST(FixedFlatSetTest, ConstructInts) {
  auto constexpr fs = fixed_flat_set_of({1, 2, 3});
  EXPECT_THAT(fs, ElementsAre(1, 2, 3));
}

TEST(FixedFlatSetTest, ConstructStrings) {
  auto constexpr fs = fixed_flat_set_of<std::string_view>({"a", "b", "c"});
  EXPECT_THAT(fs, ElementsAre("a", "b", "c"));
}

TEST(FixedFlatSetTest, SortInts) {
  auto constexpr fs = fixed_flat_set_of({1, 3, 2});
  EXPECT_THAT(fs, ElementsAre(1, 2, 3));
}

TEST(FixedFlatSetTest, SortIntsReverse) {
  auto constexpr fs = fixed_flat_set_of<int, std::greater<int>>({1, 3, 2});
  EXPECT_THAT(fs, ElementsAre(3, 2, 1));
}

TEST(FixedFlatSetTest, SortStringsReverse) {
  auto constexpr fs =
      fixed_flat_set_of<std::string_view, std::greater<std::string_view>>({"b", "a", "c"});
  EXPECT_THAT(fs, ElementsAre("c", "b", "a"));
}

TEST(FixedFlatSetDeathTest, ConstructIntsWithDuplicates) {
  EXPECT_DEATH(fixed_flat_set_of({1, 2, 1, 3}), _);
}

TEST(FixedFlatSetDeathTest, ConstructStringsWithDuplicates) {
  EXPECT_DEATH(fixed_flat_set_of({"a", "b", "a", "c"}), _);
}

}  // namespace
