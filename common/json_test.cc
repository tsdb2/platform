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
using ::testing::Optional;
using ::testing::VariantWith;
using ::testing::status::IsOkAndHolds;

char constexpr kFieldName1[] = "lorem";
char constexpr kFieldName2[] = "ipsum";
char constexpr kFieldName3[] = "dolor";
char constexpr kFieldName4[] = "sit";
char constexpr kFieldName5[] = "amet";
char constexpr kFieldName6[] = "consectetur";
char constexpr kFieldName7[] = "adipisci";
char constexpr kFieldName8[] = "elit";

using FooBar = json::Object<json::Field<int, kFieldName1>, json::Field<bool, kFieldName2>,
                            json::Field<std::string, kFieldName3>, json::Field<double, kFieldName4>,
                            json::Field<std::vector<int>, kFieldName5>,
                            json::Field<std::tuple<int, bool, std::string>, kFieldName6>,
                            json::Field<std::variant<std::string, double, int>, kFieldName7>,
                            json::Field<std::optional<double>, kFieldName8>>;

TEST(JsonTest, FieldAccess) {
  FooBar object;
  object.get<kFieldName1>() = 42;
  object.get<kFieldName2>() = true;
  object.get<kFieldName3>() = "foobar";
  object.get<kFieldName4>() = 3.14;
  object.get<kFieldName5>() = std::vector<int>{1, 2, 3};
  object.get<kFieldName6>() = std::make_tuple(43, false, "barbaz");
  object.get<kFieldName7>() = "hello";
  object.get<kFieldName8>() = 2.71;
  FooBar const& ref = object;
  EXPECT_EQ(ref.get<kFieldName1>(), 42);
  EXPECT_EQ(ref.get<kFieldName2>(), true);
  EXPECT_EQ(ref.get<kFieldName3>(), "foobar");
  EXPECT_EQ(ref.get<kFieldName4>(), 3.14);
  EXPECT_THAT(ref.get<kFieldName5>(), ElementsAre<int>(1, 2, 3));
  EXPECT_THAT(ref.get<kFieldName6>(), FieldsAre(43, false, "barbaz"));
  EXPECT_THAT(ref.get<kFieldName7>(), VariantWith<std::string>("hello"));
  EXPECT_THAT(ref.get<kFieldName8>(), Optional<double>(2.71));
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
// ((std::variant<std::string, double, int>), adipisci),  //
// (std::optional<double>, elit));                        //

TEST(MacroJsonTest, FieldNames) {
  EXPECT_EQ(kBarBaz_lorem_FieldName, std::string_view("lorem"));
  EXPECT_EQ(kBarBaz_ipsum_FieldName, std::string_view("ipsum"));
  EXPECT_EQ(kBarBaz_dolor_FieldName, std::string_view("dolor"));
  EXPECT_EQ(kBarBaz_sit_FieldName, std::string_view("sit"));
  EXPECT_EQ(kBarBaz_amet_FieldName, std::string_view("amet"));
  // TODO
}

TEST(MacroJsonTest, ParseBool) {
  EXPECT_THAT(json::Parse<bool>("true"), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>(" true"), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>("true "), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>(" true "), IsOkAndHolds(true));
  EXPECT_THAT(json::Parse<bool>("false"), IsOkAndHolds(false));
  EXPECT_THAT(json::Parse<bool>(" false"), IsOkAndHolds(false));
  EXPECT_THAT(json::Parse<bool>("false "), IsOkAndHolds(false));
  EXPECT_THAT(json::Parse<bool>(" false "), IsOkAndHolds(false));
}

}  // namespace
