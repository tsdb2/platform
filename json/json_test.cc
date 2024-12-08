#include "json/json.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/fingerprint.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/testing.h"
#include "common/trie_map.h"
#include "common/trie_set.h"
#include "common/type_string.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "json/json_testing.h"

namespace {

namespace json = tsdb2::json;

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Pointee2;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::tsdb2::common::TypeStringT;
using ::tsdb2::testing::json::JsonField;

char constexpr kFieldName1[] = "lorem";
char constexpr kFieldName2[] = "ipsum";
char constexpr kFieldName3[] = "dolor";
char constexpr kFieldName4[] = "sit";
char constexpr kFieldName5[] = "amet";
char constexpr kFieldName6[] = "consectetur";
char constexpr kFieldName7[] = "adipisci";
char constexpr kFieldName8[] = "elit";

using TestObject1 = json::Object<                                  //
    json::Field<int, kFieldName1>,                                 //
    json::Field<bool, kFieldName2>,                                //
    json::Field<std::string, kFieldName3>,                         //
    json::Field<double, kFieldName4>,                              //
    json::Field<std::array<int, 3>, kFieldName5>,                  //
    json::Field<std::vector<int>, kFieldName6>,                    //
    json::Field<std::tuple<int, bool, std::string>, kFieldName7>,  //
    json::Field<std::optional<double>, kFieldName8>>;

using TestObject2 = json::Object<                            //
    json::Field<std::unique_ptr<std::string>, kFieldName1>,  //
    json::Field<TestObject1, kFieldName2>,                   //
    json::Field<std::shared_ptr<std::string>, kFieldName3>,  //
    json::Field<std::map<std::string, int>, kFieldName4>,    //
    json::Field<std::pair<int, int>, kFieldName5>            //
    >;

// This is like `TestObject2` but without std::unique_ptr so that we can use it to test copies.
using TestObject3 = json::Object<                            //
    json::Field<std::shared_ptr<std::string>, kFieldName1>,  //
    json::Field<TestObject1, kFieldName2>,                   //
    json::Field<std::map<std::string, int>, kFieldName3>,    //
    json::Field<std::pair<int, int>, kFieldName4>            //
    >;

json::ParseOptions constexpr kParseOptions1{.allow_extra_fields = false};
json::ParseOptions constexpr kParseOptions2{.allow_extra_fields = true, .fast_skipping = false};
json::ParseOptions constexpr kParseOptions3{.allow_extra_fields = true, .fast_skipping = true};

json::StringifyOptions constexpr kStringifyOptions1{.pretty = false};
json::StringifyOptions constexpr kStringifyOptions2{.pretty = true, .indent_width = 2};
json::StringifyOptions constexpr kStringifyOptions3{.pretty = true, .indent_width = 4};

TEST(JsonTest, CheckUniqueName) {
  using json::internal::CheckUniqueNameV;
  EXPECT_TRUE((CheckUniqueNameV<TypeStringT<kFieldName1>>));
  EXPECT_TRUE((CheckUniqueNameV<TypeStringT<kFieldName1>, json::Field<int, kFieldName2>>));
  EXPECT_FALSE((CheckUniqueNameV<TypeStringT<kFieldName1>, json::Field<int, kFieldName1>>));
  EXPECT_TRUE((CheckUniqueNameV<TypeStringT<kFieldName1>, json::Field<int, kFieldName2>,
                                json::Field<int, kFieldName3>>));
  EXPECT_FALSE((CheckUniqueNameV<TypeStringT<kFieldName1>, json::Field<int, kFieldName1>,
                                 json::Field<int, kFieldName3>>));
  EXPECT_FALSE((CheckUniqueNameV<TypeStringT<kFieldName1>, json::Field<int, kFieldName2>,
                                 json::Field<int, kFieldName1>>));
}

TEST(JsonTest, FieldTypes) {
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName1>, int>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName2>, bool>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName3>, std::string>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName4>, double>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName5>, std::array<int, 3>>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName6>, std::vector<int>>));
  EXPECT_TRUE((std::is_same_v<typename TestObject1::FieldType<kFieldName7>,
                              std::tuple<int, bool, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<typename TestObject1::FieldType<kFieldName8>, std::optional<double>>));
}

TEST(JsonTest, FieldAccess) {
  TestObject1 object;
  object.get<kFieldName1>() = 42;
  object.get<kFieldName2>() = true;
  object.get<kFieldName3>() = "foobar";
  object.get<kFieldName4>() = 3.14;
  object.get<kFieldName5>() = {1, 2, 3};
  object.get<kFieldName6>() = {4, 5, 6, 7};
  object.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  object.get<kFieldName8>() = 2.71;
  TestObject1 const& ref = object;
  EXPECT_THAT(ref, AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                         JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                         JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                         JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                         JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                         JsonField<kFieldName8>(Optional<double>(2.71))));
}

TEST(JsonTest, Initialization) {
  TestObject1 object{
      json::kInitialize,
      43,
      false,
      "barbaz",
      2.71,
      std::array<int, 3>{3, 2, 1},
      std::vector<int>{7, 6, 5, 4},
      std::make_tuple(42, true, "bazfoo"),
      std::make_optional(3.14),
  };
  TestObject1 const& ref = object;
  EXPECT_THAT(ref, AllOf(JsonField<kFieldName1>(43), JsonField<kFieldName2>(false),
                         JsonField<kFieldName3>("barbaz"), JsonField<kFieldName4>(2.71),
                         JsonField<kFieldName5>(ElementsAre(3, 2, 1)),
                         JsonField<kFieldName6>(ElementsAre(7, 6, 5, 4)),
                         JsonField<kFieldName7>(FieldsAre(42, true, "bazfoo")),
                         JsonField<kFieldName8>(Optional<double>(3.14))));
}

TEST(JsonTest, NestedFieldAccess) {
  TestObject2 object;
  object.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  object.get<kFieldName2>().get<kFieldName1>() = 43;
  object.get<kFieldName2>().get<kFieldName2>() = false;
  object.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  object.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  object.get<kFieldName5>() = std::make_pair(12, 34);
  TestObject2 const& ref = object;
  EXPECT_THAT(ref, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                         JsonField<kFieldName2>(JsonField<kFieldName1>(43)),
                         JsonField<kFieldName2>(JsonField<kFieldName2>(false)),
                         JsonField<kFieldName3>(Pointee(std::string("barbaz"))),
                         JsonField<kFieldName4>(UnorderedElementsAre(
                             Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                         JsonField<kFieldName5>(Pair(12, 34))));
}

TEST(JsonTest, Clear) {
  TestObject2 object;
  TestObject1& inner = object.get<kFieldName2>();
  object.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  object.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  object.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  object.get<kFieldName5>() = std::make_pair(12, 34);
  object.Clear();
  EXPECT_FALSE(object.get<kFieldName1>());
  EXPECT_THAT(
      object,
      AllOf(JsonField<kFieldName2>(JsonField<kFieldName1>(0)),
            JsonField<kFieldName2>(JsonField<kFieldName2>(false)),
            JsonField<kFieldName2>(JsonField<kFieldName3>("")),
            JsonField<kFieldName2>(JsonField<kFieldName4>(0.0)),
            JsonField<kFieldName2>(JsonField<kFieldName5>(ElementsAre(0, 0, 0))),
            JsonField<kFieldName2>(JsonField<kFieldName6>(IsEmpty())),
            JsonField<kFieldName2>(JsonField<kFieldName7>(std::tuple<int, bool, std::string>())),
            JsonField<kFieldName2>(JsonField<kFieldName8>(std::nullopt)),
            JsonField<kFieldName3>(IsNull()), JsonField<kFieldName4>(IsEmpty()),
            JsonField<kFieldName5>(Pair(0, 0))));
}

TEST(JsonTest, CopyConstruction) {
  TestObject3 obj1;
  TestObject1& inner = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_shared<std::string>("foobar");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName4>() = std::make_pair(12, 34);
  TestObject3 obj2{obj1};
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(JsonField<kFieldName1>(42)),
                          JsonField<kFieldName2>(JsonField<kFieldName2>(true)),
                          JsonField<kFieldName2>(JsonField<kFieldName3>("foobar")),
                          JsonField<kFieldName2>(JsonField<kFieldName4>(3.14)),
                          JsonField<kFieldName2>(JsonField<kFieldName5>(ElementsAre(1, 2, 3))),
                          JsonField<kFieldName2>(JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7))),
                          JsonField<kFieldName2>(
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz")))),
                          JsonField<kFieldName2>(JsonField<kFieldName8>(Optional(2.71))),
                          JsonField<kFieldName3>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName4>(Pair(12, 34))));
}

TEST(JsonTest, CopyAssignment) {
  TestObject3 obj1;
  TestObject1& inner = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_shared<std::string>("foobar");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName4>() = std::make_pair(12, 34);
  TestObject3 obj2;
  obj2 = obj1;
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(JsonField<kFieldName1>(42)),
                          JsonField<kFieldName2>(JsonField<kFieldName2>(true)),
                          JsonField<kFieldName2>(JsonField<kFieldName3>("foobar")),
                          JsonField<kFieldName2>(JsonField<kFieldName4>(3.14)),
                          JsonField<kFieldName2>(JsonField<kFieldName5>(ElementsAre(1, 2, 3))),
                          JsonField<kFieldName2>(JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7))),
                          JsonField<kFieldName2>(
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz")))),
                          JsonField<kFieldName2>(JsonField<kFieldName8>(Optional(2.71))),
                          JsonField<kFieldName3>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName4>(Pair(12, 34))));
}

TEST(JsonTest, MoveConstruction) {
  TestObject2 obj1;
  TestObject1& inner = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  obj1.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName5>() = std::make_pair(12, 34);
  TestObject2 obj2{std::move(obj1)};
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(JsonField<kFieldName1>(42)),
                          JsonField<kFieldName2>(JsonField<kFieldName2>(true)),
                          JsonField<kFieldName2>(JsonField<kFieldName3>("foobar")),
                          JsonField<kFieldName2>(JsonField<kFieldName4>(3.14)),
                          JsonField<kFieldName2>(JsonField<kFieldName5>(ElementsAre(1, 2, 3))),
                          JsonField<kFieldName2>(JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7))),
                          JsonField<kFieldName2>(
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz")))),
                          JsonField<kFieldName2>(JsonField<kFieldName8>(Optional(2.71))),
                          JsonField<kFieldName3>(Pointee(std::string("barbaz"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(12, 34))));
}

TEST(JsonTest, MoveAssignment) {
  TestObject2 obj1;
  TestObject1& inner = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  obj1.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName5>() = std::make_pair(12, 34);
  TestObject2 obj2;
  obj2 = std::move(obj1);
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(JsonField<kFieldName1>(42)),
                          JsonField<kFieldName2>(JsonField<kFieldName2>(true)),
                          JsonField<kFieldName2>(JsonField<kFieldName3>("foobar")),
                          JsonField<kFieldName2>(JsonField<kFieldName4>(3.14)),
                          JsonField<kFieldName2>(JsonField<kFieldName5>(ElementsAre(1, 2, 3))),
                          JsonField<kFieldName2>(JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7))),
                          JsonField<kFieldName2>(
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz")))),
                          JsonField<kFieldName2>(JsonField<kFieldName8>(Optional(2.71))),
                          JsonField<kFieldName3>(Pointee(std::string("barbaz"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(12, 34))));
}

TEST(JsonTest, Swap) {
  TestObject2 obj1;
  TestObject1& inner1 = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  inner1.get<kFieldName1>() = 42;
  inner1.get<kFieldName2>() = true;
  inner1.get<kFieldName3>() = "foobar";
  inner1.get<kFieldName4>() = 3.14;
  inner1.get<kFieldName5>() = {1, 2, 3};
  inner1.get<kFieldName6>() = {4, 5, 6, 7};
  inner1.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner1.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  obj1.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName5>() = std::make_pair(12, 34);
  TestObject2 obj2;
  TestObject1& inner2 = obj2.get<kFieldName2>();
  obj2.get<kFieldName1>() = std::make_unique<std::string>("barfoo");
  inner2.get<kFieldName1>() = 24;
  inner2.get<kFieldName2>() = false;
  inner2.get<kFieldName3>() = "barbaz";
  inner2.get<kFieldName4>() = 2.71;
  inner2.get<kFieldName5>() = {3, 2, 1};
  inner2.get<kFieldName6>() = {7, 6, 5, 4};
  inner2.get<kFieldName7>() = std::make_tuple(44, true, "bazfoo");
  inner2.get<kFieldName8>() = 3.14;
  obj2.get<kFieldName3>() = std::make_shared<std::string>("bazbar");
  obj2.get<kFieldName4>() = {{"foo", 24}, {"bar", 34}, {"baz", 44}};
  obj2.get<kFieldName5>() = std::make_pair(34, 12);
  obj1.swap(obj2);
  EXPECT_THAT(obj1, AllOf(JsonField<kFieldName1>(Pointee(std::string("barfoo"))),
                          JsonField<kFieldName2>(AllOf(
                              JsonField<kFieldName1>(24), JsonField<kFieldName2>(false),
                              JsonField<kFieldName3>("barbaz"), JsonField<kFieldName4>(2.71),
                              JsonField<kFieldName5>(ElementsAre(3, 2, 1)),
                              JsonField<kFieldName6>(ElementsAre(7, 6, 5, 4)),
                              JsonField<kFieldName7>(FieldsAre(44, true, std::string("bazfoo"))),
                              JsonField<kFieldName8>(Optional(3.14)))),
                          JsonField<kFieldName3>(Pointee(std::string("bazbar"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 24), Pair("bar", 34), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(34, 12))));
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(AllOf(
                              JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                              JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                              JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                              JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz"))),
                              JsonField<kFieldName8>(Optional(2.71)))),
                          JsonField<kFieldName3>(Pointee(std::string("barbaz"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(12, 34))));
}

TEST(JsonTest, AdlSwap) {
  TestObject2 obj1;
  TestObject1& inner1 = obj1.get<kFieldName2>();
  obj1.get<kFieldName1>() = std::make_unique<std::string>("foobar");
  inner1.get<kFieldName1>() = 42;
  inner1.get<kFieldName2>() = true;
  inner1.get<kFieldName3>() = "foobar";
  inner1.get<kFieldName4>() = 3.14;
  inner1.get<kFieldName5>() = {1, 2, 3};
  inner1.get<kFieldName6>() = {4, 5, 6, 7};
  inner1.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner1.get<kFieldName8>() = 2.71;
  obj1.get<kFieldName3>() = std::make_shared<std::string>("barbaz");
  obj1.get<kFieldName4>() = {{"foo", 42}, {"bar", 43}, {"baz", 44}};
  obj1.get<kFieldName5>() = std::make_pair(12, 34);
  TestObject2 obj2;
  TestObject1& inner2 = obj2.get<kFieldName2>();
  obj2.get<kFieldName1>() = std::make_unique<std::string>("barfoo");
  inner2.get<kFieldName1>() = 24;
  inner2.get<kFieldName2>() = false;
  inner2.get<kFieldName3>() = "barbaz";
  inner2.get<kFieldName4>() = 2.71;
  inner2.get<kFieldName5>() = {3, 2, 1};
  inner2.get<kFieldName6>() = {7, 6, 5, 4};
  inner2.get<kFieldName7>() = std::make_tuple(44, true, "bazfoo");
  inner2.get<kFieldName8>() = 3.14;
  obj2.get<kFieldName3>() = std::make_shared<std::string>("bazbar");
  obj2.get<kFieldName4>() = {{"foo", 24}, {"bar", 34}, {"baz", 44}};
  obj2.get<kFieldName5>() = std::make_pair(34, 12);
  swap(obj1, obj2);
  EXPECT_THAT(obj1, AllOf(JsonField<kFieldName1>(Pointee(std::string("barfoo"))),
                          JsonField<kFieldName2>(AllOf(
                              JsonField<kFieldName1>(24), JsonField<kFieldName2>(false),
                              JsonField<kFieldName3>("barbaz"), JsonField<kFieldName4>(2.71),
                              JsonField<kFieldName5>(ElementsAre(3, 2, 1)),
                              JsonField<kFieldName6>(ElementsAre(7, 6, 5, 4)),
                              JsonField<kFieldName7>(FieldsAre(44, true, std::string("bazfoo"))),
                              JsonField<kFieldName8>(Optional(3.14)))),
                          JsonField<kFieldName3>(Pointee(std::string("bazbar"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 24), Pair("bar", 34), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(34, 12))));
  EXPECT_THAT(obj2, AllOf(JsonField<kFieldName1>(Pointee(std::string("foobar"))),
                          JsonField<kFieldName2>(AllOf(
                              JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                              JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                              JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                              JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                              JsonField<kFieldName7>(FieldsAre(43, false, std::string("barbaz"))),
                              JsonField<kFieldName8>(Optional(2.71)))),
                          JsonField<kFieldName3>(Pointee(std::string("barbaz"))),
                          JsonField<kFieldName4>(UnorderedElementsAre(
                              Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
                          JsonField<kFieldName5>(Pair(12, 34))));
}

TEST(JsonTest, EmptyObjectComparisons) {
  EXPECT_TRUE(json::Object<>() == json::Object<>());
  EXPECT_FALSE(json::Object<>() != json::Object<>());
  EXPECT_FALSE(json::Object<>() < json::Object<>());
  EXPECT_TRUE(json::Object<>() <= json::Object<>());
  EXPECT_FALSE(json::Object<>() > json::Object<>());
  EXPECT_TRUE(json::Object<>() >= json::Object<>());
}

TEST(JsonTest, CompareOneField) {
  json::Object<json::Field<int, kFieldName1>> obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj2.get<kFieldName1>() = 43;
  EXPECT_TRUE(obj1 == obj1);
  EXPECT_FALSE(obj1 == obj2);
  EXPECT_FALSE(obj1 != obj1);
  EXPECT_TRUE(obj1 != obj2);
  EXPECT_FALSE(obj1 < obj1);
  EXPECT_TRUE(obj1 < obj2);
  EXPECT_TRUE(obj1 <= obj1);
  EXPECT_TRUE(obj1 <= obj2);
  EXPECT_FALSE(obj1 > obj1);
  EXPECT_FALSE(obj1 > obj2);
  EXPECT_TRUE(obj1 >= obj1);
  EXPECT_FALSE(obj1 >= obj2);
}

TEST(JsonTest, CompareTwoFieldsFirstEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 123;
  obj2.get<kFieldName1>() = 42;
  obj2.get<kFieldName2>() = 456;
  EXPECT_TRUE(obj1 == obj1);
  EXPECT_FALSE(obj1 == obj2);
  EXPECT_FALSE(obj1 != obj1);
  EXPECT_TRUE(obj1 != obj2);
  EXPECT_FALSE(obj1 < obj1);
  EXPECT_TRUE(obj1 < obj2);
  EXPECT_TRUE(obj1 <= obj1);
  EXPECT_TRUE(obj1 <= obj2);
  EXPECT_FALSE(obj1 > obj1);
  EXPECT_FALSE(obj1 > obj2);
  EXPECT_TRUE(obj1 >= obj1);
  EXPECT_FALSE(obj1 >= obj2);
}

TEST(JsonTest, CompareTwoFieldsSecondEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 123;
  obj2.get<kFieldName1>() = 43;
  obj2.get<kFieldName2>() = 456;
  EXPECT_TRUE(obj1 == obj1);
  EXPECT_FALSE(obj1 == obj2);
  EXPECT_FALSE(obj1 != obj1);
  EXPECT_TRUE(obj1 != obj2);
  EXPECT_FALSE(obj1 < obj1);
  EXPECT_TRUE(obj1 < obj2);
  EXPECT_TRUE(obj1 <= obj1);
  EXPECT_TRUE(obj1 <= obj2);
  EXPECT_FALSE(obj1 > obj1);
  EXPECT_FALSE(obj1 > obj2);
  EXPECT_TRUE(obj1 >= obj1);
  EXPECT_FALSE(obj1 >= obj2);
}

TEST(JsonTest, CompareTwoFieldsAllDifferent) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 123;
  obj2.get<kFieldName1>() = 43;
  obj2.get<kFieldName2>() = 123;
  EXPECT_TRUE(obj1 == obj1);
  EXPECT_FALSE(obj1 == obj2);
  EXPECT_FALSE(obj1 != obj1);
  EXPECT_TRUE(obj1 != obj2);
  EXPECT_FALSE(obj1 < obj1);
  EXPECT_TRUE(obj1 < obj2);
  EXPECT_TRUE(obj1 <= obj1);
  EXPECT_TRUE(obj1 <= obj2);
  EXPECT_FALSE(obj1 > obj1);
  EXPECT_FALSE(obj1 > obj2);
  EXPECT_TRUE(obj1 >= obj1);
  EXPECT_FALSE(obj1 >= obj2);
}

TEST(JsonTest, HashEmptyObject) {
  json::Object<> obj1, obj2;
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj1));
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj2));
}

TEST(JsonTest, HashOneField) {
  json::Object<json::Field<int, kFieldName1>> obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj2.get<kFieldName1>() = 43;
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj1));
  EXPECT_NE(absl::HashOf(obj1), absl::HashOf(obj2));
}

TEST(JsonTest, HashTwoFieldsAllEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 42;
  obj2.get<kFieldName2>() = 43;
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj1));
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj2));
}

TEST(JsonTest, HashTwoFieldsFirstEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 42;
  obj2.get<kFieldName2>() = 44;
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj1));
  EXPECT_NE(absl::HashOf(obj1), absl::HashOf(obj2));
}

TEST(JsonTest, HashTwoFieldsAllDifferent) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 44;
  obj2.get<kFieldName2>() = 45;
  EXPECT_EQ(absl::HashOf(obj1), absl::HashOf(obj1));
  EXPECT_NE(absl::HashOf(obj1), absl::HashOf(obj2));
}

TEST(JsonTest, FingerprintEmptyObject) {
  json::Object<> obj1, obj2;
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj1));
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj2));
}

TEST(JsonTest, FingerprintOneField) {
  json::Object<json::Field<int, kFieldName1>> obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj2.get<kFieldName1>() = 43;
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj1));
  EXPECT_NE(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj2));
}

TEST(JsonTest, FingerprintTwoFieldsAllEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 42;
  obj2.get<kFieldName2>() = 43;
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj1));
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj2));
}

TEST(JsonTest, FingerprintTwoFieldsFirstEqual) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 42;
  obj2.get<kFieldName2>() = 44;
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj1));
  EXPECT_NE(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj2));
}

TEST(JsonTest, FingerprintTwoFieldsAllDifferent) {
  json::Object<                       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>>
      obj1, obj2;
  obj1.get<kFieldName1>() = 42;
  obj1.get<kFieldName2>() = 43;
  obj2.get<kFieldName1>() = 44;
  obj2.get<kFieldName2>() = 45;
  EXPECT_EQ(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj1));
  EXPECT_NE(tsdb2::common::FingerprintOf(obj1), tsdb2::common::FingerprintOf(obj2));
}

TEST(JsonTest, ParseEmpty) {
  EXPECT_THAT(json::Parse<json::Object<>>("{", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>("{", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>("{", kParseOptions3), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>("{}", kParseOptions1));
  EXPECT_OK(json::Parse<json::Object<>>("{}", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>("{}", kParseOptions3));
  EXPECT_OK(json::Parse<json::Object<>>(" {}", kParseOptions1));
  EXPECT_OK(json::Parse<json::Object<>>(" {}", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(" {}", kParseOptions3));
  EXPECT_OK(json::Parse<json::Object<>>("{ }", kParseOptions1));
  EXPECT_OK(json::Parse<json::Object<>>("{ }", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>("{ }", kParseOptions3));
  EXPECT_OK(json::Parse<json::Object<>>("{} ", kParseOptions1));
  EXPECT_OK(json::Parse<json::Object<>>("{} ", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>("{} ", kParseOptions3));
  EXPECT_OK(json::Parse<json::Object<>>(" { } ", kParseOptions1));
  EXPECT_OK(json::Parse<json::Object<>>(" { } ", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(" { } ", kParseOptions3));
}

TEST(JsonTest, ParseEmptyWithExtraFields) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"lorem":"ipsum"})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"lorem":"ipsum"})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"lorem":"ipsum"})", kParseOptions3));
}

TEST(JsonTest, ParseEmptyWithOptionals) {
  using Object = json::Object<                         //
      json::Field<std::optional<int>, kFieldName1>,    //
      json::Field<std::unique_ptr<int>, kFieldName2>,  //
      json::Field<std::shared_ptr<int>, kFieldName3>   //
      >;
  EXPECT_THAT(json::Parse<Object>("{", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object>("{", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object>("{", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(
      json::Parse<Object>("{}", kParseOptions1),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{}", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{}", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" {}", kParseOptions1),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" {}", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" {}", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{ }", kParseOptions1),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{ }", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{ }", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{} ", kParseOptions1),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{} ", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>("{} ", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" { } ", kParseOptions1),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" { } ", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(" { } ", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
}

TEST(JsonTest, ParseExtraFieldsButNoOptionals) {
  using Object = json::Object<                         //
      json::Field<std::optional<int>, kFieldName1>,    //
      json::Field<std::unique_ptr<int>, kFieldName2>,  //
      json::Field<std::shared_ptr<int>, kFieldName3>   //
      >;
  EXPECT_THAT(json::Parse<Object>(R"({"foo":"bar"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(
      json::Parse<Object>(R"({"foo":"bar"})", kParseOptions2),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
  EXPECT_THAT(
      json::Parse<Object>(R"({"foo":"bar"})", kParseOptions3),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(std::nullopt), JsonField<kFieldName2>(IsNull()),
                         JsonField<kFieldName3>(IsNull()))));
}

TEST(JsonTest, ParseEmptyWithMissingFields) {
  using Object1 = json::Object<                      //
      json::Field<std::optional<int>, kFieldName1>,  //
      json::Field<int, kFieldName2>                  //
      >;
  using Object2 = json::Object<                     //
      json::Field<int, kFieldName1>,                //
      json::Field<std::optional<int>, kFieldName2>  //
      >;
  using Object3 = json::Object<       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>   //
      >;
  EXPECT_THAT(json::Parse<Object1>("{", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>("{}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>("{}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>("{}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>("{}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>("{}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>("{}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" {}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" {}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" {}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{ }", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{ }", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{ }", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{} ", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{} ", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{} ", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" { } ", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" { } ", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" { } ", kParseOptions3), Not(IsOk()));
}

TEST(JsonTest, ParseEmptyWithMissingAndExtraFields) {
  using Object1 = json::Object<                      //
      json::Field<std::optional<int>, kFieldName1>,  //
      json::Field<int, kFieldName2>                  //
      >;
  using Object2 = json::Object<                     //
      json::Field<int, kFieldName1>,                //
      json::Field<std::optional<int>, kFieldName2>  //
      >;
  using Object3 = json::Object<       //
      json::Field<int, kFieldName1>,  //
      json::Field<int, kFieldName2>   //
      >;
  EXPECT_THAT(json::Parse<Object1>(R"({"bar":"baz"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(R"({"bar":"baz"})", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(R"({"bar":"baz"})", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>(R"({"bar":"baz"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>(R"({"bar":"baz"})", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>(R"({"bar":"baz"})", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>(R"({"bar":"baz"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>(R"({"bar":"baz"})", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>(R"({"bar":"baz"})", kParseOptions3), Not(IsOk()));
}

TEST(JsonTest, StringifyEmpty) {
  json::Object<> object;
  EXPECT_EQ(object.Stringify(kStringifyOptions1), "{}");
  EXPECT_EQ(object.Stringify(kStringifyOptions2), "{}");
  EXPECT_EQ(object.Stringify(kStringifyOptions3), "{}");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions1), "{}");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions2), "{}");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions3), "{}");
}

TEST(JsonTest, Parse) {
  TestObject1 object;
  EXPECT_THAT(
      json::Parse<TestObject1>(
          R"({"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"],"elit":2.71})"),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                         JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                         JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                         JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                         JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                         JsonField<kFieldName8>(Optional(2.71)))));
  EXPECT_THAT(
      json::Parse<TestObject1>(
          R"({"lorem":43,"ipsum":false,"dolor":"barfoo","sit":14.3,"amet":[5,6,7],"consectetur":[1,2,3,4],"adipisci":[42,true,"bazbar"],"elit":71.2})"),
      IsOkAndHolds(AllOf(JsonField<kFieldName1>(43), JsonField<kFieldName2>(false),
                         JsonField<kFieldName3>("barfoo"), JsonField<kFieldName4>(14.3),
                         JsonField<kFieldName5>(ElementsAre(5, 6, 7)),
                         JsonField<kFieldName6>(ElementsAre(1, 2, 3, 4)),
                         JsonField<kFieldName7>(FieldsAre(42, true, "bazbar")),
                         JsonField<kFieldName8>(Optional(71.2)))));
  EXPECT_THAT(json::Parse<TestObject1>(R"json({
        "lorem": 42,
        "ipsum": true,
        "dolor": "foobar",
        "sit": 3.14,
        "amet": [1, 2, 3],
        "consectetur": [4, 5, 6, 7],
        "adipisci": [43, false, "barbaz"],
        "elit": 2.71
      })json"),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                                 JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                                 JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                                 JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                                 JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                                 JsonField<kFieldName8>(Optional(2.71)))));
}

TEST(JsonTest, UnorderedFields) {
  TestObject1 object;
  EXPECT_THAT(json::Parse<TestObject1>(R"json({
        "ipsum": true,
        "elit": 2.71,
        "adipisci": [43, false, "barbaz"],
        "consectetur": [4, 5, 6, 7],
        "amet": [1, 2, 3],
        "sit": 3.14,
        "dolor": "foobar",
        "lorem": 42
      })json"),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                                 JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                                 JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                                 JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                                 JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                                 JsonField<kFieldName8>(Optional(2.71)))));
}

TEST(JsonTest, SkipNull) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":null})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": null})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":null })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null })", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": null })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null })", kParseOptions3));
}

TEST(JsonTest, SkipBool) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":true})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": true})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":true })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true })", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": true })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true })", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":false})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": false})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":false })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false })", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": false })", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false })", kParseOptions3));
}

TEST(JsonTest, SkipString) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":""})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":""})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":""})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": ""})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": ""})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": ""})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(
                  R"({"foo":"a \" b \\ c / d \b e \f f \n g \r h \t i \u0042"})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(
      R"({"foo":"a \" b \\ c / d \b e \f f \n g \r h \t i \u0042"})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(
      R"({"foo":"a \" b \\ c / d \b e \f f \n g \r h \t i \u0042"})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\x"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\x"})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":"\x"})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\ugggg"})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\ugggg"})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":"\ugggg"})", kParseOptions3));
}

TEST(JsonTest, SkipObject) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": {}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{ }})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ }})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ }})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": { }})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { }})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { }})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar"}})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar"}})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar"}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":null}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": {"bar":null}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {"bar":null}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {"bar":null}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{ "bar":null}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ "bar":null}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ "bar":null}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar" :null}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar" :null}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar" :null}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar": null}})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar": null}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar": null}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":null }})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null }})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null }})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": { "bar" : null }})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { "bar" : null }})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { "bar" : null }})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":null,}})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":null,}})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null,}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false}})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true ,"baz":false}})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true ,"baz":false}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true ,"baz":false}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true, "baz":false}})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true, "baz":false}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true, "baz":false}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz" :false}})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz" :false}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz" :false}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz": false}})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz": false}})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz": false}})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false }})", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false }})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false }})", kParseOptions3));
  EXPECT_THAT(
      json::Parse<json::Object<>>(R"({"foo":{"bar":true , "baz" : false }})", kParseOptions1),
      Not(IsOk()));
  EXPECT_OK(
      json::Parse<json::Object<>>(R"({"foo":{"bar":true , "baz" : false }})", kParseOptions2));
  EXPECT_OK(
      json::Parse<json::Object<>>(R"({"foo":{"bar":true , "baz" : false }})", kParseOptions3));
  EXPECT_THAT(
      json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false,"qux":null}})", kParseOptions1),
      Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false,"qux":null}})",
                                        kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false,"qux":null}})",
                                        kParseOptions3));
}

TEST(JsonTest, SkipArray) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[})", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[})", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": []})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": []})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": []})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[ ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": [ ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": [1]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [1]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [1]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[ 1]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ 1]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ 1]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1 ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": [ 1 ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,]})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,]})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1 ,2]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ,2]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ,2]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1, 2]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1, 2]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1, 2]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2 ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2 ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2 ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 ]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 ]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 ]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2,]})", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2,]})", kParseOptions2), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2,]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2,3]})", kParseOptions1), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2,3]})", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2,3]})", kParseOptions3));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 , 3 ] })", kParseOptions1),
              Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 , 3 ] })", kParseOptions2));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 , 3 ] })", kParseOptions3));
}

TEST(JsonTest, ParseObjectWithExtraFields) {
  std::string_view constexpr input = R"json({
    "extra1": false,
    "ipsum": true,
    "extra": null,
    "elit": 2.71,
    "extra3": "foo \\ bar \"baz\"",
    "adipisci": [43, false, "barbaz"],
    "extra4": {
      "matrix": [
        [1, 0, 0, 0],
        [0, 1, 0, 0],
        [0, 0, 0, 1],
        [0, 0, 1, 0]
      ]
    },
    "consectetur": [4, 5, 6, 7],
    "extra5": [44, true, "bazqux"],
    "amet": [1, 2, 3],
    "extra6": [45, null, "quxfoo", {"foo": null}],
    "sit": 3.14,
    "extra7": -12.34e56,
    "dolor": "foobar",
    "lorem": 42
  })json";
  TestObject1 object;
  EXPECT_THAT(json::Parse<TestObject1>(input, kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TestObject1>(input, kParseOptions2),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                                 JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                                 JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                                 JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                                 JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                                 JsonField<kFieldName8>(Optional(2.71)))));
  EXPECT_THAT(json::Parse<TestObject1>(input, kParseOptions3),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(true),
                                 JsonField<kFieldName3>("foobar"), JsonField<kFieldName4>(3.14),
                                 JsonField<kFieldName5>(ElementsAre(1, 2, 3)),
                                 JsonField<kFieldName6>(ElementsAre(4, 5, 6, 7)),
                                 JsonField<kFieldName7>(FieldsAre(43, false, "barbaz")),
                                 JsonField<kFieldName8>(Optional(2.71)))));
}

TEST(JsonTest, Stringify) {
  TestObject1 object;
  object.get<kFieldName1>() = 42;
  object.get<kFieldName2>() = true;
  object.get<kFieldName3>() = "foobar";
  object.get<kFieldName4>() = 3.14;
  object.get<kFieldName5>() = {1, 2, 3};
  object.get<kFieldName6>() = {4, 5, 6, 7};
  object.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  object.get<kFieldName8>() = 2.71;
  EXPECT_EQ(
      object.Stringify(kStringifyOptions1),
      R"({"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"],"elit":2.71})");
  EXPECT_EQ(object.Stringify(kStringifyOptions2),
            R"({
  "lorem": 42,
  "ipsum": true,
  "dolor": "foobar",
  "sit": 3.14,
  "amet": [
    1,
    2,
    3
  ],
  "consectetur": [
    4,
    5,
    6,
    7
  ],
  "adipisci": [43, false, "barbaz"],
  "elit": 2.71
})");
  EXPECT_EQ(object.Stringify(kStringifyOptions3),
            R"({
    "lorem": 42,
    "ipsum": true,
    "dolor": "foobar",
    "sit": 3.14,
    "amet": [
        1,
        2,
        3
    ],
    "consectetur": [
        4,
        5,
        6,
        7
    ],
    "adipisci": [43, false, "barbaz"],
    "elit": 2.71
})");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions1), object.Stringify(kStringifyOptions1));
  EXPECT_EQ(json::Stringify(object, kStringifyOptions2), object.Stringify(kStringifyOptions2));
  EXPECT_EQ(json::Stringify(object, kStringifyOptions3), object.Stringify(kStringifyOptions3));
}

TEST(JsonTest, StringifyWithOptionalFields) {
  TestObject2 outer;
  auto& inner = outer.get<kFieldName2>();
  outer.get<kFieldName1>() = std::make_unique<std::string>("sator arepo");
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  inner.get<kFieldName8>() = 2.71;
  outer.get<kFieldName3>() = std::make_shared<std::string>("arepo tenet");
  outer.get<kFieldName4>() = {{"sator", 12}, {"arepo", 34}};
  outer.get<kFieldName5>() = std::make_pair(56, 78);
  EXPECT_EQ(
      outer.Stringify(kStringifyOptions1),
      R"({"lorem":"sator arepo","ipsum":{"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"],"elit":2.71},"dolor":"arepo tenet","sit":{"arepo":34,"sator":12},"amet":[56,78]})");
  EXPECT_EQ(outer.Stringify(kStringifyOptions2), R"({
  "lorem": "sator arepo",
  "ipsum": {
    "lorem": 42,
    "ipsum": true,
    "dolor": "foobar",
    "sit": 3.14,
    "amet": [
      1,
      2,
      3
    ],
    "consectetur": [
      4,
      5,
      6,
      7
    ],
    "adipisci": [43, false, "barbaz"],
    "elit": 2.71
  },
  "dolor": "arepo tenet",
  "sit": {
    "arepo": 34,
    "sator": 12
  },
  "amet": [56, 78]
})");
  EXPECT_EQ(outer.Stringify(kStringifyOptions3), R"({
    "lorem": "sator arepo",
    "ipsum": {
        "lorem": 42,
        "ipsum": true,
        "dolor": "foobar",
        "sit": 3.14,
        "amet": [
            1,
            2,
            3
        ],
        "consectetur": [
            4,
            5,
            6,
            7
        ],
        "adipisci": [43, false, "barbaz"],
        "elit": 2.71
    },
    "dolor": "arepo tenet",
    "sit": {
        "arepo": 34,
        "sator": 12
    },
    "amet": [56, 78]
})");
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions1), outer.Stringify(kStringifyOptions1));
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions2), outer.Stringify(kStringifyOptions2));
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions3), outer.Stringify(kStringifyOptions3));
}

TEST(JsonTest, StringifyWithMissingOptionalFields) {
  TestObject2 outer;
  auto& inner = outer.get<kFieldName2>();
  inner.get<kFieldName1>() = 42;
  inner.get<kFieldName2>() = true;
  inner.get<kFieldName3>() = "foobar";
  inner.get<kFieldName4>() = 3.14;
  inner.get<kFieldName5>() = {1, 2, 3};
  inner.get<kFieldName6>() = {4, 5, 6, 7};
  inner.get<kFieldName7>() = std::make_tuple(43, false, "barbaz");
  outer.get<kFieldName4>() = {{"sator", 12}, {"arepo", 34}};
  outer.get<kFieldName5>() = std::make_pair(56, 78);
  EXPECT_EQ(
      outer.Stringify(kStringifyOptions1),
      R"({"ipsum":{"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"]},"sit":{"arepo":34,"sator":12},"amet":[56,78]})");
  EXPECT_EQ(outer.Stringify(kStringifyOptions2), R"({
  "ipsum": {
    "lorem": 42,
    "ipsum": true,
    "dolor": "foobar",
    "sit": 3.14,
    "amet": [
      1,
      2,
      3
    ],
    "consectetur": [
      4,
      5,
      6,
      7
    ],
    "adipisci": [43, false, "barbaz"]
  },
  "sit": {
    "arepo": 34,
    "sator": 12
  },
  "amet": [56, 78]
})");
  EXPECT_EQ(outer.Stringify(kStringifyOptions3), R"({
    "ipsum": {
        "lorem": 42,
        "ipsum": true,
        "dolor": "foobar",
        "sit": 3.14,
        "amet": [
            1,
            2,
            3
        ],
        "consectetur": [
            4,
            5,
            6,
            7
        ],
        "adipisci": [43, false, "barbaz"]
    },
    "sit": {
        "arepo": 34,
        "sator": 12
    },
    "amet": [56, 78]
})");
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions1), outer.Stringify(kStringifyOptions1));
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions2), outer.Stringify(kStringifyOptions2));
  EXPECT_EQ(json::Stringify(outer, kStringifyOptions3), outer.Stringify(kStringifyOptions3));
}

TEST(JsonTest, ParseBool) {
  EXPECT_THAT(json::Parse<bool>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<bool>(" "), Not(IsOk()));
  EXPECT_THAT(json::Parse<bool>("true"), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>(" true"), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>("true "), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>(" true "), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>("truesuffix"), Not(IsOk()));
  EXPECT_THAT(json::Parse<bool>("false"), IsOkAndHolds(false));
  EXPECT_THAT(json::Parse<bool>(" false"), IsOkAndHolds(false));
  EXPECT_THAT(json::Parse<bool>("falsesuffix"), Not(IsOk()));
  EXPECT_THAT(json::Parse<bool>("prefixtrue"), Not(IsOk()));
  EXPECT_THAT(json::Parse<bool>("prefixfalse"), Not(IsOk()));
}

TEST(JsonTest, StringifyBool) {
  EXPECT_EQ(json::Stringify(true, kStringifyOptions1), "true");
  EXPECT_EQ(json::Stringify(true, kStringifyOptions2), "true");
  EXPECT_EQ(json::Stringify(true, kStringifyOptions3), "true");
  EXPECT_EQ(json::Stringify(false, kStringifyOptions1), "false");
  EXPECT_EQ(json::Stringify(false, kStringifyOptions2), "false");
  EXPECT_EQ(json::Stringify(false, kStringifyOptions3), "false");
}

TEST(JsonTest, SkipWhitespace) {
  EXPECT_THAT(json::Parse<bool>(" \r\n\ttrue"), IsOkAndHolds(true));
}

TEST(JsonTest, ParseUnsignedInteger) {
  EXPECT_THAT(json::Parse<unsigned int>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<unsigned int>(" "), Not(IsOk()));
  EXPECT_THAT(json::Parse<unsigned int>("-3"), Not(IsOk()));
  EXPECT_THAT(json::Parse<unsigned int>("abc"), Not(IsOk()));
  EXPECT_THAT(json::Parse<unsigned int>("0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<unsigned int>(" 0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<unsigned int>("0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<unsigned int>(" 0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<unsigned int>("03"), Not(IsOk()));
  EXPECT_THAT(json::Parse<unsigned int>("314"), IsOkAndHolds(314));
  EXPECT_THAT(json::Parse<unsigned int>(" 314"), IsOkAndHolds(314));
  EXPECT_THAT(json::Parse<unsigned int>("314 "), IsOkAndHolds(314));
  EXPECT_THAT(json::Parse<unsigned int>(" 314 "), IsOkAndHolds(314));
}

TEST(JsonTest, StringifyUnsignedInteger) {
  EXPECT_EQ(json::Stringify<uint8_t>(42, kStringifyOptions1), "42");
  EXPECT_EQ(json::Stringify<uint8_t>(42, kStringifyOptions2), "42");
  EXPECT_EQ(json::Stringify<uint8_t>(42, kStringifyOptions3), "42");
  EXPECT_EQ(json::Stringify<uint16_t>(43, kStringifyOptions1), "43");
  EXPECT_EQ(json::Stringify<uint16_t>(43, kStringifyOptions2), "43");
  EXPECT_EQ(json::Stringify<uint16_t>(43, kStringifyOptions3), "43");
  EXPECT_EQ(json::Stringify<uint32_t>(44, kStringifyOptions1), "44");
  EXPECT_EQ(json::Stringify<uint32_t>(44, kStringifyOptions2), "44");
  EXPECT_EQ(json::Stringify<uint32_t>(44, kStringifyOptions3), "44");
  EXPECT_EQ(json::Stringify<uint64_t>(45, kStringifyOptions1), "45");
  EXPECT_EQ(json::Stringify<uint64_t>(45, kStringifyOptions2), "45");
  EXPECT_EQ(json::Stringify<uint64_t>(45, kStringifyOptions3), "45");
}

TEST(JsonTest, ParseSignedInteger) {
  EXPECT_THAT(json::Parse<signed int>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<signed int>(" "), Not(IsOk()));
  EXPECT_THAT(json::Parse<signed int>("abc"), Not(IsOk()));
  EXPECT_THAT(json::Parse<signed int>("0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<signed int>(" 0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<signed int>("0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<signed int>(" 0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<signed int>("-0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<signed int>("02"), Not(IsOk()));
  EXPECT_THAT(json::Parse<signed int>("271"), IsOkAndHolds(271));
  EXPECT_THAT(json::Parse<signed int>(" 271"), IsOkAndHolds(271));
  EXPECT_THAT(json::Parse<signed int>("271 "), IsOkAndHolds(271));
  EXPECT_THAT(json::Parse<signed int>(" 271 "), IsOkAndHolds(271));
  EXPECT_THAT(json::Parse<signed int>("-271"), IsOkAndHolds(-271));
  EXPECT_THAT(json::Parse<signed int>(" -271"), IsOkAndHolds(-271));
  EXPECT_THAT(json::Parse<signed int>("-271 "), IsOkAndHolds(-271));
  EXPECT_THAT(json::Parse<signed int>(" -271 "), IsOkAndHolds(-271));
  EXPECT_THAT(json::Parse<signed int>("- 271"), Not(IsOk()));
}

TEST(JsonTest, StringifySignedInteger) {
  EXPECT_EQ(json::Stringify<int8_t>(42, kStringifyOptions1), "42");
  EXPECT_EQ(json::Stringify<int8_t>(42, kStringifyOptions2), "42");
  EXPECT_EQ(json::Stringify<int8_t>(42, kStringifyOptions3), "42");
  EXPECT_EQ(json::Stringify<int16_t>(43, kStringifyOptions1), "43");
  EXPECT_EQ(json::Stringify<int16_t>(43, kStringifyOptions2), "43");
  EXPECT_EQ(json::Stringify<int16_t>(43, kStringifyOptions3), "43");
  EXPECT_EQ(json::Stringify<int32_t>(44, kStringifyOptions1), "44");
  EXPECT_EQ(json::Stringify<int32_t>(44, kStringifyOptions2), "44");
  EXPECT_EQ(json::Stringify<int32_t>(44, kStringifyOptions3), "44");
  EXPECT_EQ(json::Stringify<int64_t>(45, kStringifyOptions1), "45");
  EXPECT_EQ(json::Stringify<int64_t>(45, kStringifyOptions2), "45");
  EXPECT_EQ(json::Stringify<int64_t>(45, kStringifyOptions3), "45");
  EXPECT_EQ(json::Stringify<int8_t>(-46, kStringifyOptions1), "-46");
  EXPECT_EQ(json::Stringify<int8_t>(-46, kStringifyOptions2), "-46");
  EXPECT_EQ(json::Stringify<int8_t>(-46, kStringifyOptions3), "-46");
  EXPECT_EQ(json::Stringify<int16_t>(-47, kStringifyOptions1), "-47");
  EXPECT_EQ(json::Stringify<int16_t>(-47, kStringifyOptions2), "-47");
  EXPECT_EQ(json::Stringify<int16_t>(-47, kStringifyOptions3), "-47");
  EXPECT_EQ(json::Stringify<int32_t>(-48, kStringifyOptions1), "-48");
  EXPECT_EQ(json::Stringify<int32_t>(-48, kStringifyOptions2), "-48");
  EXPECT_EQ(json::Stringify<int32_t>(-48, kStringifyOptions3), "-48");
  EXPECT_EQ(json::Stringify<int64_t>(-49, kStringifyOptions1), "-49");
  EXPECT_EQ(json::Stringify<int64_t>(-49, kStringifyOptions2), "-49");
  EXPECT_EQ(json::Stringify<int64_t>(-49, kStringifyOptions3), "-49");
}

TEST(JsonTest, ParseFloat) {
  EXPECT_THAT(json::Parse<double>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>(" "), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("abc"), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<double>(" 0"), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<double>("0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<double>(" 0 "), IsOkAndHolds(0));
  EXPECT_THAT(json::Parse<double>("-0"), IsOkAndHolds(-0.0));
  EXPECT_THAT(json::Parse<double>("123"), IsOkAndHolds(123));
  EXPECT_THAT(json::Parse<double>("-123"), IsOkAndHolds(-123));
  EXPECT_THAT(json::Parse<double>("- 123"), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("123."), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("123.e+12"), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("123.456"), IsOkAndHolds(123.456));
  EXPECT_THAT(json::Parse<double>("-123.456"), IsOkAndHolds(-123.456));
  EXPECT_THAT(json::Parse<double>(".456"), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("-.456"), Not(IsOk()));
  EXPECT_THAT(json::Parse<double>("123456000000000e-12"), IsOkAndHolds(123.456));
  EXPECT_THAT(json::Parse<double>("123456000000000E-12"), IsOkAndHolds(123.456));
  EXPECT_THAT(json::Parse<double>("-123456000000000e-12"), IsOkAndHolds(-123.456));
  EXPECT_THAT(json::Parse<double>("-123456000000000E-12"), IsOkAndHolds(-123.456));
  EXPECT_THAT(json::Parse<double>("123.456e+12"), IsOkAndHolds(123456000000000));
  EXPECT_THAT(json::Parse<double>("123.456E+12"), IsOkAndHolds(123456000000000));
  EXPECT_THAT(json::Parse<double>("-123.456e+12"), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>("-123.456E+12"), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>("123.456e12"), IsOkAndHolds(123456000000000));
  EXPECT_THAT(json::Parse<double>("123.456E12"), IsOkAndHolds(123456000000000));
  EXPECT_THAT(json::Parse<double>("-123.456e12"), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>("-123.456E12"), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>(" -123.456e+12"), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>("-123.456e+12 "), IsOkAndHolds(-123456000000000));
  EXPECT_THAT(json::Parse<double>(" -123.456e+12 "), IsOkAndHolds(-123456000000000));
}

TEST(JsonTest, StringifyFloat) {
  EXPECT_EQ(json::Stringify<float>(3.14, kStringifyOptions1), "3.14");
  EXPECT_EQ(json::Stringify<float>(3.14, kStringifyOptions2), "3.14");
  EXPECT_EQ(json::Stringify<float>(3.14, kStringifyOptions3), "3.14");
  EXPECT_EQ(json::Stringify<float>(-3.14, kStringifyOptions1), "-3.14");
  EXPECT_EQ(json::Stringify<float>(-3.14, kStringifyOptions2), "-3.14");
  EXPECT_EQ(json::Stringify<float>(-3.14, kStringifyOptions3), "-3.14");
  EXPECT_EQ(json::Stringify<double>(2.71, kStringifyOptions1), "2.71");
  EXPECT_EQ(json::Stringify<double>(2.71, kStringifyOptions2), "2.71");
  EXPECT_EQ(json::Stringify<double>(2.71, kStringifyOptions3), "2.71");
  EXPECT_EQ(json::Stringify<double>(-2.71, kStringifyOptions1), "-2.71");
  EXPECT_EQ(json::Stringify<double>(-2.71, kStringifyOptions2), "-2.71");
  EXPECT_EQ(json::Stringify<double>(-2.71, kStringifyOptions3), "-2.71");
  // TODO: debug these two.
  // EXPECT_EQ(json::Stringify<long double>(1.41), "1.41");
  // EXPECT_EQ(json::Stringify<long double>(-1.41), "-1.41");
}

TEST(JsonTest, ParseString) {
  EXPECT_THAT(json::Parse<std::string>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<std::string>(" "), Not(IsOk()));
  EXPECT_THAT(json::Parse<std::string>("\""), Not(IsOk()));
  EXPECT_THAT(json::Parse<std::string>("\"\""), IsOkAndHolds(""));
  EXPECT_THAT(json::Parse<std::string>("\"lorem ipsum\""), IsOkAndHolds("lorem ipsum"));
  EXPECT_THAT(json::Parse<std::string>("\"lorem \\\"ipsum\\\"\""), IsOkAndHolds("lorem \"ipsum\""));
  EXPECT_THAT(
      json::Parse<std::string>("\"a \\\" b \\\\ c \\/ d \\b e \\f f \\n g \\r h \\t i \\u0042\""),
      IsOkAndHolds("a \" b \\ c / d \b e \f f \n g \r h \t i \u0042"));
  EXPECT_THAT(json::Parse<std::string>(" \"lorem \\\"ipsum\\\"\""),
              IsOkAndHolds("lorem \"ipsum\""));
  EXPECT_THAT(json::Parse<std::string>("\"lorem \\\"ipsum\\\"\" "),
              IsOkAndHolds("lorem \"ipsum\""));
  EXPECT_THAT(json::Parse<std::string>(" \"lorem \\\"ipsum\\\"\" "),
              IsOkAndHolds("lorem \"ipsum\""));
}

TEST(JsonTest, StringifyString) {
  EXPECT_EQ(json::Stringify<std::string>("lorem \"ipsum\"", kStringifyOptions1),
            "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify<std::string>("lorem \"ipsum\"", kStringifyOptions2),
            "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify<std::string>("lorem \"ipsum\"", kStringifyOptions3),
            "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("lorem \"ipsum\"", kStringifyOptions1), "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("lorem \"ipsum\"", kStringifyOptions2), "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("lorem \"ipsum\"", kStringifyOptions3), "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("a \" b \\ c / d \b e \f f \n g \r h \t i \x84", kStringifyOptions1),
            "\"a \\\" b \\\\ c / d \\b e \\f f \\n g \\r h \\t i \\u0084\"");
  EXPECT_EQ(json::Stringify("a \" b \\ c / d \b e \f f \n g \r h \t i \x84", kStringifyOptions2),
            "\"a \\\" b \\\\ c / d \\b e \\f f \\n g \\r h \\t i \\u0084\"");
  EXPECT_EQ(json::Stringify("a \" b \\ c / d \b e \f f \n g \r h \t i \x84", kStringifyOptions3),
            "\"a \\\" b \\\\ c / d \\b e \\f f \\n g \\r h \\t i \\u0084\"");
}

TEST(JsonTest, ParseOptional) {
  EXPECT_THAT(json::Parse<std::optional<std::string>>("null"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(json::Parse<std::optional<std::string>>(" null"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(json::Parse<std::optional<std::string>>("null "), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(json::Parse<std::optional<std::string>>(" null "), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(json::Parse<std::optional<bool>>("null"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(json::Parse<std::optional<std::string>>("\"lorem \\\"ipsum\\\"\""),
              IsOkAndHolds(Optional<std::string>("lorem \"ipsum\"")));
  EXPECT_THAT(json::Parse<std::optional<bool>>("true"), IsOkAndHolds(Optional(true)));
  EXPECT_THAT(json::Parse<std::optional<bool>>(" true"), IsOkAndHolds(Optional(true)));
  EXPECT_THAT(json::Parse<std::optional<bool>>("true "), IsOkAndHolds(Optional(true)));
  EXPECT_THAT(json::Parse<std::optional<bool>>(" true "), IsOkAndHolds(Optional(true)));
}

TEST(JsonTest, StringifyOptional) {
  EXPECT_EQ(json::Stringify<std::optional<int>>(std::nullopt, kStringifyOptions1), "null");
  EXPECT_EQ(json::Stringify<std::optional<int>>(std::nullopt, kStringifyOptions2), "null");
  EXPECT_EQ(json::Stringify<std::optional<int>>(std::nullopt, kStringifyOptions3), "null");
  EXPECT_EQ(json::Stringify<std::optional<int>>(42, kStringifyOptions1), "42");
  EXPECT_EQ(json::Stringify<std::optional<int>>(42, kStringifyOptions2), "42");
  EXPECT_EQ(json::Stringify<std::optional<int>>(42, kStringifyOptions3), "42");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>(std::nullopt, kStringifyOptions1), "null");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>(std::nullopt, kStringifyOptions2), "null");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>(std::nullopt, kStringifyOptions3), "null");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>("lorem", kStringifyOptions1), "\"lorem\"");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>("lorem", kStringifyOptions2), "\"lorem\"");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>("lorem", kStringifyOptions3), "\"lorem\"");
}

TEST(JsonTest, ParsePair) {
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("42")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[\"lorem ipsum\"]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42,]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42,\"lorem \\\"ipsum\\\"\"]")),
              IsOkAndHolds(Pair(42, "lorem \"ipsum\"")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[\"lorem \\\"ipsum\\\"\",42]")),
              Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<std::string, int>>("[\"dolor \\\"amet\\\"\", -43]")),
              IsOkAndHolds(Pair("dolor \"amet\"", -43)));
  EXPECT_THAT((json::Parse<std::pair<std::string, int>>("[\"dolor \\\"amet\\\"\", - 43]")),
              Not(IsOk()));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>(" [42,\"lorem\"]")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[ 42,\"lorem\"]")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42 ,\"lorem\"]")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42, \"lorem\"]")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42,\"lorem\" ]")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>("[42,\"lorem\"] ")),
              IsOkAndHolds(Pair(42, "lorem")));
  EXPECT_THAT((json::Parse<std::pair<int, std::string>>(" [ 42 , \"lorem\" ] ")),
              IsOkAndHolds(Pair(42, "lorem")));
}

TEST(JsonTest, StringifyPair) {
  EXPECT_EQ(json::Stringify(std::make_pair(42, "lorem"), kStringifyOptions1), "[42,\"lorem\"]");
  EXPECT_EQ(json::Stringify(std::make_pair(42, "lorem"), kStringifyOptions2), "[42, \"lorem\"]");
  EXPECT_EQ(json::Stringify(std::make_pair(42, "lorem"), kStringifyOptions3), "[42, \"lorem\"]");
  EXPECT_EQ(json::Stringify(std::make_pair("ipsum", 43), kStringifyOptions1), "[\"ipsum\",43]");
  EXPECT_EQ(json::Stringify(std::make_pair("ipsum", 43), kStringifyOptions2), "[\"ipsum\", 43]");
  EXPECT_EQ(json::Stringify(std::make_pair("ipsum", 43), kStringifyOptions3), "[\"ipsum\", 43]");
}

TEST(JsonTest, ParseTuple) {
  EXPECT_THAT((json::Parse<std::tuple<>>("")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<>>("[")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<>>("[]")), IsOkAndHolds(std::make_tuple()));
  EXPECT_THAT((json::Parse<std::tuple<>>(" []")), IsOkAndHolds(std::make_tuple()));
  EXPECT_THAT((json::Parse<std::tuple<>>("[ ]")), IsOkAndHolds(std::make_tuple()));
  EXPECT_THAT((json::Parse<std::tuple<>>("[] ")), IsOkAndHolds(std::make_tuple()));
  EXPECT_THAT((json::Parse<std::tuple<>>(" [ ] ")), IsOkAndHolds(std::make_tuple()));
  EXPECT_THAT((json::Parse<std::tuple<int>>("")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[42]")), IsOkAndHolds(FieldsAre(42)));
  EXPECT_THAT((json::Parse<std::tuple<int>>(" [43]")), IsOkAndHolds(FieldsAre(43)));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[ 44]")), IsOkAndHolds(FieldsAre(44)));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[45 ]")), IsOkAndHolds(FieldsAre(45)));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[46] ")), IsOkAndHolds(FieldsAre(46)));
  EXPECT_THAT((json::Parse<std::tuple<int>>(" [ 47 ] ")), IsOkAndHolds(FieldsAre(47)));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[-48]")), IsOkAndHolds(FieldsAre(-48)));
  EXPECT_THAT((json::Parse<std::tuple<int>>("[- 48]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\"]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",42]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<int, std::string>>("[43,\"ipsum\"]")),
              IsOkAndHolds(FieldsAre(43, "ipsum")));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",42,")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",42,]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>(" [\"lorem\",42]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[ \"lorem\",42]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\" ,42]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\", 42]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",42 ]")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>("[\"lorem\",42] ")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<std::string, int>>(" [ \"lorem\" , 42 ] ")),
              IsOkAndHolds(FieldsAre("lorem", 42)));
  EXPECT_THAT((json::Parse<std::tuple<bool, int, std::string, int>>("[true, 42, \"lorem\", 43]")),
              IsOkAndHolds(FieldsAre(true, 42, "lorem", 43)));
  EXPECT_THAT((json::Parse<std::tuple<bool, int, std::string, int>>("[false, 43, \"ipsum\", 42]")),
              IsOkAndHolds(FieldsAre(false, 43, "ipsum", 42)));
}

TEST(JsonTest, StringifyTuple) {
  EXPECT_EQ(json::Stringify(std::make_tuple(), kStringifyOptions1), "[]");
  EXPECT_EQ(json::Stringify(std::make_tuple(), kStringifyOptions2), "[]");
  EXPECT_EQ(json::Stringify(std::make_tuple(), kStringifyOptions3), "[]");
  EXPECT_EQ(json::Stringify(std::make_tuple(42), kStringifyOptions1), "[42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(42), kStringifyOptions2), "[42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(42), kStringifyOptions3), "[42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42), kStringifyOptions1), "[true,42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42), kStringifyOptions2), "[true, 42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42), kStringifyOptions3), "[true, 42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, "lorem", 42), kStringifyOptions1),
            "[true,\"lorem\",42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, "lorem", 42), kStringifyOptions2),
            "[true, \"lorem\", 42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, "lorem", 42), kStringifyOptions3),
            "[true, \"lorem\", 42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42, "lorem", 43), kStringifyOptions1),
            "[true,42,\"lorem\",43]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42, "lorem", 43), kStringifyOptions2),
            "[true, 42, \"lorem\", 43]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42, "lorem", 43), kStringifyOptions3),
            "[true, 42, \"lorem\", 43]");
  EXPECT_EQ(json::Stringify(std::make_tuple(false, 43, "ipsum", 42), kStringifyOptions1),
            "[false,43,\"ipsum\",42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(false, 43, "ipsum", 42), kStringifyOptions2),
            "[false, 43, \"ipsum\", 42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(false, 43, "ipsum", 42), kStringifyOptions3),
            "[false, 43, \"ipsum\", 42]");
}

TEST(JsonTest, ParseStdArray) {
  EXPECT_THAT((json::Parse<std::array<int, 4>>("")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[42]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[42,]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[42,43]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[42,43,]")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[1,2,3,4]")),
              IsOkAndHolds(ElementsAre(1, 2, 3, 4)));
  EXPECT_THAT((json::Parse<std::array<int, 4>>(" [ 1 , 2 , 3 , 4 ] ")),
              IsOkAndHolds(ElementsAre(1, 2, 3, 4)));
  EXPECT_THAT((json::Parse<std::array<int, 4>>("[44,-75,93,43]")),
              IsOkAndHolds(ElementsAre(44, -75, 93, 43)));
  EXPECT_THAT((json::Parse<std::array<int, 4>>(" [ 44 , 75 , -93 , 43 ] ")),
              IsOkAndHolds(ElementsAre(44, 75, -93, 43)));
  EXPECT_THAT((json::Parse<std::array<int, 4>>(" [ 44 , 75 , - 93 , 43 ] ")), Not(IsOk()));
  EXPECT_THAT((json::Parse<std::array<int, 3>>("[3,2,1]")), IsOkAndHolds(ElementsAre(3, 2, 1)));
  EXPECT_THAT((json::Parse<std::array<int, 3>>(" [ 3 , 2 , 1 ] ")),
              IsOkAndHolds(ElementsAre(3, 2, 1)));
}

TEST(JsonTest, StringifyStdArray) {
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({1, 2, 3, 4}, kStringifyOptions1)),
            R"([1,2,3,4])");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({1, 2, 3, 4}, kStringifyOptions2)), R"([
  1,
  2,
  3,
  4
])");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({1, 2, 3, 4}, kStringifyOptions3)), R"([
    1,
    2,
    3,
    4
])");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({44, -75, 93, 43}, kStringifyOptions1)),
            R"([44,-75,93,43])");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({44, -75, 93, 43}, kStringifyOptions2)), R"([
  44,
  -75,
  93,
  43
])");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({44, -75, 93, 43}, kStringifyOptions3)), R"([
    44,
    -75,
    93,
    43
])");
  EXPECT_EQ((json::Stringify<std::array<int, 3>>({75, 44, -93}, kStringifyOptions1)),
            R"([75,44,-93])");
  EXPECT_EQ((json::Stringify<std::array<int, 3>>({75, 44, -93}, kStringifyOptions2)), R"([
  75,
  44,
  -93
])");
  EXPECT_EQ((json::Stringify<std::array<int, 3>>({75, 44, -93}, kStringifyOptions3)), R"([
    75,
    44,
    -93
])");
}

class Point {
 private:
  static char constexpr kPointXField[] = "x";
  static char constexpr kPointYField[] = "y";

  using JsonPoint =
      json::Object<json::Field<double, kPointXField>, json::Field<double, kPointYField>>;

 public:
  explicit Point() : x_(0), y_(0) {}
  explicit Point(double const x, double const y) : x_(x), y_(y) {}

  void Ref() const { ++ref_count_; }
  void Unref() const { --ref_count_; }

  double x() const { return x_; }
  double y() const { return y_; }

  friend absl::Status Tsdb2JsonParse(json::Parser* const parser, Point* const point) {
    DEFINE_CONST_OR_RETURN(obj, (parser->ReadObject<JsonPoint>()));
    point->x_ = obj.get<kPointXField>();
    point->y_ = obj.get<kPointYField>();
    return absl::OkStatus();
  }

  friend void Tsdb2JsonStringify(json::Stringifier* const stringifier, Point const& point) {
    stringifier->WriteObject(JsonPoint{json::kInitialize, point.x_, point.y_});
  }

 private:
  intptr_t mutable ref_count_ = 0;
  double x_;
  double y_;
};

TEST(JsonTest, ParseCustomObject) {
  std::string_view constexpr input = R"json({
    "x": 12.34,
    "y": 56.78
  })json";
  EXPECT_THAT(tsdb2::json::Parse<Point>(input),
              IsOkAndHolds(AllOf(Property(&Point::x, 12.34), Property(&Point::y, 56.78))));
}

TEST(JsonTest, ParseCustomObjectWithExtraFields) {
  std::string_view constexpr input = R"json({
    "lorem": "ipsum",
    "x": 34.12,
    "dolor": 42,
    "y": 78.56,
    "amet": false
  })json";
  EXPECT_THAT(json::Parse<Point>(input, kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<Point>(input, kParseOptions2),
              IsOkAndHolds(AllOf(Property(&Point::x, 34.12), Property(&Point::y, 78.56))));
  EXPECT_THAT(json::Parse<Point>(input, kParseOptions3),
              IsOkAndHolds(AllOf(Property(&Point::x, 34.12), Property(&Point::y, 78.56))));
}

TEST(JsonTest, StringifyCustomObject) {
  Point value{12.34, 56.78};
  EXPECT_EQ(json::Stringify(value, kStringifyOptions1), R"({"x":12.34,"y":56.78})");
  EXPECT_EQ(json::Stringify(value, kStringifyOptions2), R"({
  "x": 12.34,
  "y": 56.78
})");
  EXPECT_EQ(json::Stringify(value, kStringifyOptions3), R"({
    "x": 12.34,
    "y": 56.78
})");
}

using TestObjectWithReffedPtr =
    json::Object<json::Field<int, kFieldName1>,
                 json::Field<tsdb2::common::reffed_ptr<Point>, kFieldName2>,
                 json::Field<bool, kFieldName3>>;

TEST(JsonTest, ParseReffedPtr) {
  std::string_view const input = R"json({
    "lorem": 42,
    "ipsum": {
      "x": 123.456,
      "y": 654.321
    },
    "dolor": false
  })json";
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions1),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42),
                                 JsonField<kFieldName2>(Pointee2(AllOf(
                                     Property(&Point::x, 123.456), Property(&Point::y, 654.321)))),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions2),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42),
                                 JsonField<kFieldName2>(Pointee2(AllOf(
                                     Property(&Point::x, 123.456), Property(&Point::y, 654.321)))),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions3),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42),
                                 JsonField<kFieldName2>(Pointee2(AllOf(
                                     Property(&Point::x, 123.456), Property(&Point::y, 654.321)))),
                                 JsonField<kFieldName3>(false))));
}

TEST(JsonTest, ParseNullReffedPtr) {
  std::string_view const input = R"json({
    "lorem": 42,
    "ipsum": null,
    "dolor": false
  })json";
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions1),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions2),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions3),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
}

TEST(JsonTest, ParseMissingReffedPtr) {
  std::string_view const input = R"json({
    "lorem": 42,
    "dolor": false
  })json";
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions1),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions2),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
  EXPECT_THAT(json::Parse<TestObjectWithReffedPtr>(input, kParseOptions3),
              IsOkAndHolds(AllOf(JsonField<kFieldName1>(42), JsonField<kFieldName2>(nullptr),
                                 JsonField<kFieldName3>(false))));
}

TEST(JsonTest, StringifyReffedPtr) {
  Point point{12.12, 34.34};
  TestObjectWithReffedPtr object{json::kInitialize, 43, tsdb2::common::WrapReffed(&point), true};
  EXPECT_EQ(json::Stringify(object, kStringifyOptions1),
            R"({"lorem":43,"ipsum":{"x":12.12,"y":34.34},"dolor":true})");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions2), R"({
  "lorem": 43,
  "ipsum": {
    "x": 12.12,
    "y": 34.34
  },
  "dolor": true
})");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions3), R"({
    "lorem": 43,
    "ipsum": {
        "x": 12.12,
        "y": 34.34
    },
    "dolor": true
})");
}

TEST(JsonTest, StringifyMissingReffedPtr) {
  TestObjectWithReffedPtr object{json::kInitialize, 43, nullptr, true};
  EXPECT_EQ(json::Stringify(object, kStringifyOptions1), R"({"lorem":43,"dolor":true})");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions2), R"({
  "lorem": 43,
  "dolor": true
})");
  EXPECT_EQ(json::Stringify(object, kStringifyOptions3), R"({
    "lorem": 43,
    "dolor": true
})");
}

template <typename Sequence>
struct MaybeOrderedElementsAre;

template <typename Element>
struct MaybeOrderedElementsAre<std::vector<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return ElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<std::set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return ElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<std::unordered_set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return UnorderedElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<absl::btree_set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return ElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<absl::flat_hash_set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return UnorderedElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<absl::node_hash_set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return UnorderedElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Element>
struct MaybeOrderedElementsAre<tsdb2::common::flat_set<Element>> {
  template <typename... Args>
  auto operator()(Args&&... args) const {
    return ElementsAre(std::forward<Args>(args)...);
  }
};

template <typename Value>
class TypedJsonSequenceTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(TypedJsonSequenceTest);

TYPED_TEST_P(TypedJsonSequenceTest, ParseSequence) {
  auto const elements_are = MaybeOrderedElementsAre<TypeParam>();
  EXPECT_THAT(json::Parse<TypeParam>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("["), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[]"), IsOkAndHolds(elements_are()));
  EXPECT_THAT(json::Parse<TypeParam>(" []"), IsOkAndHolds(elements_are()));
  EXPECT_THAT(json::Parse<TypeParam>("[ ]"), IsOkAndHolds(elements_are()));
  EXPECT_THAT(json::Parse<TypeParam>("[] "), IsOkAndHolds(elements_are()));
  EXPECT_THAT(json::Parse<TypeParam>(" [ ] "), IsOkAndHolds(elements_are()));
  EXPECT_THAT(json::Parse<TypeParam>("[,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42]"), IsOkAndHolds(elements_are(42)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[,42]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>(" [42,43]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[ 42,43]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42 ,43]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42, 43]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43 ]"), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43] "), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>(" [ 42 , 43 ] "), IsOkAndHolds(elements_are(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[-42,43]"), IsOkAndHolds(elements_are(-42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,- 43]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43,44]"), IsOkAndHolds(elements_are(42, 43, 44)));
  EXPECT_THAT(json::Parse<TypeParam>(" [ 42 , 43 , 44 ] "), IsOkAndHolds(elements_are(42, 43, 44)));
}

TYPED_TEST_P(TypedJsonSequenceTest, StringifySequence) {
  EXPECT_EQ(json::Stringify<TypeParam>({}, kStringifyOptions1), "[]");
  EXPECT_EQ(json::Stringify<TypeParam>({}, kStringifyOptions2), "[]");
  EXPECT_EQ(json::Stringify<TypeParam>({}, kStringifyOptions3), "[]");
  EXPECT_EQ(json::Stringify<TypeParam>({42}, kStringifyOptions1), R"([42])");
  EXPECT_EQ(json::Stringify<TypeParam>({42}, kStringifyOptions2), R"([
  42
])");
  EXPECT_EQ(json::Stringify<TypeParam>({42}, kStringifyOptions3), R"([
    42
])");
  EXPECT_THAT(json::Stringify<TypeParam>({42, 43}, kStringifyOptions1),
              AnyOf(R"([42,43])", R"([43,42])"));
  EXPECT_THAT(json::Stringify<TypeParam>({42, 43}, kStringifyOptions2), AnyOf(R"([
  42,
  43
])",
                                                                              R"([
  43,
  42
])"));
  EXPECT_THAT(json::Stringify<TypeParam>({42, 43}, kStringifyOptions3), AnyOf(R"([
    42,
    43
])",
                                                                              R"([
    43,
    42
])"));
  EXPECT_THAT(json::Stringify<TypeParam>({-75, 44, 93}, kStringifyOptions1),
              AnyOf(R"([-75,44,93])", R"([-75,93,44])", R"([44,-75,93])", R"([44,93,-75])",
                    R"([93,-75,44])", R"([93,44,-75])"));
  EXPECT_THAT(json::Stringify<TypeParam>({-75, 44, 93}, kStringifyOptions2), AnyOf(R"([
  -75,
  44,
  93
])",
                                                                                   R"([
  -75,
  93,
  44
])",
                                                                                   R"([
  44,
  -75,
  93
])",
                                                                                   R"([
  44,
  93,
  -75
])",
                                                                                   R"([
  93,
  -75,
  44
])",
                                                                                   R"([
  93,
  44,
  -75
])"));
  EXPECT_THAT(json::Stringify<TypeParam>({-75, 44, 93}, kStringifyOptions3), AnyOf(R"([
    -75,
    44,
    93
])",
                                                                                   R"([
    -75,
    93,
    44
])",
                                                                                   R"([
    44,
    -75,
    93
])",
                                                                                   R"([
    44,
    93,
    -75
])",
                                                                                   R"([
    93,
    -75,
    44
])",
                                                                                   R"([
    93,
    44,
    -75
])"));
}

REGISTER_TYPED_TEST_SUITE_P(TypedJsonSequenceTest, ParseSequence, StringifySequence);

// TODO: add unordered sets.
using SequenceTypes = ::testing::Types<  //
    std::vector<int>,                    //
    std::set<int>,                       //
    std::unordered_set<int>,             //
    absl::btree_set<int>,                //
    absl::flat_hash_set<int>,            //
    absl::node_hash_set<int>,            //
    tsdb2::common::flat_set<int>         //
    >;

INSTANTIATE_TYPED_TEST_SUITE_P(TypedJsonSequenceTest, TypedJsonSequenceTest, SequenceTypes);

template <typename Value>
class TypedJsonDictionaryTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(TypedJsonDictionaryTest);

TYPED_TEST_P(TypedJsonDictionaryTest, ParseDictionary) {
  EXPECT_THAT(json::Parse<TypeParam>("", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{}", kParseOptions1), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{}", kParseOptions2), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{}", kParseOptions3), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" {}", kParseOptions1), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" {}", kParseOptions2), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" {}", kParseOptions3), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{ }", kParseOptions1), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{ }", kParseOptions2), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{ }", kParseOptions3), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{} ", kParseOptions1), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{} ", kParseOptions2), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{} ", kParseOptions3), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" { } ", kParseOptions1), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" { } ", kParseOptions2), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" { } ", kParseOptions3), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{,}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{,}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{,}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 }", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 }", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 }", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42} ", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42} ", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42} ", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 } ", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 } ", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 } ", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 ,\"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 ,\"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 ,\"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42, \"bar\":43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42, \"bar\":43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42, \"bar\":43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\" :43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\" :43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\" :43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\": 43}", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\": 43}", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\": 43}", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43 }", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43 }", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43 }", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43} ", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43} ", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43} ", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 } ", kParseOptions1),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 } ", kParseOptions2),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 } ", kParseOptions3),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,}", kParseOptions1), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,}", kParseOptions2), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,}", kParseOptions3), Not(IsOk()));
  EXPECT_THAT(
      json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"baz\":44}", kParseOptions1),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"baz\":44}", kParseOptions2),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"baz\":44}", kParseOptions3),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 , \"baz\" : 44 } ", kParseOptions1),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 , \"baz\" : 44 } ", kParseOptions2),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 , \"baz\" : 44 } ", kParseOptions3),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"foo\":44}", kParseOptions1),
              Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"foo\":44}", kParseOptions2),
              Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"foo\":44}", kParseOptions3),
              Not(IsOk()));
}

TYPED_TEST_P(TypedJsonDictionaryTest, StringifyDictionary) {
  EXPECT_THAT(json::Stringify<TypeParam>({}, kStringifyOptions1), "{}");
  EXPECT_THAT(json::Stringify<TypeParam>({}, kStringifyOptions2), "{}");
  EXPECT_THAT(json::Stringify<TypeParam>({}, kStringifyOptions3), "{}");
  EXPECT_THAT(json::Stringify<TypeParam>({{"foo", 42}}, kStringifyOptions1), R"({"foo":42})");
  EXPECT_THAT(json::Stringify<TypeParam>({{"foo", 42}}, kStringifyOptions2), R"({
  "foo": 42
})");
  EXPECT_THAT(json::Stringify<TypeParam>({{"foo", 42}}, kStringifyOptions3), R"({
    "foo": 42
})");
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}}, kStringifyOptions1),
              AnyOf(R"({"lorem":123,"ipsum":456})", R"({"ipsum":456,"lorem":123})"));
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}}, kStringifyOptions2),
              AnyOf(R"({
  "lorem": 123,
  "ipsum": 456
})",
                    R"({
  "ipsum": 456,
  "lorem": 123
})"));
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}}, kStringifyOptions3),
              AnyOf(R"({
    "lorem": 123,
    "ipsum": 456
})",
                    R"({
    "ipsum": 456,
    "lorem": 123
})"));
  EXPECT_THAT(
      json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}, {"dolor", 789}},
                                 kStringifyOptions1),
      AnyOf(R"({"lorem":123,"ipsum":456,"dolor":789})", R"({"lorem":123,"dolor":789,"ipsum":456})",
            R"({"ipsum":456,"lorem":123,"dolor":789})", R"({"ipsum":456,"dolor":789,"lorem":123})",
            R"({"dolor":789,"lorem":123,"ipsum":456})",
            R"({"dolor":789,"ipsum":456,"lorem":123})"));
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}, {"dolor", 789}},
                                         kStringifyOptions2),
              AnyOf(R"({
  "lorem": 123,
  "ipsum": 456,
  "dolor": 789
})",
                    R"({
  "lorem": 123,
  "dolor": 789,
  "ipsum": 456
})",
                    R"({
  "ipsum": 456,
  "lorem": 123,
  "dolor": 789
})",
                    R"({
  "ipsum": 456,
  "dolor": 789,
  "lorem": 123
})",
                    R"({
  "dolor": 789,
  "lorem": 123,
  "ipsum": 456
})",
                    R"({
  "dolor": 789,
  "ipsum": 456,
  "lorem": 123
})"));
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}, {"dolor", 789}},
                                         kStringifyOptions3),
              AnyOf(R"({
    "lorem": 123,
    "ipsum": 456,
    "dolor": 789
})",
                    R"({
    "lorem": 123,
    "dolor": 789,
    "ipsum": 456
})",
                    R"({
    "ipsum": 456,
    "lorem": 123,
    "dolor": 789
})",
                    R"({
    "ipsum": 456,
    "dolor": 789,
    "lorem": 123
})",
                    R"({
    "dolor": 789,
    "lorem": 123,
    "ipsum": 456
})",
                    R"({
    "dolor": 789,
    "ipsum": 456,
    "lorem": 123
})"));
}

REGISTER_TYPED_TEST_SUITE_P(TypedJsonDictionaryTest, ParseDictionary, StringifyDictionary);

using DictionaryTypes = ::testing::Types<       //
    std::map<std::string, int>,                 //
    std::unordered_map<std::string, int>,       //
    absl::btree_map<std::string, int>,          //
    absl::flat_hash_map<std::string, int>,      //
    absl::node_hash_map<std::string, int>,      //
    tsdb2::common::flat_map<std::string, int>,  //
    tsdb2::common::trie_map<int>                //
    >;

INSTANTIATE_TYPED_TEST_SUITE_P(TypedJsonDictionaryTest, TypedJsonDictionaryTest, DictionaryTypes);

}  // namespace
