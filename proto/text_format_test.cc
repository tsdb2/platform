#include "proto/text_format.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests/field_test.pb.h"
#include "proto/tests/indirection_test.pb.h"
#include "proto/tests/map_test.pb.h"
#include "proto/tests/oneof_test.pb.h"
#include "proto/tests/time_field_test.pb.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::tsdb2::proto::test::ColorEnum;
using ::tsdb2::proto::text::Parse;
using ::tsdb2::proto::text::Stringify;

TEST(ParserTest, Enum) {
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

TEST(ParserTest, EmptyMessage) {
  using ::tsdb2::proto::test::EmptyMessage;
  EXPECT_THAT(Parse<EmptyMessage>(""), IsOkAndHolds(EmptyMessage{}));
  EXPECT_THAT(Parse<EmptyMessage>("foo: 42"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ParserTest, OptionalField) {
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
  EXPECT_THAT(Parse<OptionalField>("field: 12.34"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalField>("field: FOO"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalField>("field: \"42\""),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalField>("field: true"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalField>("lorem: 42"), IsOkAndHolds(OptionalField()));
  EXPECT_THAT(Parse<OptionalField>("field2: 42"), IsOkAndHolds(OptionalField()));
  EXPECT_THAT(Parse<OptionalField>("field: 123"), IsOkAndHolds(OptionalField{.field = 123}));
}

TEST(ParserTest, DefaultedField) {
  using ::tsdb2::proto::test::DefaultedField;
  EXPECT_THAT(Parse<DefaultedField>(""), IsOkAndHolds(DefaultedField{.field = 42}));
  EXPECT_THAT(Parse<DefaultedField>("field: 43"), IsOkAndHolds(DefaultedField{.field = 43}));
  EXPECT_THAT(Parse<DefaultedField>("field: 12.34"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedField>("field: FOO"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedField>("field: \"43\""),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedField>("field: true"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedField>("field: 123"), IsOkAndHolds(DefaultedField{.field = 123}));
}

TEST(ParserTest, RequiredField) {
  using ::tsdb2::proto::test::RequiredField;
  EXPECT_THAT(Parse<RequiredField>(""), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: 43"), IsOkAndHolds(RequiredField{.field = 43}));
  EXPECT_THAT(Parse<RequiredField>("field: 12.34"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: FOO"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: \"43\""),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: true"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredField>("field: 123"), IsOkAndHolds(RequiredField{.field = 123}));
}

TEST(ParserTest, RepeatedField) {
  using ::tsdb2::proto::test::RepeatedField;
  EXPECT_THAT(Parse<RepeatedField>(""), IsOkAndHolds(RepeatedField{}));
  EXPECT_THAT(Parse<RepeatedField>("field: 42"), IsOkAndHolds(RepeatedField{.field{42}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12 field: 34"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12, field: 34"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12 field: 34,"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12, field: 34,"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12 field: 34"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12; field: 34"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12 field: 34;"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12; field: 34;"),
              IsOkAndHolds(RepeatedField{.field{12, 34}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 12.34"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedField>("field: FOO"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedField>("field: \"43\""),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedField>("field: true"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedField>("field: 56 field: 34 field: 12"),
              IsOkAndHolds(RepeatedField{.field{56, 34, 12}}));
}

TEST(ParserTest, OptionalEnumField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  EXPECT_THAT(Parse<OptionalEnumField>(""), IsOkAndHolds(OptionalEnumField{}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color:COLOR_GREEN"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color :COLOR_GREEN"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_GREEN"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color : COLOR_GREEN "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN,"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color:COLOR_GREEN,"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color :COLOR_GREEN,"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_GREEN,"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN ,"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN, "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color : COLOR_GREEN , "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN;"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color:COLOR_GREEN;"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color :COLOR_GREEN;"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_GREEN;"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN ;"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color:COLOR_GREEN; "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>(" color : COLOR_GREEN ; "),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: FOO"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("lorem: COLOR_GREEN"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("colorem: COLOR_GREEN"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_BLUE}));
}

TEST(ParserTest, DefaultedEnumField) {
  using ::tsdb2::proto::test::DefaultedEnumField;
  EXPECT_THAT(Parse<DefaultedEnumField>(""),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_CYAN}));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: COLOR_GREEN"),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: FOO"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_BLUE}));
}

TEST(ParserTest, RequiredEnumField) {
  using ::tsdb2::proto::test::RequiredEnumField;
  EXPECT_THAT(Parse<RequiredEnumField>(""), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredEnumField>("color: COLOR_GREEN"),
              IsOkAndHolds(RequiredEnumField{.color = ColorEnum::COLOR_GREEN}));
  EXPECT_THAT(Parse<RequiredEnumField>("color: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: FOO"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(RequiredEnumField{.color = ColorEnum::COLOR_BLUE}));
}

TEST(ParserTest, RepeatedEnumField) {
  using ::tsdb2::proto::test::RepeatedEnumField;
  EXPECT_THAT(Parse<RepeatedEnumField>(""), IsOkAndHolds(RepeatedEnumField{}));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: COLOR_GREEN"),
              IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE color: COLOR_GREEN"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE, color: COLOR_GREEN"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE color: COLOR_GREEN,"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE, color: COLOR_GREEN,"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE color: COLOR_GREEN"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE; color: COLOR_GREEN"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE color: COLOR_GREEN;"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(
      Parse<RepeatedEnumField>("color: COLOR_BLUE; color: COLOR_GREEN;"),
      IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_BLUE, ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: FOO"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: \"43\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: COLOR_RED color: COLOR_GREEN color: COLOR_BLUE"),
              IsOkAndHolds(RepeatedEnumField{
                  .color{ColorEnum::COLOR_RED, ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE}}));
}

TEST(ParserTest, ManyFields) {
  using ::tsdb2::proto::test::ManyFields;
  EXPECT_THAT(Parse<ManyFields>(""), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<ManyFields>(R"pb(
                required_fixed32_field: 42 required_bool_field: true
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 42,
                  .required_bool_field = true,
              }));
  EXPECT_THAT(Parse<ManyFields>(R"pb(
                # lorem ipsum
                required_fixed32_field: 42
                # dolor amet
                required_bool_field: true
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 42,
                  .required_bool_field = true,
              }));
}

// TODO

TEST(StringifierTest, Enum) {
  EXPECT_EQ(Stringify(ColorEnum::COLOR_RED), "COLOR_RED");
  EXPECT_EQ(Stringify(ColorEnum::COLOR_GREEN), "COLOR_GREEN");
  EXPECT_EQ(Stringify(ColorEnum::COLOR_BLUE), "COLOR_BLUE");
  EXPECT_EQ(Stringify(ColorEnum::COLOR_CYAN), "COLOR_CYAN");
  EXPECT_EQ(Stringify(ColorEnum::COLOR_MAGENTA), "COLOR_MAGENTA");
  EXPECT_EQ(Stringify(ColorEnum::COLOR_YELLOW), "COLOR_YELLOW");
  EXPECT_EQ(Stringify(static_cast<ColorEnum>(42)), "42");
  EXPECT_EQ(Stringify(static_cast<ColorEnum>(-43)), "-43");
}

// TODO

}  // namespace
