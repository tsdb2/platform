#include "common/fingerprint.h"

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/numeric/int128.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace foo {

class TestClass : public tsdb2::common::SimpleRefCounted {
 public:
  explicit TestClass(std::string_view const x, int const y, bool const z) : x_(x), y_(y), z_(z) {}

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, TestClass const& value) {
    return State::Combine(std::move(state), value.x_, value.y_, value.z_);
  }

 private:
  std::string x_;
  int y_;
  bool z_;
};

}  // namespace foo

namespace {

using ::tsdb2::common::FingerprintOf;
using ::tsdb2::common::flat_map;
using ::tsdb2::common::flat_set;

TEST(FingerprintTest, Integrals) {
  EXPECT_EQ(FingerprintOf(int8_t{42}), FingerprintOf(int8_t{42}));
  EXPECT_NE(FingerprintOf(int8_t{42}), FingerprintOf(int8_t{43}));
  EXPECT_EQ(FingerprintOf(int16_t{42}), FingerprintOf(int16_t{42}));
  EXPECT_NE(FingerprintOf(int16_t{42}), FingerprintOf(int16_t{43}));
  EXPECT_EQ(FingerprintOf(int32_t{42}), FingerprintOf(int32_t{42}));
  EXPECT_NE(FingerprintOf(int32_t{42}), FingerprintOf(int32_t{43}));
  EXPECT_EQ(FingerprintOf(int64_t{42}), FingerprintOf(int64_t{42}));
  EXPECT_NE(FingerprintOf(int64_t{42}), FingerprintOf(int64_t{43}));
  EXPECT_EQ(FingerprintOf(absl::int128{42}), FingerprintOf(absl::int128{42}));
  EXPECT_NE(FingerprintOf(absl::int128{42}), FingerprintOf(absl::int128{43}));
  EXPECT_EQ(FingerprintOf(uint8_t{42}), FingerprintOf(uint8_t{42}));
  EXPECT_NE(FingerprintOf(uint8_t{42}), FingerprintOf(uint8_t{43}));
  EXPECT_EQ(FingerprintOf(uint16_t{42}), FingerprintOf(uint16_t{42}));
  EXPECT_NE(FingerprintOf(uint16_t{42}), FingerprintOf(uint16_t{43}));
  EXPECT_EQ(FingerprintOf(uint32_t{42}), FingerprintOf(uint32_t{42}));
  EXPECT_NE(FingerprintOf(uint32_t{42}), FingerprintOf(uint32_t{43}));
  EXPECT_EQ(FingerprintOf(uint64_t{42}), FingerprintOf(uint64_t{42}));
  EXPECT_NE(FingerprintOf(uint64_t{42}), FingerprintOf(uint64_t{43}));
  EXPECT_EQ(FingerprintOf(absl::uint128{42}), FingerprintOf(absl::uint128{42}));
  EXPECT_NE(FingerprintOf(absl::uint128{42}), FingerprintOf(absl::uint128{43}));
}

TEST(FingerprintTest, Floats) {
  float constexpr pi1 = 3.141;
  double constexpr pi2 = 3.141;
  long double constexpr pi3 = 3.141;
  float constexpr e1 = 2.718;
  double constexpr e2 = 2.718;
  long double constexpr e3 = 2.718;
  EXPECT_EQ(FingerprintOf(pi1), FingerprintOf(pi1));
  EXPECT_NE(FingerprintOf(pi1), FingerprintOf(e1));
  EXPECT_EQ(FingerprintOf(pi2), FingerprintOf(pi2));
  EXPECT_NE(FingerprintOf(pi2), FingerprintOf(e2));
  EXPECT_EQ(FingerprintOf(pi3), FingerprintOf(pi3));
  EXPECT_NE(FingerprintOf(pi3), FingerprintOf(e3));
}

TEST(FingerprintTest, Booleans) {
  EXPECT_EQ(FingerprintOf(true), FingerprintOf(true));
  EXPECT_NE(FingerprintOf(true), FingerprintOf(false));
  EXPECT_EQ(FingerprintOf(false), FingerprintOf(false));
}

TEST(FingerprintTest, Strings) {
  EXPECT_EQ(FingerprintOf(std::string("lorem ipsum")), FingerprintOf(std::string("lorem ipsum")));
  EXPECT_NE(FingerprintOf(std::string("lorem ipsum")), FingerprintOf(std::string("dolor amet")));
  EXPECT_EQ(FingerprintOf(std::string_view("lorem ipsum")),
            FingerprintOf(std::string_view("lorem ipsum")));
  EXPECT_EQ(FingerprintOf(std::string_view("lorem ipsum")),
            FingerprintOf(std::string("lorem ipsum")));
  EXPECT_NE(FingerprintOf(std::string_view("lorem ipsum")),
            FingerprintOf(std::string_view("dolor amet")));
  EXPECT_NE(FingerprintOf(std::string_view("lorem ipsum")),
            FingerprintOf(std::string("dolor amet")));
  EXPECT_EQ(FingerprintOf("lorem ipsum"), FingerprintOf("lorem ipsum"));
  EXPECT_EQ(FingerprintOf("lorem ipsum"), FingerprintOf(std::string_view("lorem ipsum")));
  EXPECT_NE(FingerprintOf("lorem ipsum"), FingerprintOf("dolor amet"));
  EXPECT_NE(FingerprintOf("lorem ipsum"), FingerprintOf(std::string_view("dolor amet")));
  char const ch1[6] = "lorem";
  char const ch2[6] = "ipsum";
  char const ch3[5] = {'l', 'o', 'r', 'e', 'm'};
  char ch4[6] = "lorem";
  char ch5[6] = "ipsum";
  char ch6[5] = {'l', 'o', 'r', 'e', 'm'};
  EXPECT_EQ(FingerprintOf("lorem"), FingerprintOf(ch1));
  EXPECT_EQ(FingerprintOf(ch1), FingerprintOf(ch1));
  EXPECT_NE(FingerprintOf(ch1), FingerprintOf(ch2));
  EXPECT_EQ(FingerprintOf(ch1), FingerprintOf(ch3));
  EXPECT_EQ(FingerprintOf(ch1), FingerprintOf(ch4));
  EXPECT_NE(FingerprintOf(ch1), FingerprintOf(ch5));
  EXPECT_EQ(FingerprintOf(ch1), FingerprintOf(ch6));
}

TEST(FingerprintTest, Pointers) {
  std::string const s1 = "foo";
  std::string const s2 = "bar";
  auto const s3 = std::make_unique<std::string>("foo");
  auto const s4 = std::make_shared<std::string>("foo");
  std::string const* const p = nullptr;
  int constexpr i1 = 42;
  int constexpr i2 = 43;
  auto const i3 = std::make_unique<int>(42);
  auto const i4 = std::make_shared<int>(42);
  bool constexpr b1 = false;
  bool constexpr b2 = true;
  EXPECT_EQ(FingerprintOf(p), FingerprintOf(nullptr));
  EXPECT_EQ(FingerprintOf(&s1), FingerprintOf(&s1));
  EXPECT_NE(FingerprintOf(&s1), FingerprintOf(nullptr));
  EXPECT_NE(FingerprintOf(&s1), FingerprintOf(&s2));
  EXPECT_NE(FingerprintOf(&s1), FingerprintOf(&i1));
  EXPECT_EQ(FingerprintOf(&s1), FingerprintOf(s3));
  EXPECT_EQ(FingerprintOf(&s1), FingerprintOf(s4));
  EXPECT_EQ(FingerprintOf(&i1), FingerprintOf(&i1));
  EXPECT_NE(FingerprintOf(&i1), FingerprintOf(nullptr));
  EXPECT_NE(FingerprintOf(&i1), FingerprintOf(&i2));
  EXPECT_EQ(FingerprintOf(&i1), FingerprintOf(i3));
  EXPECT_EQ(FingerprintOf(&i1), FingerprintOf(i4));
  EXPECT_EQ(FingerprintOf(&b1), FingerprintOf(&b1));
  EXPECT_NE(FingerprintOf(&b1), FingerprintOf(nullptr));
  EXPECT_NE(FingerprintOf(&b1), FingerprintOf(&b2));
}

TEST(FingerprintTest, Times) {
  auto const now = absl::Now();
  EXPECT_EQ(now + absl::Seconds(123), now + absl::Seconds(123));
  EXPECT_NE(now + absl::Seconds(123), now + absl::Seconds(321));
}

TEST(FingerprintTest, Durations) {
  EXPECT_EQ(absl::Nanoseconds(123), absl::Nanoseconds(123));
  EXPECT_NE(absl::Nanoseconds(123), absl::Nanoseconds(321));
  EXPECT_EQ(absl::Hours(123), absl::Hours(123));
  EXPECT_NE(absl::Hours(123), absl::Hours(321));
}

TEST(FingerprintTest, Tuples) {
  std::string const s = "foobar";
  int constexpr i = 42;
  bool constexpr b = true;
  float constexpr f = 3.14;
  EXPECT_EQ(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(s, i, b, f)));
  EXPECT_NE(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(s, i, b, f, s)));
  EXPECT_NE(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(i, s, b, f)));
  EXPECT_NE(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(s, i, b)));
  EXPECT_NE(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(s, i)));
  EXPECT_NE(FingerprintOf(std::tie(s, i, b, f)), FingerprintOf(std::tie(s)));
  EXPECT_NE(FingerprintOf(std::make_tuple(i, b)), FingerprintOf(std::make_tuple(i + 1, b)));
  EXPECT_NE(FingerprintOf(std::make_tuple(i, b)), FingerprintOf(std::make_tuple(i, !b)));
  EXPECT_NE(FingerprintOf(std::make_tuple(i, b)), FingerprintOf(std::make_tuple(b, i)));
}

TEST(FingerprintTest, Optionals) {
  std::optional<std::string> const s1 = "foo";
  std::optional<std::string> const s2 = "bar";
  std::optional<std::string> const s3 = std::nullopt;
  std::optional<int> const i = 42;
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s1));
  EXPECT_NE(FingerprintOf(s1), FingerprintOf(std::nullopt));
  EXPECT_EQ(FingerprintOf(s3), FingerprintOf(std::nullopt));
  EXPECT_NE(FingerprintOf(s1), FingerprintOf(s2));
  EXPECT_NE(FingerprintOf(s1), FingerprintOf(i));
}

TEST(FingerprintTest, Variant) {
  using Variant = std::variant<std::string, int, bool>;
  Variant const v1 = std::string("foo");
  Variant const v2 = 123;
  Variant const v3 = true;
  EXPECT_EQ(FingerprintOf(v1), FingerprintOf(v1));
  EXPECT_NE(FingerprintOf(v1), FingerprintOf(v2));
  EXPECT_NE(FingerprintOf(v1), FingerprintOf(v3));
}

TEST(FingerprintTest, Arrays) {
  std::vector<std::string> a1{"lorem", "ipsum", "dolor", "amet"};
  std::vector<std::string> a1_1{"lorem", "ipsum", "dolor", "amet"};
  std::vector<std::string> a1_2{"lorem", "ipsum", "dolor"};
  std::vector<std::string> a1_3{"lorem", "ipsum"};
  std::vector<std::string> a1_4{"lorem"};
  std::vector<std::string> a2{"foo", "bar", "baz", "qux"};
  std::vector<std::string> a3{"foo", "bar", "baz"};
  std::string a4[] = {"lorem", "ipsum", "dolor", "amet"};
  std::array<std::string, 4> a5 = {"lorem", "ipsum", "dolor", "amet"};
  std::deque<std::string> a6 = {"lorem", "ipsum", "dolor", "amet"};
  absl::InlinedVector<std::string, 2> a7{"lorem", "ipsum", "dolor", "amet"};
  absl::InlinedVector<std::string, 6> a8{"lorem", "ipsum", "dolor", "amet"};
  std::string a9_1[4] = {"lorem", "ipsum", "dolor", "amet"};
  std::string_view a9_2[4] = {"lorem", "ipsum", "dolor", "amet"};
  std::string s = "lorem";
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a1));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a1_1));
  EXPECT_NE(FingerprintOf(a1_1), FingerprintOf(a1_2));
  EXPECT_NE(FingerprintOf(a1_1), FingerprintOf(a1_3));
  EXPECT_NE(FingerprintOf(a1_1), FingerprintOf(a1_4));
  EXPECT_NE(FingerprintOf(a1_2), FingerprintOf(a1_3));
  EXPECT_NE(FingerprintOf(a1_2), FingerprintOf(a1_4));
  EXPECT_NE(FingerprintOf(a1_3), FingerprintOf(a1_4));
  EXPECT_NE(FingerprintOf(a1), FingerprintOf(a2));
  EXPECT_NE(FingerprintOf(a2), FingerprintOf(a3));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a4));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a5));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a6));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a7));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a8));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a9_1));
  EXPECT_NE(FingerprintOf(a9_1), FingerprintOf(&s));
  EXPECT_NE(FingerprintOf(a9_1), FingerprintOf(s));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf(a9_2));
  EXPECT_NE(FingerprintOf(a9_2), FingerprintOf(&s));
  EXPECT_NE(FingerprintOf(a9_2), FingerprintOf(s));
  EXPECT_EQ(FingerprintOf(a1), FingerprintOf({"lorem", "ipsum", "dolor", "amet"}));
  EXPECT_EQ(FingerprintOf({"lorem", "ipsum", "dolor", "amet"}),
            FingerprintOf({"lorem", "ipsum", "dolor", "amet"}));
  EXPECT_NE(FingerprintOf({"lorem", "ipsum", "dolor", "amet"}), FingerprintOf({"lorem"}));
  EXPECT_NE(FingerprintOf({"lorem", "ipsum", "dolor", "amet"}),
            FingerprintOf({"foo", "bar", "baz", "qux"}));
}

TEST(FingerprintTest, Sets) {
  std::set<std::string> s1{"lorem", "ipsum", "dolor", "amet"};
  std::set<std::string> s2{"foo", "bar", "baz", "qux"};
  std::set<std::string> s3{"foo", "bar", "baz"};
  absl::btree_set<std::string> s4{"lorem", "ipsum", "dolor", "amet"};
  flat_set<std::string> s5{"lorem", "ipsum", "dolor", "amet"};
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s1));
  EXPECT_NE(FingerprintOf(s1), FingerprintOf(s2));
  EXPECT_NE(FingerprintOf(s2), FingerprintOf(s3));
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s4));
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s5));
}

TEST(FingerprintTest, UnorderedSets) {
  std::unordered_set<std::string> s1{"lorem", "ipsum", "dolor", "amet"};
  std::unordered_set<std::string> s2{"foo", "bar", "baz", "qux"};
  std::unordered_set<std::string> s3{"foo", "bar", "baz"};
  absl::flat_hash_set<std::string> s4{"lorem", "ipsum", "dolor", "amet"};
  absl::node_hash_set<std::string> s5{"lorem", "ipsum", "dolor", "amet"};
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s1));
  EXPECT_NE(FingerprintOf(s1), FingerprintOf(s2));
  EXPECT_NE(FingerprintOf(s2), FingerprintOf(s3));
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s4));
  EXPECT_EQ(FingerprintOf(s1), FingerprintOf(s5));
}

TEST(FingerprintTest, Maps) {
  std::map<int, std::string> m1{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  std::map<int, std::string> m2{
      {1, "foo"},
      {2, "bar"},
      {3, "baz"},
      {4, "qux"},
  };
  std::map<int, std::string> m3{
      {1, "foo"},
      {2, "bar"},
      {3, "baz"},
  };
  absl::btree_map<int, std::string> m4{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  flat_map<int, std::string> m5{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m1));
  EXPECT_NE(FingerprintOf(m1), FingerprintOf(m2));
  EXPECT_NE(FingerprintOf(m2), FingerprintOf(m3));
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m4));
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m5));
}

TEST(FingerprintTest, UnorderedMaps) {
  std::unordered_map<int, std::string> m1{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  std::unordered_map<int, std::string> m2{
      {1, "foo"},
      {2, "bar"},
      {3, "baz"},
      {4, "qux"},
  };
  std::unordered_map<int, std::string> m3{
      {1, "foo"},
      {2, "bar"},
      {3, "baz"},
  };
  absl::flat_hash_map<int, std::string> m4{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  absl::node_hash_map<int, std::string> m5{
      {1, "lorem"},
      {2, "ipsum"},
      {3, "dolor"},
      {4, "amet"},
  };
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m1));
  EXPECT_NE(FingerprintOf(m1), FingerprintOf(m2));
  EXPECT_NE(FingerprintOf(m2), FingerprintOf(m3));
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m4));
  EXPECT_EQ(FingerprintOf(m1), FingerprintOf(m5));
}

TEST(FingerprintTest, CustomObject) {
  ::foo::TestClass value{"foo", 42, true};
  EXPECT_EQ(FingerprintOf(value), FingerprintOf(::foo::TestClass("foo", 42, true)));
  EXPECT_NE(FingerprintOf(value), FingerprintOf(::foo::TestClass("bar", 43, false)));
  EXPECT_EQ(FingerprintOf(&value),
            FingerprintOf(tsdb2::common::MakeReffed<::foo::TestClass>("foo", 42, true)));
  EXPECT_EQ(FingerprintOf(value), FingerprintOf(std::make_tuple(std::string("foo"), 42, true)));
  EXPECT_EQ(FingerprintOf(value), FingerprintOf(std::make_tuple("foo", 42, true)));
}

}  // namespace
