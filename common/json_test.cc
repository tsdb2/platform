#include "common/json.h"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

namespace {

JSON_OBJECT(                           //
    FooBar,                            //
    (int, lorem),                      //
    (bool, ipsum),                     //
    (std::string, dolor),              //
    (double, amet),                    //
    (std::vector<int>, consectetur));  //

// ...
// ((std::tuple<int, bool, std::string>), adipisci),  //
// (std::optional<double>, elit));                    //

TEST(JsonTest, FieldNames) {
  EXPECT_EQ(kFooBar_lorem_FieldName, "lorem");
  EXPECT_EQ(kFooBar_ipsum_FieldName, "ipsum");
  EXPECT_EQ(kFooBar_dolor_FieldName, "dolor");
  EXPECT_EQ(kFooBar_amet_FieldName, "amet");
  EXPECT_EQ(kFooBar_consectetur_FieldName, "consectetur");
  // TODO
}

}  // namespace
