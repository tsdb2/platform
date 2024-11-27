#include "common/json.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
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
#include "absl/status/status_matchers.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/testing.h"
#include "common/type_string.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::tsdb2::common::TypeStringT;

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
  EXPECT_THAT(ref, AllOf(Property(&TestObject1::get<kFieldName1>, 42),
                         Property(&TestObject1::get<kFieldName2>, true),
                         Property(&TestObject1::get<kFieldName3>, "foobar"),
                         Property(&TestObject1::get<kFieldName4>, 3.14),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre<int>(1, 2, 3)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre<int>(4, 5, 6, 7)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(43, false, "barbaz")),
                         Property(&TestObject1::get<kFieldName8>, Optional<double>(2.71))));
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
  EXPECT_THAT(
      ref,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 43)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, false)),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("barbaz"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(12, 34))));
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
      AllOf(
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 0)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, false)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName3>, "")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 0.0)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(0, 0, 0))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, IsEmpty())),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>, std::tuple<int, bool, std::string>())),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, std::nullopt)),
          Property(&TestObject2::get<kFieldName3>, nullptr),
          Property(&TestObject2::get<kFieldName4>, IsEmpty()),
          Property(&TestObject2::get<kFieldName5>, Pair(0, 0))));
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
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject3::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject3::get<kFieldName3>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject3::get<kFieldName4>, Pair(12, 34))));
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
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject3::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject3::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject3::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject3::get<kFieldName3>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject3::get<kFieldName4>, Pair(12, 34))));
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
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("barbaz"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(12, 34))));
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
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("barbaz"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(12, 34))));
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
  EXPECT_THAT(
      obj1,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("barfoo"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 24)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, false)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "barbaz")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 2.71)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(3, 2, 1))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(7, 6, 5, 4))),
          Property(
              &TestObject2::get<kFieldName2>,
              Property(&TestObject1::get<kFieldName7>, FieldsAre(44, true, std::string("bazfoo")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(3.14))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("bazbar"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 24), Pair("bar", 34), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(34, 12))));
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("barbaz"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(12, 34))));
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
  EXPECT_THAT(
      obj1,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("barfoo"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 24)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, false)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "barbaz")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 2.71)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(3, 2, 1))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(7, 6, 5, 4))),
          Property(
              &TestObject2::get<kFieldName2>,
              Property(&TestObject1::get<kFieldName7>, FieldsAre(44, true, std::string("bazfoo")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(3.14))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("bazbar"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 24), Pair("bar", 34), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(34, 12))));
  EXPECT_THAT(
      obj2,
      AllOf(
          Property(&TestObject2::get<kFieldName1>, Pointee(std::string("foobar"))),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName1>, 42)),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName2>, true)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName3>, "foobar")),
          Property(&TestObject2::get<kFieldName2>, Property(&TestObject1::get<kFieldName4>, 3.14)),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName7>,
                            FieldsAre(43, false, std::string("barbaz")))),
          Property(&TestObject2::get<kFieldName2>,
                   Property(&TestObject1::get<kFieldName8>, Optional(2.71))),
          Property(&TestObject2::get<kFieldName3>, Pointee(std::string("barbaz"))),
          Property(&TestObject2::get<kFieldName4>,
                   UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))),
          Property(&TestObject2::get<kFieldName5>, Pair(12, 34))));
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

TEST(JsonTest, ParseEmpty) {
  EXPECT_THAT(json::Parse<json::Object<>>("{"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>("{}"));
  EXPECT_OK(json::Parse<json::Object<>>(" {}"));
  EXPECT_OK(json::Parse<json::Object<>>("{ }"));
  EXPECT_OK(json::Parse<json::Object<>>("{} "));
  EXPECT_OK(json::Parse<json::Object<>>(" { } "));
}

TEST(JsonTest, ParseEmptyWithOptionals) {
  using Object = json::Object<                         //
      json::Field<std::optional<int>, kFieldName1>,    //
      json::Field<std::unique_ptr<int>, kFieldName2>,  //
      json::Field<std::shared_ptr<int>, kFieldName3>   //
      >;
  EXPECT_THAT(json::Parse<Object>("{"), Not(IsOk()));
  EXPECT_OK(json::Parse<Object>("{}"));
  EXPECT_OK(json::Parse<Object>(" {}"));
  EXPECT_OK(json::Parse<Object>("{ }"));
  EXPECT_OK(json::Parse<Object>("{} "));
  EXPECT_OK(json::Parse<Object>(" { } "));
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
  EXPECT_THAT(json::Parse<Object1>("{"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object2>("{}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object3>("{}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" {}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{ }"), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>("{} "), Not(IsOk()));
  EXPECT_THAT(json::Parse<Object1>(" { } "), Not(IsOk()));
}

TEST(JsonTest, StringifyEmpty) {
  json::Object<> object;
  EXPECT_EQ(object.Stringify(), "{}");
  EXPECT_EQ(json::Stringify(object), "{}");
}

TEST(JsonTest, Parse) {
  TestObject1 object;
  EXPECT_THAT(
      json::Parse<TestObject1>(
          R"({"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"],"elit":2.71})"),
      IsOkAndHolds(AllOf(Property(&TestObject1::get<kFieldName1>, 42),
                         Property(&TestObject1::get<kFieldName2>, true),
                         Property(&TestObject1::get<kFieldName3>, "foobar"),
                         Property(&TestObject1::get<kFieldName4>, 3.14),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(43, false, "barbaz")),
                         Property(&TestObject1::get<kFieldName8>, Optional(2.71)))));
  EXPECT_THAT(
      json::Parse<TestObject1>(
          R"({"lorem":43,"ipsum":false,"dolor":"barfoo","sit":14.3,"amet":[5,6,7],"consectetur":[1,2,3,4],"adipisci":[42,true,"bazbar"],"elit":71.2})"),
      IsOkAndHolds(AllOf(Property(&TestObject1::get<kFieldName1>, 43),
                         Property(&TestObject1::get<kFieldName2>, false),
                         Property(&TestObject1::get<kFieldName3>, "barfoo"),
                         Property(&TestObject1::get<kFieldName4>, 14.3),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre(5, 6, 7)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre(1, 2, 3, 4)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(42, true, "bazbar")),
                         Property(&TestObject1::get<kFieldName8>, Optional(71.2)))));
  EXPECT_THAT(
      json::Parse<TestObject1>(R"json({
        "lorem": 42,
        "ipsum": true,
        "dolor": "foobar",
        "sit": 3.14,
        "amet": [1, 2, 3],
        "consectetur": [4, 5, 6, 7],
        "adipisci": [43, false, "barbaz"],
        "elit": 2.71
      })json"),
      IsOkAndHolds(AllOf(Property(&TestObject1::get<kFieldName1>, 42),
                         Property(&TestObject1::get<kFieldName2>, true),
                         Property(&TestObject1::get<kFieldName3>, "foobar"),
                         Property(&TestObject1::get<kFieldName4>, 3.14),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(43, false, "barbaz")),
                         Property(&TestObject1::get<kFieldName8>, Optional(2.71)))));
}

TEST(JsonTest, UnorderedFields) {
  TestObject1 object;
  EXPECT_THAT(
      json::Parse<TestObject1>(R"json({
        "ipsum": true,
        "elit": 2.71,
        "adipisci": [43, false, "barbaz"],
        "consectetur": [4, 5, 6, 7],
        "amet": [1, 2, 3],
        "sit": 3.14,
        "dolor": "foobar",
        "lorem": 42
      })json"),
      IsOkAndHolds(AllOf(Property(&TestObject1::get<kFieldName1>, 42),
                         Property(&TestObject1::get<kFieldName2>, true),
                         Property(&TestObject1::get<kFieldName3>, "foobar"),
                         Property(&TestObject1::get<kFieldName4>, 3.14),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(43, false, "barbaz")),
                         Property(&TestObject1::get<kFieldName8>, Optional(2.71)))));
}

TEST(JsonTest, SkipNull) {
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":null })"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": null })"));
}

TEST(JsonTest, SkipBool) {
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":true })"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": true })"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":false })"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": false })"));
}

TEST(JsonTest, SkipString) {
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":""})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": ""})"));
  EXPECT_OK(
      json::Parse<json::Object<>>(R"({"foo":"a \" b \\ c / d \b e \f f \n g \r h \t i \u0042"})"));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\x"})"), Not(IsOk()));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":"\ugggg"})"), Not(IsOk()));
}

TEST(JsonTest, SkipObject) {
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ }})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { }})"));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar"}})"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": {"bar":null}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{ "bar":null}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar" :null}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar": null}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":null }})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": { "bar" : null }})"));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":{"bar":null,}})"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true ,"baz":false}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true, "baz":false}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz" :false}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz": false}})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false }})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true , "baz" : false }})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":{"bar":true,"baz":false,"qux":null}})"));
}

TEST(JsonTest, SkipArray) {
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[})"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": []})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ ]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ ]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [1]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[ 1]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 ]})"));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,]})"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1 ,2]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1, 2]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2 ]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 ]})"));
  EXPECT_THAT(json::Parse<json::Object<>>(R"({"foo":[1,2,]})"), Not(IsOk()));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo":[1,2,3]})"));
  EXPECT_OK(json::Parse<json::Object<>>(R"({"foo": [ 1 , 2 , 3 ] })"));
}

TEST(JsonTest, ParseObjectWithExtraFields) {
  TestObject1 object;
  EXPECT_THAT(
      json::Parse<TestObject1>(R"json({
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
      })json"),
      IsOkAndHolds(AllOf(Property(&TestObject1::get<kFieldName1>, 42),
                         Property(&TestObject1::get<kFieldName2>, true),
                         Property(&TestObject1::get<kFieldName3>, "foobar"),
                         Property(&TestObject1::get<kFieldName4>, 3.14),
                         Property(&TestObject1::get<kFieldName5>, ElementsAre(1, 2, 3)),
                         Property(&TestObject1::get<kFieldName6>, ElementsAre(4, 5, 6, 7)),
                         Property(&TestObject1::get<kFieldName7>, FieldsAre(43, false, "barbaz")),
                         Property(&TestObject1::get<kFieldName8>, Optional(2.71)))));
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
      object.Stringify(),
      R"({"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[4,5,6,7],"adipisci":[43,false,"barbaz"],"elit":2.71})");
  EXPECT_EQ(json::Stringify(object), object.Stringify());
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
  EXPECT_EQ(json::Stringify(true), "true");
  EXPECT_EQ(json::Stringify(false), "false");
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
  EXPECT_EQ(json::Stringify<uint8_t>(42), "42");
  EXPECT_EQ(json::Stringify<uint16_t>(43), "43");
  EXPECT_EQ(json::Stringify<uint32_t>(44), "44");
  EXPECT_EQ(json::Stringify<uint64_t>(45), "45");
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
  EXPECT_EQ(json::Stringify<int8_t>(42), "42");
  EXPECT_EQ(json::Stringify<int16_t>(43), "43");
  EXPECT_EQ(json::Stringify<int32_t>(44), "44");
  EXPECT_EQ(json::Stringify<int64_t>(45), "45");
  EXPECT_EQ(json::Stringify<int8_t>(-46), "-46");
  EXPECT_EQ(json::Stringify<int16_t>(-47), "-47");
  EXPECT_EQ(json::Stringify<int32_t>(-48), "-48");
  EXPECT_EQ(json::Stringify<int64_t>(-49), "-49");
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
  EXPECT_EQ(json::Stringify<float>(3.14), "3.14");
  EXPECT_EQ(json::Stringify<float>(-3.14), "-3.14");
  EXPECT_EQ(json::Stringify<double>(2.71), "2.71");
  EXPECT_EQ(json::Stringify<double>(-2.71), "-2.71");
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
  EXPECT_EQ(json::Stringify<std::string>("lorem \"ipsum\""), "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("lorem \"ipsum\""), "\"lorem \\\"ipsum\\\"\"");
  EXPECT_EQ(json::Stringify("a \" b \\ c / d \b e \f f \n g \r h \t i \x84"),
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
  EXPECT_EQ(json::Stringify<std::optional<int>>(std::nullopt), "null");
  EXPECT_EQ(json::Stringify<std::optional<int>>(42), "42");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>(std::nullopt), "null");
  EXPECT_EQ(json::Stringify<std::optional<std::string>>("lorem"), "\"lorem\"");
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
  EXPECT_EQ(json::Stringify(std::make_pair(42, "lorem")), "[42,\"lorem\"]");
  EXPECT_EQ(json::Stringify(std::make_pair("ipsum", 43)), "[\"ipsum\",43]");
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
  EXPECT_EQ(json::Stringify(std::make_tuple()), "[]");
  EXPECT_EQ(json::Stringify(std::make_tuple(42)), "[42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42)), "[true,42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, "lorem", 42)), "[true,\"lorem\",42]");
  EXPECT_EQ(json::Stringify(std::make_tuple(true, 42, "lorem", 43)), "[true,42,\"lorem\",43]");
  EXPECT_EQ(json::Stringify(std::make_tuple(false, 43, "ipsum", 42)), "[false,43,\"ipsum\",42]");
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
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({1, 2, 3, 4})), "[1,2,3,4]");
  EXPECT_EQ((json::Stringify<std::array<int, 4>>({44, -75, 93, 43})), "[44,-75,93,43]");
  EXPECT_EQ((json::Stringify<std::array<int, 3>>({75, 44, -93})), "[75,44,-93]");
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
  EXPECT_EQ(json::Stringify<TypeParam>({}), "[]");
  EXPECT_EQ(json::Stringify<TypeParam>({42}), "[42]");
  EXPECT_THAT(json::Stringify<TypeParam>({42, 43}), AnyOf("[42,43]", "[43,42]"));
  EXPECT_THAT(json::Stringify<TypeParam>({-75, 44, 93}),
              AnyOf("[-75,44,93]", "[-75,93,44]", "[44,-75,93]", "[44,93,-75]", "[93,-75,44]",
                    "[93,44,-75]"));
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
  EXPECT_THAT(json::Parse<TypeParam>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{}"), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" {}"), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{ }"), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{} "), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>(" { } "), IsOkAndHolds(IsEmpty()));
  EXPECT_THAT(json::Parse<TypeParam>("{,}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 }"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42} "),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 } "),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,}"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" {\"foo\":42,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{ \"foo\":42,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\" :42,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\": 42,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42 ,\"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42, \"bar\":43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\" :43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\": 43}"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43 }"),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43} "),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 } "),
              IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,}"), Not(IsOk()));
  EXPECT_THAT(
      json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"baz\":44}"),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(
      json::Parse<TypeParam>(" { \"foo\" : 42 , \"bar\" : 43 , \"baz\" : 44 } "),
      IsOkAndHolds(UnorderedElementsAre(Pair("foo", 42), Pair("bar", 43), Pair("baz", 44))));
  EXPECT_THAT(json::Parse<TypeParam>("{\"foo\":42,\"bar\":43,\"foo\":44}"), Not(IsOk()));
}

TYPED_TEST_P(TypedJsonDictionaryTest, StringifyDictionary) {
  EXPECT_THAT(json::Stringify<TypeParam>({}), "{}");
  EXPECT_THAT(json::Stringify<TypeParam>({{"foo", 42}}), "{\"foo\":42}");
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}}),
              AnyOf("{\"lorem\":123,\"ipsum\":456}", "{\"ipsum\":456,\"lorem\":123}"));
  EXPECT_THAT(json::Stringify<TypeParam>({{"lorem", 123}, {"ipsum", 456}, {"dolor", 789}}),
              AnyOf("{\"lorem\":123,\"ipsum\":456,\"dolor\":789}",
                    "{\"lorem\":123,\"dolor\":789,\"ipsum\":456}",
                    "{\"ipsum\":456,\"lorem\":123,\"dolor\":789}",
                    "{\"ipsum\":456,\"dolor\":789,\"lorem\":123}",
                    "{\"dolor\":789,\"lorem\":123,\"ipsum\":456}",
                    "{\"dolor\":789,\"ipsum\":456,\"lorem\":123}"));
}

REGISTER_TYPED_TEST_SUITE_P(TypedJsonDictionaryTest, ParseDictionary, StringifyDictionary);

using DictionaryTypes = ::testing::Types<      //
    std::map<std::string, int>,                //
    std::unordered_map<std::string, int>,      //
    absl::btree_map<std::string, int>,         //
    absl::flat_hash_map<std::string, int>,     //
    absl::node_hash_map<std::string, int>,     //
    tsdb2::common::flat_map<std::string, int>  //
    >;

INSTANTIATE_TYPED_TEST_SUITE_P(TypedJsonDictionaryTest, TypedJsonDictionaryTest, DictionaryTypes);

}  // namespace
