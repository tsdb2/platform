#include "proto/text_format.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests.pb.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::tsdb2::proto::test::ColorEnum;
using ::tsdb2::proto::text::Parse;

TEST(TextFormatTest, Enum) {
  EXPECT_THAT(Parse<ColorEnum>(""), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<ColorEnum>("foobar"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<ColorEnum>("123"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_YELLOW"), IsOkAndHolds(ColorEnum::COLOR_YELLOW));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_MAGENTA"), IsOkAndHolds(ColorEnum::COLOR_MAGENTA));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_CYAN"), IsOkAndHolds(ColorEnum::COLOR_CYAN));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_RED"), IsOkAndHolds(ColorEnum::COLOR_RED));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_GREEN"), IsOkAndHolds(ColorEnum::COLOR_GREEN));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_BLUE"), IsOkAndHolds(ColorEnum::COLOR_BLUE));
  EXPECT_THAT(Parse<ColorEnum>(" COLOR_RED"), IsOkAndHolds(ColorEnum::COLOR_RED));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_RED "), IsOkAndHolds(ColorEnum::COLOR_RED));
  EXPECT_THAT(Parse<ColorEnum>(" COLOR_RED "), IsOkAndHolds(ColorEnum::COLOR_RED));
  EXPECT_THAT(Parse<ColorEnum>("COLOR_RED COLOR_GREEN"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(TextFormatTest, EmptyMessage) {
  using ::tsdb2::proto::test::EmptyMessage;
  EXPECT_THAT(Parse<EmptyMessage>(""), IsOkAndHolds(EmptyMessage{}));
  EXPECT_THAT(Parse<EmptyMessage>("foo: 42"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(TextFormatTest, OptionalField) {
  using ::tsdb2::proto::test::OptionalField;
  EXPECT_THAT(Parse<OptionalField>(""), IsOkAndHolds(OptionalField{}));
  EXPECT_THAT(Parse<OptionalField>("field:42"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field:42"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field :42"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field: 42"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42 "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field : 42 "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42,"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field:42,"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field :42,"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field: 42,"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42 ,"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42, "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field : 42 , "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42;"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field:42;"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field :42;"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field: 42;"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42 ;"), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field:42; "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>(" field : 42 ; "), IsOkAndHolds(OptionalField{.field = 42}));
  EXPECT_THAT(Parse<OptionalField>("field: 12.34"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("field: 12.34f"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("field: FOO"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("field: \"42\""), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("field: true"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("lorem: 42"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalField>("field: 123"), IsOkAndHolds(OptionalField{.field = 123}));
}

// TODO

}  // namespace
