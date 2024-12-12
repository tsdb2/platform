#include "tsz/types.h"

#include "absl/hash/hash.h"
#include "gtest/gtest.h"

namespace {

using ::tsz::BoolValue;
using ::tsz::FieldMap;
using ::tsz::FieldMapView;
using ::tsz::IntValue;
using ::tsz::StringValue;

TEST(TszTypesTest, EmptyView) {
  FieldMapView const view;
  EXPECT_TRUE(view.empty());
}

TEST(TszTypesTest, NonEmptyView) {
  FieldMap const fields{
      {"lorem", IntValue(123)},
      {"ipsum", BoolValue(true)},
      {"dolor", StringValue("hello")},
  };
  FieldMapView const view{fields};
  EXPECT_FALSE(view.empty());
  EXPECT_EQ(view.hash(), absl::HashOf(fields));
  EXPECT_EQ(view.value(), fields);
}

TEST(TszTypesTest, IdenticalViews) {
  FieldMap const fields{
      {"lorem", IntValue(123)},
      {"ipsum", BoolValue(true)},
      {"dolor", StringValue("hello")},
  };
  FieldMapView const view1{fields};
  FieldMapView const view2{fields};
  EXPECT_EQ(view1.hash(), view2.hash());
  EXPECT_EQ(view1.value(), view2.value());
  EXPECT_TRUE(view1 == view2);
  EXPECT_FALSE(view1 != view2);
}

TEST(TszTypesTest, DifferentViews) {
  FieldMap const fields1{
      {"lorem", IntValue(123)},
      {"ipsum", BoolValue(true)},
  };
  FieldMap const fields2{
      {"dolor", StringValue("hello")},
      {"amet", IntValue(456)},
  };
  FieldMapView const view1{fields1};
  FieldMapView const view2{fields2};
  EXPECT_NE(view1.hash(), view2.hash());
  EXPECT_NE(view1.value(), view2.value());
  EXPECT_FALSE(view1 == view2);
  EXPECT_TRUE(view1 != view2);
}

}  // namespace
