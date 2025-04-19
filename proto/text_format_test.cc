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
  EXPECT_THAT(Parse<ColorEnum>("foobar"), StatusIs(absl::StatusCode::kFailedPrecondition));
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
  EXPECT_THAT(Parse<EmptyMessage>(""), IsOkAndHolds(EmptyMessage()));
  EXPECT_THAT(Parse<EmptyMessage>("foo: 42"), IsOkAndHolds(EmptyMessage()));
}

TEST(ParserTest, OptionalField) {
  using ::tsdb2::proto::test::OptionalField;
  EXPECT_THAT(Parse<OptionalField>(""), IsOkAndHolds(OptionalField()));
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
  EXPECT_THAT(Parse<OptionalField>("field: 12 field: 34"),
              IsOkAndHolds(OptionalField{.field = 34}));
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
  EXPECT_THAT(Parse<DefaultedField>("field: 12 field: 34"),
              IsOkAndHolds(DefaultedField{.field = 34}));
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
  EXPECT_THAT(Parse<RequiredField>("field: 12 field: 34"),
              IsOkAndHolds(RequiredField{.field = 34}));
}

TEST(ParserTest, RepeatedField) {
  using ::tsdb2::proto::test::RepeatedField;
  EXPECT_THAT(Parse<RepeatedField>(""), IsOkAndHolds(RepeatedField()));
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
  EXPECT_THAT(Parse<RepeatedField>(R"pb(
                # element 1
                field: 56
                # element 2
                field: 34
                # element 3
                field: 12
              )pb"),
              IsOkAndHolds(RepeatedField{.field{56, 34, 12}}));
  EXPECT_THAT(Parse<RepeatedField>("field: 56 unknown_field: 34 field: 12"),
              IsOkAndHolds(RepeatedField{.field{56, 12}}));
}

TEST(ParserTest, OptionalEnumField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  EXPECT_THAT(Parse<OptionalEnumField>(""), IsOkAndHolds(OptionalEnumField()));
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
  EXPECT_THAT(Parse<OptionalEnumField>("color: FOO"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalEnumField>("color: true"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<OptionalEnumField>("lorem: COLOR_GREEN"), IsOkAndHolds(OptionalEnumField()));
  EXPECT_THAT(Parse<OptionalEnumField>("colorem: COLOR_GREEN"), IsOkAndHolds(OptionalEnumField()));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_BLUE}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_BLUE color: COLOR_RED"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_RED}));
  EXPECT_THAT(Parse<OptionalEnumField>("color: COLOR_BLUE unknown_field: FOO color: COLOR_RED"),
              IsOkAndHolds(OptionalEnumField{.color = ColorEnum::COLOR_RED}));
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
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: true"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_BLUE}));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: COLOR_BLUE color: COLOR_RED"),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_RED}));
  EXPECT_THAT(Parse<DefaultedEnumField>("color: COLOR_BLUE unknown_field: 42 color: COLOR_RED"),
              IsOkAndHolds(DefaultedEnumField{.color = ColorEnum::COLOR_RED}));
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
  EXPECT_THAT(Parse<RequiredEnumField>("color: FOO"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredEnumField>("color: \"COLOR_GREEN\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredEnumField>("color: true"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredEnumField>("color: COLOR_BLUE"),
              IsOkAndHolds(RequiredEnumField{.color = ColorEnum::COLOR_BLUE}));
  EXPECT_THAT(Parse<RequiredEnumField>("color: COLOR_BLUE color: COLOR_RED"),
              IsOkAndHolds(RequiredEnumField{.color = ColorEnum::COLOR_RED}));
  EXPECT_THAT(Parse<RequiredEnumField>("color: COLOR_BLUE unknown_field: 42 color: COLOR_RED"),
              IsOkAndHolds(RequiredEnumField{.color = ColorEnum::COLOR_RED}));
}

TEST(ParserTest, RepeatedEnumField) {
  using ::tsdb2::proto::test::RepeatedEnumField;
  EXPECT_THAT(Parse<RepeatedEnumField>(""), IsOkAndHolds(RepeatedEnumField()));
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
  EXPECT_THAT(Parse<RepeatedEnumField>("color: FOO"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: \"43\""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: true"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: COLOR_RED color: COLOR_GREEN color: COLOR_BLUE"),
              IsOkAndHolds(RepeatedEnumField{
                  .color{ColorEnum::COLOR_RED, ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE}}));
  EXPECT_THAT(Parse<RepeatedEnumField>(R"pb(
                # red
                color: COLOR_RED
                # green
                color: COLOR_GREEN
                # blue
                color: COLOR_BLUE
              )pb"),
              IsOkAndHolds(RepeatedEnumField{
                  .color{ColorEnum::COLOR_RED, ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE}}));
  EXPECT_THAT(Parse<RepeatedEnumField>("color: COLOR_RED unknwon_color: FOO color: COLOR_BLUE"),
              IsOkAndHolds(RepeatedEnumField{.color{ColorEnum::COLOR_RED, ColorEnum::COLOR_BLUE}}));
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
                required_bool_field: true required_fixed32_field: 42
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 42,
                  .required_bool_field = true,
              }));
  EXPECT_THAT(Parse<ManyFields>(R"pb(
                required_bool_field: true
                required_bool_field: false
                required_fixed32_field: 12
                required_fixed32_field: 34
                required_fixed32_field: 56
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 56,
                  .required_bool_field = false,
              }));
  EXPECT_THAT(Parse<ManyFields>(R"pb(
                required_fixed32_field: 42
                required_bool_field: true
                unknown_field: false
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 42,
                  .required_bool_field = true,
              }));
  EXPECT_THAT(Parse<ManyFields>(R"pb(
                required_fixed32_field: 42
                unknown_field: "foo"
                required_bool_field: true
              )pb"),
              IsOkAndHolds(ManyFields{
                  .required_fixed32_field = 42,
                  .required_bool_field = true,
              }));
  EXPECT_THAT(
      Parse<ManyFields>(R"pb(
        unknown_field: BAR required_fixed32_field: 42 required_bool_field: true
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

TEST(ParserTest, OptionalStringField) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_THAT(Parse<OptionalStringField>(""), IsOkAndHolds(OptionalStringField()));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\""),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field:\"lorem\""),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field :\"lorem\""),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field: \"lorem\""),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\" "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field : \"lorem\" "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\","),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field:\"lorem\","),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field :\"lorem\","),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field: \"lorem\","),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\" ,"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\", "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field : \"lorem\" , "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\";"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field:\"lorem\";"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field :\"lorem\";"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field: \"lorem\";"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\" ;"),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field:\"lorem\"; "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>(" field : \"lorem\" ; "),
              IsOkAndHolds(OptionalStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<OptionalStringField>("field: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("field: lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("field: FOO"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("field: 42"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("field: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<OptionalStringField>("lorem: \"lorem\""), IsOkAndHolds(OptionalStringField()));
  EXPECT_THAT(Parse<OptionalStringField>("field2: \"lorem\""), IsOkAndHolds(OptionalStringField()));
  EXPECT_THAT(Parse<OptionalStringField>("field: \"ipsum\""),
              IsOkAndHolds(OptionalStringField{.field = "ipsum"}));
  EXPECT_THAT(Parse<OptionalStringField>("field: \"lorem\" field: \"ipsum\""),
              IsOkAndHolds(OptionalStringField{.field = "ipsum"}));
}

TEST(ParserTest, DefaultedStringField) {
  using ::tsdb2::proto::test::DefaultedStringField;
  EXPECT_THAT(Parse<DefaultedStringField>(""),
              IsOkAndHolds(DefaultedStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<DefaultedStringField>("field: \"sator\""),
              IsOkAndHolds(DefaultedStringField{.field = "sator"}));
  EXPECT_THAT(Parse<DefaultedStringField>("field: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: sator"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: FOO"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: 42"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<DefaultedStringField>("field: \"arepo\""),
              IsOkAndHolds(DefaultedStringField{.field = "arepo"}));
  EXPECT_THAT(Parse<DefaultedStringField>("field: \"sator\" field: \"arepo\""),
              IsOkAndHolds(DefaultedStringField{.field = "arepo"}));
}

TEST(ParserTest, RequiredStringField) {
  using ::tsdb2::proto::test::RequiredStringField;
  EXPECT_THAT(Parse<RequiredStringField>(""), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse<RequiredStringField>("field: \"lorem\""),
              IsOkAndHolds(RequiredStringField{.field = "lorem"}));
  EXPECT_THAT(Parse<RequiredStringField>("field: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: FOO"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: 42"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RequiredStringField>("field: \"ipsum\""),
              IsOkAndHolds(RequiredStringField{.field = "ipsum"}));
  EXPECT_THAT(Parse<RequiredStringField>("field: \"lorem\" field: \"ipsum\""),
              IsOkAndHolds(RequiredStringField{.field = "ipsum"}));
}

TEST(ParserTest, RepeatedStringField) {
  using ::tsdb2::proto::test::RepeatedStringField;
  EXPECT_THAT(Parse<RepeatedStringField>(""), IsOkAndHolds(RepeatedStringField()));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\""),
              IsOkAndHolds(RepeatedStringField{.field{"lorem"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\" field: \"ipsum\""),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\", field: \"ipsum\""),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\" field: \"ipsum\","),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\", field: \"ipsum\","),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\" field: \"ipsum\""),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\"; field: \"ipsum\""),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\" field: \"ipsum\";"),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"lorem\"; field: \"ipsum\";"),
              IsOkAndHolds(RepeatedStringField{.field{"lorem", "ipsum"}}));
  EXPECT_THAT(Parse<RepeatedStringField>("field: 12.34"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: 12.34f"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: FOO"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: 42"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: true"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse<RepeatedStringField>("field: \"sator\" field: \"arepo\" field: \"tenet\""),
              IsOkAndHolds(RepeatedStringField{.field{"sator", "arepo", "tenet"}}));
  EXPECT_THAT(Parse<RepeatedStringField>(R"pb(
                # element 1
                field: "sator"
                # element 2
                field: "arepo"
                # element 3
                field: "tenet"
              )pb"),
              IsOkAndHolds(RepeatedStringField{.field{"sator", "arepo", "tenet"}}));
  EXPECT_THAT(
      Parse<RepeatedStringField>("field: \"sator\" unknown_field: \"arepo\" field: \"tenet\""),
      IsOkAndHolds(RepeatedStringField{.field{"sator", "tenet"}}));
}

TEST(ParserTest, OptionalSubMessageField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  using ::tsdb2::proto::test::OptionalSubMessageField;
  EXPECT_THAT(Parse<OptionalSubMessageField>(""), IsOkAndHolds(OptionalSubMessageField{}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field:{}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>(" field:{}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field :{}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field: {}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field:{} "),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>(" field : {} "),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field{}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>(" field{}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field {}"),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field{} "),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>(" field {} "),
              IsOkAndHolds(OptionalSubMessageField{.field = OptionalEnumField{}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field: { color: COLOR_GREEN }"),
              IsOkAndHolds(OptionalSubMessageField{
                  .field = OptionalEnumField{.color = ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("field { color: COLOR_GREEN }"),
              IsOkAndHolds(OptionalSubMessageField{
                  .field = OptionalEnumField{.color = ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>("unknown_field: true field { color: COLOR_GREEN }"),
              IsOkAndHolds(OptionalSubMessageField{
                  .field = OptionalEnumField{.color = ColorEnum::COLOR_GREEN}}));
  EXPECT_THAT(Parse<OptionalSubMessageField>(R"pb(
                field { color: COLOR_GREEN }
                field { color: COLOR_RED }
              )pb"),
              IsOkAndHolds(OptionalSubMessageField{
                  .field = OptionalEnumField{.color = ColorEnum::COLOR_RED}}));
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

TEST(StringifierTest, EmptyMessage) {
  EXPECT_EQ(Stringify(tsdb2::proto::test::EmptyMessage{}), "");
}

TEST(StringifierTest, OptionalField) {
  EXPECT_EQ(Stringify(tsdb2::proto::test::OptionalField{}), "");
  EXPECT_EQ(Stringify(tsdb2::proto::test::OptionalField{.field = 12}), "field: 12\n");
  EXPECT_EQ(Stringify(tsdb2::proto::test::OptionalField{.field = 34}), "field: 34\n");
}

// TODO

}  // namespace
