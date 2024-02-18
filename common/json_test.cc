#include "common/json.h"

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "common/testing.h"
#include "gtest/gtest.h"

namespace {

namespace json = tsdb2::json;

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Not;
using ::testing::Optional;
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
}

TEST(JsonTest, StringifyBool) {
  EXPECT_EQ(json::Stringify(true), "true");
  EXPECT_EQ(json::Stringify(false), "false");
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

// TODO: StringifyFloat test?

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
