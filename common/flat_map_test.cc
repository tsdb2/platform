#include "common/flat_map.h"

#include <array>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "common/fingerprint.h"
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
using ::tsdb2::common::fixed_flat_map_of;
using ::tsdb2::common::flat_map;
using ::tsdb2::testing::OtherTestKey;
using ::tsdb2::testing::ReverseTestCompare;
using ::tsdb2::testing::TestCompare;
using ::tsdb2::testing::TestKey;
using ::tsdb2::testing::TestValue;
using ::tsdb2::testing::TestValueRepresentation;
using ::tsdb2::testing::TransparentTestCompare;

using TestAllocator = tsdb2::testing::TestAllocator<std::pair<int, int>>;

template <typename FlatMap, typename... Inner>
class TestValuesMatcher;

template <typename Key, typename Value, typename Compare, typename Representation>
class TestValuesMatcher<flat_map<Key, Value, Compare, Representation>>
    : public ::testing::MatcherInterface<flat_map<Key, Value, Compare, Representation> const&> {
 public:
  using is_gtest_matcher = void;

  using FlatMap = flat_map<Key, Value, Compare, Representation>;

  explicit TestValuesMatcher() = default;
  ~TestValuesMatcher() override = default;

  TestValuesMatcher(TestValuesMatcher const&) = default;
  TestValuesMatcher& operator=(TestValuesMatcher const&) = default;
  TestValuesMatcher(TestValuesMatcher&&) noexcept = default;
  TestValuesMatcher& operator=(TestValuesMatcher&&) noexcept = default;

  bool MatchAndExplain(FlatMap const& value,
                       ::testing::MatchResultListener* const listener) const override {
    return MatchAndExplainInternal(value, value.begin(), listener);
  }

  void DescribeTo(std::ostream* const os) const override { *os << "is an empty flat_map"; }

 protected:
  virtual bool MatchAndExplainInternal(FlatMap const& value,
                                       typename FlatMap::const_iterator const it,
                                       ::testing::MatchResultListener* const listener) const {
    return it == value.end();
  }

  virtual void DescribeToInternal(std::ostream* const os) const {}
};

template <typename Key, typename Value, typename Compare, typename Representation,
          typename KeyMatcher, typename ValueMatcher, typename... Rest>
class TestValuesMatcher<flat_map<Key, Value, Compare, Representation>, KeyMatcher, ValueMatcher,
                        Rest...>
    : public TestValuesMatcher<flat_map<Key, Value, Compare, Representation>, Rest...> {
 public:
  using is_gtest_matcher = void;

  using FlatMap = flat_map<Key, Value, Compare, Representation>;
  using Base = TestValuesMatcher<FlatMap, Rest...>;

  explicit TestValuesMatcher(KeyMatcher&& key, ValueMatcher&& value, Rest&&... rest)
      : Base(std::forward<Rest>(rest)...),
        key_matcher_(std::move(key)),
        value_matcher_(std::move(value)) {}

  ~TestValuesMatcher() override = default;

  TestValuesMatcher(TestValuesMatcher const&) = default;
  TestValuesMatcher& operator=(TestValuesMatcher const&) = default;
  TestValuesMatcher(TestValuesMatcher&&) noexcept = default;
  TestValuesMatcher& operator=(TestValuesMatcher&&) noexcept = default;

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a flat_map with:";
    DescribeToInternal(os);
  }

 protected:
  bool MatchAndExplainInternal(FlatMap const& value, typename FlatMap::const_iterator it,
                               ::testing::MatchResultListener* const listener) const override {
    return ::testing::MatcherCast<Key>(key_matcher_).MatchAndExplain(it->first, listener) &&
           ::testing::MatcherCast<Value>(value_matcher_).MatchAndExplain(it->second, listener) &&
           Base::MatchAndExplainInternal(value, ++it, listener);
  }

  void DescribeToInternal(std::ostream* const os) const override {
    *os << " {";
    ::testing::MatcherCast<Key>(key_matcher_).DescribeTo(os);
    *os << ", ";
    ::testing::MatcherCast<Value>(value_matcher_).DescribeTo(os);
    *os << "},";
    Base::DescribeToInternal(os);
  }

 private:
  KeyMatcher key_matcher_;
  ValueMatcher value_matcher_;
};

template <typename Representation, typename... Inner>
auto TestValuesAre(Inner&&... inner) {
  return TestValuesMatcher<flat_map<TestKey, std::string, TestCompare, Representation>, Inner...>(
      std::forward<Inner>(inner)...);
}

template <typename Representation, typename... Inner>
auto TransparentTestValuesAre(Inner&&... inner) {
  return TestValuesMatcher<flat_map<TestKey, std::string, TransparentTestCompare, Representation>,
                           Inner...>(std::forward<Inner>(inner)...);
}

TEST(FlatMapTest, Traits) {
  using flat_map = flat_map<TestKey, std::string, TestCompare, TestValueRepresentation>;
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::value_type, TestValue>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::representation_type, TestValueRepresentation>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reference, TestValue&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reference, TestValue const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::pointer, TestValue*>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_pointer, TestValue const*>));
  EXPECT_TRUE(
      (std::is_same_v<typename flat_map::iterator, typename TestValueRepresentation::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_iterator,
                              typename TestValueRepresentation::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reverse_iterator,
                              typename TestValueRepresentation::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reverse_iterator,
                              typename TestValueRepresentation::const_reverse_iterator>));
}

TEST(FlatMapTest, DefaultRepresentation) {
  using flat_map = flat_map<TestKey, std::string, TestCompare>;
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::value_type, TestValue>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::representation_type, std::vector<TestValue>>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_compare, TestCompare>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reference, TestValue&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reference, TestValue const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::pointer, TestValue*>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_pointer, TestValue const*>));
  EXPECT_TRUE(
      (std::is_same_v<typename flat_map::iterator, typename std::vector<TestValue>::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_iterator,
                              typename std::vector<TestValue>::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reverse_iterator,
                              typename std::vector<TestValue>::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reverse_iterator,
                              typename std::vector<TestValue>::const_reverse_iterator>));
}

TEST(FlatMapTest, DefaultComparator) {
  using flat_map = flat_map<TestKey, std::string>;
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::key_type, TestKey>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::node_type::mapped_type, std::string>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::value_type, TestValue>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::representation_type, std::vector<TestValue>>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::size_type, size_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::difference_type, ptrdiff_t>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::key_compare, std::less<TestKey>>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reference, TestValue&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reference, TestValue const&>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::pointer, TestValue*>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_pointer, TestValue const*>));
  EXPECT_TRUE(
      (std::is_same_v<typename flat_map::iterator, typename std::vector<TestValue>::iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_iterator,
                              typename std::vector<TestValue>::const_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::reverse_iterator,
                              typename std::vector<TestValue>::reverse_iterator>));
  EXPECT_TRUE((std::is_same_v<typename flat_map::const_reverse_iterator,
                              typename std::vector<TestValue>::const_reverse_iterator>));
}

template <typename Representation>
class FlatMapWithRepresentationTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(FlatMapWithRepresentationTest);

TYPED_TEST_P(FlatMapWithRepresentationTest, Construct) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{TestCompare()};
  EXPECT_THAT(fm1, TestValuesAre<TypeParam>());
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2;
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>());
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstructWithIterators) {
  std::vector<TestValue> keys{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };

  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{keys.begin(), keys.end(),
                                                             TestCompare()};
  EXPECT_THAT(fm1, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));

  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{keys.begin(), keys.end()};
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstructWithInitializerList) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, AssignInitializerList) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
  };
  fm = {
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Deduplication) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-2, "lorem"}, {-3, "ipsum"}, {4, "dolor"}, {-1, "sit"}, {1, "consectetur"}, {5, "adipisci"},
  };
  EXPECT_EQ(fm1, fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, GetAllocator) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm;
  EXPECT_TRUE((std::is_same_v<typename TypeParam::allocator_type, decltype(fm.get_allocator())>));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, CompareEqual) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_TRUE(fm1 == fm2);
  EXPECT_FALSE(fm1 != fm2);
  EXPECT_FALSE(fm1 < fm2);
  EXPECT_TRUE(fm1 <= fm2);
  EXPECT_FALSE(fm1 > fm2);
  EXPECT_TRUE(fm1 >= fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, CompareLHSLessThanRHS) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-3, "lorem"}, {4, "ipsum"}, {-1, "dolor"}, {1, "amet"}, {5, "consectetur"}, {-3, "adipisci"},
  };
  EXPECT_FALSE(fm1 == fm2);
  EXPECT_TRUE(fm1 != fm2);
  EXPECT_TRUE(fm1 < fm2);
  EXPECT_TRUE(fm1 <= fm2);
  EXPECT_FALSE(fm1 > fm2);
  EXPECT_FALSE(fm1 >= fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, CompareLHSGreaterThanRHS) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-3, "lorem"}, {4, "ipsum"}, {-1, "dolor"}, {1, "amet"}, {5, "consectetur"}, {-3, "adipisci"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_FALSE(fm1 == fm2);
  EXPECT_TRUE(fm1 != fm2);
  EXPECT_FALSE(fm1 < fm2);
  EXPECT_FALSE(fm1 <= fm2);
  EXPECT_TRUE(fm1 > fm2);
  EXPECT_TRUE(fm1 >= fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ReverseCompareLHSLessThanRHS) {
  flat_map<TestKey, std::string, ReverseTestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, ReverseTestCompare, TypeParam> fm2{
      {-3, "lorem"}, {4, "ipsum"}, {-1, "dolor"}, {1, "amet"}, {5, "consectetur"}, {-3, "adipisci"},
  };
  EXPECT_FALSE(fm1 == fm2);
  EXPECT_TRUE(fm1 != fm2);
  EXPECT_TRUE(fm1 < fm2);
  EXPECT_TRUE(fm1 <= fm2);
  EXPECT_FALSE(fm1 > fm2);
  EXPECT_FALSE(fm1 >= fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ReverseCompareLHSGreaterThanRHS) {
  flat_map<TestKey, std::string, ReverseTestCompare, TypeParam> fm1{
      {-3, "lorem"}, {4, "ipsum"}, {-1, "dolor"}, {1, "amet"}, {5, "consectetur"}, {-3, "adipisci"},
  };
  flat_map<TestKey, std::string, ReverseTestCompare, TypeParam> fm2{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_FALSE(fm1 == fm2);
  EXPECT_TRUE(fm1 != fm2);
  EXPECT_FALSE(fm1 < fm2);
  EXPECT_FALSE(fm1 <= fm2);
  EXPECT_TRUE(fm1 > fm2);
  EXPECT_TRUE(fm1 >= fm2);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, At) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.at(-2), "lorem");
  EXPECT_EQ(fm.at(4), "dolor");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstAt) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.at(-2), "lorem");
  EXPECT_EQ(fm.at(4), "dolor");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, AtMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_DEATH(fm.at(7), _);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstAtMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_DEATH(fm.at(7), _);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Subscript) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm[-2], "lorem");
  EXPECT_EQ(fm[4], "dolor");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, SubscriptMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm[7], "");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, AssignSubscript) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  fm[7] = "foobar";
  EXPECT_EQ(fm[7], "foobar");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, UpdateSubscript) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  fm[7] = "foobar";
  fm[7] = "barfoo";
  EXPECT_EQ(fm[7], "barfoo");
}

TYPED_TEST_P(FlatMapWithRepresentationTest, CopyConstruct) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{fm1};
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Copy) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2;
  fm2 = fm1;
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, MoveConstruct) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{std::move(fm1)};
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Move) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2;
  fm2 = std::move(fm1);
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Empty) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm;
  ASSERT_THAT(fm, TestValuesAre<TypeParam>());
  EXPECT_TRUE(fm.empty());
  EXPECT_EQ(fm.size(), 0);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, NotEmpty) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_FALSE(fm.empty());
  EXPECT_EQ(fm.size(), 6);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Hash) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-2, "lorem"},      {-3, "ipsum"},   {4, "dolor"}, {-1, "sit"},
      {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm3{
      {-3, "ipsum"}, {4, "dolor"}, {-1, "sit"}, {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(absl::HashOf(fm1), absl::HashOf(fm2));
  EXPECT_NE(absl::HashOf(fm1), absl::HashOf(fm3));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Fingerprint) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {-2, "lorem"},      {-3, "ipsum"},   {4, "dolor"}, {-1, "sit"},
      {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm3{
      {-3, "ipsum"}, {4, "dolor"}, {-1, "sit"}, {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(FingerprintOf(fm1), FingerprintOf(fm2));
  EXPECT_NE(FingerprintOf(fm1), FingerprintOf(fm3));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Clear) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  fm.clear();
  EXPECT_THAT(fm, TestValuesAre<TypeParam>());
  EXPECT_TRUE(fm.empty());
  EXPECT_EQ(fm.size(), 0);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Insert) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestValue const value{6, "foobar"};
  auto const [it, inserted] = fm.insert(value);
  EXPECT_EQ(value, *it);
  EXPECT_TRUE(inserted);
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci", 6, "foobar"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, MoveInsert) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestValue value{6, "foobar"};
  auto const [it, inserted] = fm.insert(std::move(value));
  EXPECT_THAT(*it, Pair(6, "foobar"));
  EXPECT_TRUE(inserted);
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci", 6, "foobar"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestValue const value{5, "foobar"};
  auto const [it, inserted] = fm.insert(value);
  EXPECT_THAT(*it, Pair(5, "adipisci"));
  EXPECT_FALSE(inserted);
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, MoveInsertCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestValue value{5, "foobar"};
  auto const [it, inserted] = fm.insert(std::move(value));
  EXPECT_THAT(*it, Pair(5, "adipisci"));
  EXPECT_FALSE(inserted);
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertFromIterators) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"}, {4, "dolor"}, {-1, "sit"}};
  std::vector<TestValue> v{{-2, "amet"}, {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"}};
  fm.insert(v.begin(), v.end());
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertFromInitializerList) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"}, {4, "dolor"}, {-1, "sit"}};
  fm.insert({{-2, "amet"}, {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"}});
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertNode) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto node1 = fm.extract(TestKey{1});
  auto const [it, inserted, node2] = fm.insert(std::move(node1));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
  EXPECT_TRUE(inserted);
  EXPECT_TRUE(node2.empty());
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertNodeCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto node1 = fm.extract(TestKey{1});
  fm[1] = "foobar";
  auto const [it, inserted, node2] = fm.insert(std::move(node1));
  EXPECT_THAT(*it, Pair(1, "foobar"));
  EXPECT_FALSE(inserted);
  EXPECT_FALSE(node2.empty());
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "foobar", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertEmptyNode) {
  using flat_map = flat_map<TestKey, std::string, TestCompare, TypeParam>;
  flat_map fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  typename flat_map::node_type node1;
  EXPECT_TRUE(node1.empty());
  EXPECT_TRUE(!node1);
  auto const [it, inserted, node2] = fm.insert(std::move(node1));
  EXPECT_EQ(it, fm.end());
  EXPECT_FALSE(inserted);
  EXPECT_TRUE(node2.empty());
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, InsertAndNotAssign) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestKey const key{7};
  auto const [it, inserted] = fm.insert_or_assign(key, "foobar");
  EXPECT_THAT(*it, Pair(7, "foobar"));
  EXPECT_TRUE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, MoveInsertAndNotAssign) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.insert_or_assign(TestKey{7}, "foobar");
  EXPECT_THAT(*it, Pair(7, "foobar"));
  EXPECT_TRUE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, AssignAndNotInsert) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  TestKey const key{1};
  auto const [it, inserted] = fm.insert_or_assign(key, "foobar");
  EXPECT_THAT(*it, Pair(1, "foobar"));
  EXPECT_FALSE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, MoveAssignAndNotInsert) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.insert_or_assign(TestKey{1}, "foobar");
  EXPECT_THAT(*it, Pair(1, "foobar"));
  EXPECT_FALSE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Emplace) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.emplace(TestKey{6}, "foobar");
  EXPECT_THAT(*it, Pair(6, "foobar"));
  EXPECT_TRUE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EmplaceCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.emplace(TestKey{4}, "foobar");
  EXPECT_THAT(*it, Pair(4, "dolor"));
  EXPECT_FALSE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EmplaceHint) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  // Our emplace_hint implementation flat out ignores the hint.
  auto const it = fm.emplace_hint(fm.begin(), TestKey{6}, "foobar");
  EXPECT_THAT(*it, Pair(6, "foobar"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EmplaceHintCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  // Our emplace_hint implementation flat out ignores the hint.
  auto const it = fm.emplace_hint(fm.begin(), TestKey{4}, "foobar");
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, TryEmplace) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.try_emplace(6, "foobar");
  EXPECT_THAT(*it, Pair(6, "foobar"));
  EXPECT_TRUE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, TryEmplaceCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const [it, inserted] = fm.try_emplace(4, "foobar");
  EXPECT_THAT(*it, Pair(4, "dolor"));
  EXPECT_FALSE(inserted);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, TryEmplaceHint) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  // Our emplace_hint implementation flat out ignores the hint.
  auto const it = fm.try_emplace(fm.begin(), 6, "foobar");
  EXPECT_THAT(*it, Pair(6, "foobar"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, TryEmplaceHintCollision) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  // Our emplace_hint implementation flat out ignores the hint.
  auto const it = fm.try_emplace(fm.begin(), 4, "foobar");
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EraseIterator) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.erase(fm.begin() + 2);
  EXPECT_THAT(*it, Pair(1, "consectetur"));
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", 1, "consectetur", 4, "dolor",
                                           5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EraseRange) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.erase(fm.begin() + 1, fm.begin() + 3);
  EXPECT_THAT(*it, Pair(1, "consectetur"));
  EXPECT_THAT(fm,
              TestValuesAre<TypeParam>(-3, "ipsum", 1, "consectetur", 4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EraseKey) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.erase(TestKey(1)), 1);
  EXPECT_THAT(
      fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EraseNotFound) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.erase(TestKey(7)), 0);
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EraseKeyTransparent) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.erase(OtherTestKey{1}), 1);
  EXPECT_THAT(fm, TransparentTestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 4,
                                                      "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Swap) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {2, "lorem"}, {3, "ipsum"},        {-4, "dolor"},    {1, "sit"},
      {2, "amet"},  {-1, "consectetur"}, {-5, "adipisci"}, {3, "elit"},
  };
  fm1.swap(fm2);
  EXPECT_THAT(fm1, TestValuesAre<TypeParam>(-5, "adipisci", -4, "dolor", -1, "consectetur", 1,
                                            "sit", 2, "lorem", 3, "ipsum"));
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, AdlSwap) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm1{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm2{
      {2, "lorem"}, {3, "ipsum"},        {-4, "dolor"},    {1, "sit"},
      {2, "amet"},  {-1, "consectetur"}, {-5, "adipisci"}, {3, "elit"},
  };
  swap(fm1, fm2);
  EXPECT_THAT(fm1, TestValuesAre<TypeParam>(-5, "adipisci", -4, "dolor", -1, "consectetur", 1,
                                            "sit", 2, "lorem", 3, "ipsum"));
  EXPECT_THAT(fm2, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur",
                                            4, "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ExtractIterator) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const node = fm.extract(fm.begin() + 2);
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.key(), -1);
  EXPECT_EQ(node.mapped(), "sit");
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", 1, "consectetur", 4, "dolor",
                                           5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ExtractKey) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const node = fm.extract(TestKey{-1});
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.key(), -1);
  EXPECT_EQ(node.mapped(), "sit");
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", 1, "consectetur", 4, "dolor",
                                           5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ExtractMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const node = fm.extract(TestKey{7});
  EXPECT_TRUE(node.empty());
  EXPECT_FALSE(node.operator bool());
  EXPECT_THAT(fm, TestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", -1, "sit", 1, "consectetur", 4,
                                           "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ExtractKeyTransparent) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const node = fm.extract(OtherTestKey{-1});
  EXPECT_FALSE(node.empty());
  EXPECT_TRUE(node.operator bool());
  EXPECT_EQ(node.key(), -1);
  EXPECT_EQ(node.mapped(), "sit");
  EXPECT_THAT(fm, TransparentTestValuesAre<TypeParam>(-3, "ipsum", -2, "lorem", 1, "consectetur", 4,
                                                      "dolor", 5, "adipisci"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Representation) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const& rep = fm.rep();
  EXPECT_THAT(rep, ElementsAre(Pair(-3, "ipsum"), Pair(-2, "lorem"), Pair(-1, "sit"),
                               Pair(1, "consectetur"), Pair(4, "dolor"), Pair(5, "adipisci")));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ExtractRep) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const rep = std::move(fm).ExtractRep();
  EXPECT_THAT(rep, ElementsAre(Pair(-3, "ipsum"), Pair(-2, "lorem"), Pair(-1, "sit"),
                               Pair(1, "consectetur"), Pair(4, "dolor"), Pair(5, "adipisci")));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Count) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_EQ(fm.count(OtherTestKey{-2}), 1);
  EXPECT_EQ(fm.count(OtherTestKey{6}), 0);
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Find) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey{4});
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, FindTransparent) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(OtherTestKey{4});
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, FindMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey{7});
  EXPECT_EQ(it, fm.end());
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstFind) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey{4});
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstFindTransparent) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(OtherTestKey{4});
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstFindMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey{7});
  EXPECT_EQ(it, fm.end());
}

TYPED_TEST_P(FlatMapWithRepresentationTest, Contains) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_TRUE(fm.contains(TestKey{4}));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ContainsTransparent) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_TRUE(fm.contains(OtherTestKey{4}));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ContainsMissing) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  EXPECT_FALSE(fm.contains(TestKey{7}));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, EqualRange) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey(1));
  EXPECT_THAT(fm.equal_range(TestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, TransparentEqualRange) {
  flat_map<TestKey, std::string, TransparentTestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey(1));
  EXPECT_THAT(fm.equal_range(OtherTestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstEqualRange) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.find(TestKey(1));
  EXPECT_THAT(fm.equal_range(TestKey(1)), Pair(it, it + 1));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, LowerBoundExclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.lower_bound(TestKey(0));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstLowerBoundExclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.lower_bound(TestKey(0));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, LowerBoundInclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.lower_bound(TestKey(1));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstLowerBoundInclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.lower_bound(TestKey(1));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, UpperBoundExclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.upper_bound(TestKey(0));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstUpperBoundExclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.upper_bound(TestKey(0));
  EXPECT_THAT(*it, Pair(1, "consectetur"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, UpperBoundInclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.upper_bound(TestKey(1));
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

TYPED_TEST_P(FlatMapWithRepresentationTest, ConstUpperBoundInclusive) {
  flat_map<TestKey, std::string, TestCompare, TypeParam> const fm{
      {-2, "lorem"}, {-3, "ipsum"},      {4, "dolor"},    {-1, "sit"},
      {-2, "amet"},  {1, "consectetur"}, {5, "adipisci"}, {-3, "elit"},
  };
  auto const it = fm.upper_bound(TestKey(1));
  EXPECT_THAT(*it, Pair(4, "dolor"));
}

REGISTER_TYPED_TEST_SUITE_P(
    FlatMapWithRepresentationTest, Construct, ConstructWithIterators, ConstructWithInitializerList,
    AssignInitializerList, Deduplication, GetAllocator, CompareEqual, CompareLHSLessThanRHS,
    CompareLHSGreaterThanRHS, ReverseCompareLHSLessThanRHS, ReverseCompareLHSGreaterThanRHS, At,
    ConstAt, AtMissing, ConstAtMissing, Subscript, SubscriptMissing, AssignSubscript,
    UpdateSubscript, CopyConstruct, Copy, MoveConstruct, Move, Empty, NotEmpty, Hash, Fingerprint,
    Clear, Insert, MoveInsert, InsertCollision, MoveInsertCollision, InsertFromIterators,
    InsertFromInitializerList, InsertNode, InsertNodeCollision, InsertEmptyNode, InsertAndNotAssign,
    MoveInsertAndNotAssign, AssignAndNotInsert, MoveAssignAndNotInsert, Emplace, EmplaceCollision,
    EmplaceHint, EmplaceHintCollision, TryEmplace, TryEmplaceCollision, TryEmplaceHint,
    TryEmplaceHintCollision, EraseIterator, EraseRange, EraseKey, EraseNotFound,
    EraseKeyTransparent, Swap, AdlSwap, ExtractIterator, ExtractKey, ExtractMissing,
    ExtractKeyTransparent, Representation, ExtractRep, Count, Find, FindTransparent, FindMissing,
    ConstFind, ConstFindTransparent, ConstFindMissing, Contains, ContainsTransparent,
    ContainsMissing, EqualRange, TransparentEqualRange, ConstEqualRange, LowerBoundExclusive,
    ConstLowerBoundExclusive, LowerBoundInclusive, ConstLowerBoundInclusive, UpperBoundExclusive,
    ConstUpperBoundExclusive, UpperBoundInclusive, ConstUpperBoundInclusive);

using RepresentationElement = std::pair<TestKey, std::string>;
using RepresentationTypes =
    ::testing::Types<std::vector<RepresentationElement>, std::deque<RepresentationElement>,
                     absl::InlinedVector<RepresentationElement, 1>,
                     absl::InlinedVector<RepresentationElement, 10>,
                     std::vector<RepresentationElement, std::allocator<RepresentationElement>>>;
INSTANTIATE_TYPED_TEST_SUITE_P(FlatMapWithRepresentationTest, FlatMapWithRepresentationTest,
                               RepresentationTypes);

TEST(FlatMapWithAllocatorTest, DefaultAllocator) {
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fm;
  EXPECT_THAT(fm.get_allocator(), Property(&TestAllocator::tag, 0));
}

TEST(FlatMapWithAllocatorTest, CustomAllocator) {
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fs{alloc};
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator ::tag, 42));
}

TEST(FlatMapWithAllocatorTest, CustomComparatorAndAllocator) {
  std::less<int> comp;
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fs{comp,
                                                                                         alloc};
  EXPECT_THAT(fs.get_allocator(), Property(&TestAllocator::tag, 42));
}

TEST(FlatMapWithAllocatorTest, IteratorsAndAllocator) {
  std::vector<std::pair<int, int>> v{{1, 2}, {3, 4}, {5, 6}, {7, 8}};
  std::less<int> comp;
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fm{
      v.begin(), v.end(), comp, alloc};
  EXPECT_THAT(fm, ElementsAre(Pair(1, 2), Pair(3, 4), Pair(5, 6), Pair(7, 8)));
  EXPECT_THAT(fm.get_allocator(), Property(&TestAllocator::tag, 42));
}

TEST(FlatMapWithAllocatorTest, InitializerListAndAllocator) {
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fm{
      {{1, 2}, {3, 4}, {5, 6}, {7, 8}}, alloc};
  EXPECT_THAT(fm, ElementsAre(Pair(1, 2), Pair(3, 4), Pair(5, 6), Pair(7, 8)));
  EXPECT_THAT(fm.get_allocator(), Property(&TestAllocator::tag, 42));
}

TEST(FlatMapWithAllocatorTest, CopyAllocator) {
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fm1{alloc};
  auto fm2 = fm1;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_THAT(fm2.get_allocator(), Property(&TestAllocator::tag, 42));
}

TEST(FlatMapWithAllocatorTest, MoveAllocator) {
  TestAllocator alloc{42};
  flat_map<int, int, std::less<int>, std::vector<std::pair<int, int>, TestAllocator>> fm1{alloc};
  auto fm2 = std::move(fm1);
  EXPECT_THAT(fm2.get_allocator(), Property(&TestAllocator::tag, 42));
}

TEST(FlatMapCapacityTest, InitialCapacity) {
  flat_map<int, std::string> fm;
  EXPECT_EQ(fm.capacity(), 0);
  EXPECT_EQ(fm.size(), 0);
}

TEST(FlatMapCapacityTest, CapacityAfterInsert) {
  flat_map<int, std::string> fm;
  fm.try_emplace(2, "lorem");
  fm.try_emplace(3, "ipsum");
  fm.try_emplace(1, "dolor");
  EXPECT_THAT(fm.capacity(), Ge(3));
  EXPECT_THAT(fm, ElementsAre(Pair(1, "dolor"), Pair(2, "lorem"), Pair(3, "ipsum")));
}

TEST(FlatMapCapacityTest, Reserve) {
  flat_map<int, std::string> fm;
  fm.reserve(3);
  EXPECT_EQ(fm.capacity(), 3);
  EXPECT_EQ(fm.size(), 0);
}

TEST(FlatMapCapacityTest, ReserveAndInsert) {
  flat_map<int, std::string> fm;
  fm.reserve(3);
  fm.try_emplace(2, "lorem");
  fm.try_emplace(3, "ipsum");
  fm.try_emplace(1, "dolor");
  EXPECT_EQ(fm.capacity(), 3);
  EXPECT_THAT(fm, ElementsAre(Pair(1, "dolor"), Pair(2, "lorem"), Pair(3, "ipsum")));
}

TEST(FlatMapCapacityTest, InsertMoreThanReserved) {
  flat_map<int, std::string> fm;
  fm.reserve(3);
  fm.try_emplace(2, "lorem");
  fm.try_emplace(3, "ipsum");
  fm.try_emplace(1, "dolor");
  fm.try_emplace(5, "amet");
  fm.try_emplace(4, "consectetur");
  EXPECT_THAT(fm.capacity(), Ge(5));
  EXPECT_THAT(fm, ElementsAre(Pair(1, "dolor"), Pair(2, "lorem"), Pair(3, "ipsum"),
                              Pair(4, "consectetur"), Pair(5, "amet")));
}

template <typename Key, typename T, typename Compare = std::less<Key>, typename... Inner>
auto FixedValuesAre(Inner&&... inner) {
  return TestValuesMatcher<flat_map<Key, T, Compare, std::array<std::pair<Key, T>, 3>>, Inner...>(
      std::forward<Inner>(inner)...);
}

TEST(FixedFlatMapTest, Empty) {
  auto constexpr fm = fixed_flat_map_of<int, std::string_view>({});
  EXPECT_TRUE(fm.empty());
}

TEST(FixedFlatMapTest, ConstructIntKeys) {
  auto constexpr fm = fixed_flat_map_of<int, std::string_view>({
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
  });
  EXPECT_THAT(fm, (FixedValuesAre<int, std::string_view>(1, "lorem", 2, "ipsum", 3, "dolor")));
}

TEST(FixedFlatMapTest, ConstructStringKeys) {
  auto constexpr fm = fixed_flat_map_of<std::string_view, std::string_view>({
      {"lorem", "ipsum"},
      {"dolor", "amet"},
      {"consectetur", "adipisci"},
  });
  EXPECT_THAT(fm, (FixedValuesAre<std::string_view, std::string_view>(
                      "consectetur", "adipisci", "dolor", "amet", "lorem", "ipsum")));
}

TEST(FixedFlatMapTest, SortInts) {
  auto constexpr fm = fixed_flat_map_of<int, std::string_view>({
      {1, "lorem"},
      {3, "ipsum"},
      {2, "dolor"},
  });
  EXPECT_THAT(fm, (FixedValuesAre<int, std::string_view>(1, "lorem", 2, "dolor", 3, "ipsum")));
}

TEST(FixedFlatMapTest, SortIntsReverse) {
  auto constexpr fm = fixed_flat_map_of<int, std::string_view, std::greater<int>>({
      {1, "lorem"},
      {3, "ipsum"},
      {2, "dolor"},
  });
  EXPECT_THAT(fm, (FixedValuesAre<int, std::string_view, std::greater<int>>(3, "ipsum", 2, "dolor",
                                                                            1, "lorem")));
}

TEST(FixedFlatMapTest, SortStringsReverse) {
  auto constexpr fm =
      fixed_flat_map_of<std::string_view, std::string_view, std::greater<std::string_view>>({
          {"lorem", "ipsum"},
          {"dolor", "amet"},
          {"consectetur", "adipisci"},
      });
  EXPECT_THAT(fm,
              (FixedValuesAre<std::string_view, std::string_view, std::greater<std::string_view>>(
                  "lorem", "ipsum", "dolor", "amet", "consectetur", "adipisci")));
}

TEST(FixedFlatSetDeathTest, ConstructIntsWithDuplicates) {
  EXPECT_DEATH((fixed_flat_map_of<int, std::string_view>({
                   {1, "lorem"},
                   {2, "ipsum"},
                   {1, "dolor"},
                   {3, "amet"},
               })),
               _);
}

TEST(FixedFlatSetDeathTest, ConstructStringsWithDuplicates) {
  EXPECT_DEATH((fixed_flat_map_of<std::string_view, std::string_view>({
                   {"a", "lorem"},
                   {"b", "ipsum"},
                   {"a", "dolor"},
                   {"c", "amet"},
               })),
               _);
}

}  // namespace
