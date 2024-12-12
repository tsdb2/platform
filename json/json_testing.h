#ifndef __TSDB2_JSON_JSON_TESTING_H__
#define __TSDB2_JSON_JSON_TESTING_H__

#include <ostream>
#include <utility>

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
template <char const field_name[], typename Inner>
class JsonFieldMatcher {
 public:
  using is_gtest_matcher = void;

  explicit JsonFieldMatcher(Inner inner) : inner_(std::move(inner)) {}

  // We can't provide meaningful implementations for our Describe methods because we won't know the
  // type of the field until `MatchAndExplain` is called.
  void DescribeTo(std::ostream* const /*os*/) const {}
  void DescribeNegationTo(std::ostream* const /*os*/) const {}

  template <typename... Fields>
  bool MatchAndExplain(tsdb2::json::Object<Fields...> const& value,
                       ::testing::MatchResultListener* const listener) const {
    *listener << "whose field \"" << field_name << "\" ";
    return ::testing::SafeMatcherCast<
               typename tsdb2::json::Object<Fields...>::template FieldType<field_name>>(inner_)
        .MatchAndExplain(value.template cget<field_name>(), listener);
  }

 private:
  Inner inner_;
};

template <char const field_name[], typename Inner>
JsonFieldMatcher<field_name, Inner> JsonField(Inner inner) {
  return JsonFieldMatcher<field_name, Inner>(std::move(inner));
}

}  // namespace json
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_JSON_JSON_TESTING_H__
