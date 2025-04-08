#ifndef __TSDB2_JSON_JSON_TESTING_H__
#define __TSDB2_JSON_JSON_TESTING_H__

#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/strings/escaping.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/json.h"

namespace tsdb2 {
namespace testing {
namespace json {

// GoogleTest matcher to check a field of a JSON object.
//
// Example:
//
//   char constexpr kField1[] = "lorem";
//   char constexpr kField2[] = "ipsum";
//
//   json::Object<
//     json::Field<int, kField1>,
//     json::Field<bool, kField2>
//   > obj = /* ... */;
//
//   EXPECT_THAT(obj, AllOf(JsonField<kField1>(42), JsonField<kField2>(true)));
//
template <char const field_name[], typename Object>
class JsonFieldMatcher;

template <char const field_name[], typename... Fields>
class JsonFieldMatcher<field_name, tsdb2::json::Object<Fields...>>
    : public ::testing::MatcherInterface<tsdb2::json::Object<Fields...> const&> {
 public:
  using is_gtest_matcher = void;

  using Field = typename tsdb2::json::Object<Fields...>::template FieldType<field_name>;

  template <typename Inner>
  explicit JsonFieldMatcher(Inner&& inner)
      : inner_(::testing::SafeMatcherCast<Field>(std::forward<Inner>(inner))) {}

  ~JsonFieldMatcher() = default;

  void DescribeTo(std::ostream* const os) const override {
    *os << "whose field \"" << absl::CEscape(field_name) << "\" ";
    inner_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "whose field \"" << absl::CEscape(field_name) << "\" ";
    inner_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(tsdb2::json::Object<Fields...> const& value,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "whose field \"" << absl::CEscape(field_name) << "\" ";
    return inner_.MatchAndExplain(value.template cget<field_name>(), listener);
  }

 private:
  JsonFieldMatcher(JsonFieldMatcher const&) = delete;
  JsonFieldMatcher& operator=(JsonFieldMatcher const&) = delete;
  JsonFieldMatcher(JsonFieldMatcher&&) = delete;
  JsonFieldMatcher& operator=(JsonFieldMatcher&&) = delete;

  ::testing::Matcher<Field> inner_;
};

template <char const field_name[], typename Inner>
class JsonFieldImpl {
 public:
  explicit JsonFieldImpl(Inner inner) : inner_(std::move(inner)) {}

  template <typename Object>
  operator ::testing::Matcher<Object>() const {  // NOLINT(google-explicit-constructor)
    return ::testing::Matcher<Object>(
        new JsonFieldMatcher<field_name, std::decay_t<Object>>(inner_));
  }

 private:
  std::decay_t<Inner> inner_;
};

template <char const field_name[], typename Inner>
auto JsonField(Inner&& inner) {
  return JsonFieldImpl<field_name, Inner>(std::forward<Inner>(inner));
}

}  // namespace json
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_JSON_JSON_TESTING_H__
