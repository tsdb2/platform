#include "common/json.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "common/flat_set.h"
#include "common/testing.h"
#include "gtest/gtest.h"

namespace {

namespace json = tsdb2::json;

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::status::IsOk;
using ::testing::status::IsOkAndHolds;

char constexpr kFieldName1[] = "lorem";
char constexpr kFieldName2[] = "ipsum";
char constexpr kFieldName3[] = "dolor";
char constexpr kFieldName4[] = "sit";
char constexpr kFieldName5[] = "amet";
char constexpr kFieldName6[] = "consectetur";
char constexpr kFieldName7[] = "adipisci";

using TestObject = json::Object<                                   //
    json::Field<int, kFieldName1>,                                 //
    json::Field<bool, kFieldName2>,                                //
    json::Field<std::string, kFieldName3>,                         //
    json::Field<double, kFieldName4>,                              //
    json::Field<std::vector<int>, kFieldName5>,                    //
    json::Field<std::tuple<int, bool, std::string>, kFieldName6>,  //
    json::Field<std::optional<double>, kFieldName7>>;              //

TEST(JsonTest, FieldAccess) {
  TestObject object;
  object.get<kFieldName1>() = 42;
  object.get<kFieldName2>() = true;
  object.get<kFieldName3>() = "foobar";
  object.get<kFieldName4>() = 3.14;
  object.get<kFieldName5>() = std::vector<int>{1, 2, 3};
  object.get<kFieldName6>() = std::make_tuple(43, false, "barbaz");
  object.get<kFieldName7>() = 2.71;
  TestObject const& ref = object;
  EXPECT_EQ(ref.get<kFieldName1>(), 42);
  EXPECT_EQ(ref.get<kFieldName2>(), true);
  EXPECT_EQ(ref.get<kFieldName3>(), "foobar");
  EXPECT_EQ(ref.get<kFieldName4>(), 3.14);
  EXPECT_THAT(ref.get<kFieldName5>(), ElementsAre<int>(1, 2, 3));
  EXPECT_THAT(ref.get<kFieldName6>(), FieldsAre(43, false, "barbaz"));
  EXPECT_THAT(ref.get<kFieldName7>(), Optional<double>(2.71));
}

TEST(JsonTest, Stringify) {
  TestObject object;
  object.get<kFieldName1>() = 42;
  object.get<kFieldName2>() = true;
  object.get<kFieldName3>() = "foobar";
  object.get<kFieldName4>() = 3.14;
  object.get<kFieldName5>() = std::vector<int>{1, 2, 3};
  object.get<kFieldName6>() = std::make_tuple(43, false, "barbaz");
  object.get<kFieldName7>() = 2.71;
  EXPECT_EQ(
      json::Stringify(object),
      R"json({"lorem":42,"ipsum":true,"dolor":"foobar","sit":3.14,"amet":[1,2,3],"consectetur":[43,false,"barbaz"],"adipisci":2.71})json");
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
  // TODO: test escape codes.
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

template <typename Value>
class TypedJsonTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(TypedJsonTest);

TYPED_TEST_P(TypedJsonTest, ParseSequence) {
  EXPECT_THAT(json::Parse<TypeParam>(""), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("["), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[]"), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(json::Parse<TypeParam>(" []"), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(json::Parse<TypeParam>("[ ]"), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(json::Parse<TypeParam>("[] "), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(json::Parse<TypeParam>(" [ ] "), IsOkAndHolds(ElementsAre()));
  EXPECT_THAT(json::Parse<TypeParam>("[,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42]"), IsOkAndHolds(ElementsAre(42)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[,42]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>(" [42,43]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[ 42,43]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42 ,43]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42, 43]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43 ]"), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43] "), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>(" [ 42 , 43 ] "), IsOkAndHolds(ElementsAre(42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[-42,43]"), IsOkAndHolds(ElementsAre(-42, 43)));
  EXPECT_THAT(json::Parse<TypeParam>("[42,- 43]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43,]"), Not(IsOk()));
  EXPECT_THAT(json::Parse<TypeParam>("[42,43,44]"), IsOkAndHolds(ElementsAre(42, 43, 44)));
  EXPECT_THAT(json::Parse<TypeParam>(" [ 42 , 43 , 44 ] "), IsOkAndHolds(ElementsAre(42, 43, 44)));
}

TYPED_TEST_P(TypedJsonTest, StringifySequence) {
  EXPECT_EQ(json::Stringify<TypeParam>({}), "[]");
  EXPECT_EQ(json::Stringify<TypeParam>({42}), "[42]");
  EXPECT_EQ(json::Stringify<TypeParam>({42, 43}), "[42,43]");
  EXPECT_EQ(json::Stringify<TypeParam>({-75, 43, 44, 93}), "[-75,43,44,93]");
}

REGISTER_TYPED_TEST_SUITE_P(TypedJsonTest, ParseSequence, StringifySequence);

// TODO: add unordered sets.
using SequenceTypes = ::testing::Types<  //
    std::vector<int>,                    //
    std::set<int>,                       //
    tsdb2::common::flat_set<int>         //
    >;

INSTANTIATE_TYPED_TEST_SUITE_P(TypedJsonTest, TypedJsonTest, SequenceTypes);

// TODO

JSON_OBJECT(                    //
    BarBaz,                     //
    (int, lorem),               //
    (bool, ipsum),              //
    (std::string, dolor),       //
    (double, sit),              //
    (std::vector<int>, amet));  //

// ...
// ((std::tuple<int, bool, std::string>), consectetur),   //
// (std::optional<double>, adipisci));                    //

TEST(MacroJsonTest, FieldNames) {
  EXPECT_EQ(kBarBaz_lorem_FieldName, std::string_view("lorem"));
  EXPECT_EQ(kBarBaz_ipsum_FieldName, std::string_view("ipsum"));
  EXPECT_EQ(kBarBaz_dolor_FieldName, std::string_view("dolor"));
  EXPECT_EQ(kBarBaz_sit_FieldName, std::string_view("sit"));
  EXPECT_EQ(kBarBaz_amet_FieldName, std::string_view("amet"));
  // TODO
}

}  // namespace
