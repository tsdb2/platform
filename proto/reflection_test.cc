#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/proto.h"
#include "proto/tests.pb.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::tsdb2::proto::BaseMessageDescriptor;
using ::tsdb2::proto::test::ColorEnum;

TEST(ReflectionTest, ColorEnum) {
  EXPECT_EQ(&tsdb2::proto::GetEnumDescriptor<ColorEnum>(),
            &tsdb2::proto::test::ColorEnum_ENUM_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::ColorEnum_ENUM_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetValueNames(),
              UnorderedElementsAre("COLOR_YELLOW", "COLOR_MAGENTA", "COLOR_CYAN", "COLOR_RED",
                                   "COLOR_GREEN", "COLOR_BLUE"));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_YELLOW"), IsOkAndHolds(-30));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_MAGENTA"), IsOkAndHolds(-20));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_CYAN"), IsOkAndHolds(-10));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_RED"), IsOkAndHolds(10));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_GREEN"), IsOkAndHolds(20));
  EXPECT_THAT(descriptor.GetValueForName("COLOR_BLUE"), IsOkAndHolds(30));
  EXPECT_THAT(descriptor.GetValueForName("foobar"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetNameForValue(-30), IsOkAndHolds("COLOR_YELLOW"));
  EXPECT_THAT(descriptor.GetNameForValue(-20), IsOkAndHolds("COLOR_MAGENTA"));
  EXPECT_THAT(descriptor.GetNameForValue(-10), IsOkAndHolds("COLOR_CYAN"));
  EXPECT_THAT(descriptor.GetNameForValue(10), IsOkAndHolds("COLOR_RED"));
  EXPECT_THAT(descriptor.GetNameForValue(20), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_THAT(descriptor.GetNameForValue(30), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_THAT(descriptor.GetNameForValue(0), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetNameForValue(123), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_YELLOW), IsOkAndHolds("COLOR_YELLOW"));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_MAGENTA), IsOkAndHolds("COLOR_MAGENTA"));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_CYAN), IsOkAndHolds("COLOR_CYAN"));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_RED), IsOkAndHolds("COLOR_RED"));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_GREEN), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_THAT(descriptor.GetValueName(ColorEnum::COLOR_BLUE), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_THAT(descriptor.GetValueName(static_cast<ColorEnum>(0)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetValueName(static_cast<ColorEnum>(123)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_YELLOW"), IsOkAndHolds(ColorEnum::COLOR_YELLOW));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_MAGENTA"), IsOkAndHolds(ColorEnum::COLOR_MAGENTA));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_CYAN"), IsOkAndHolds(ColorEnum::COLOR_CYAN));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_RED"), IsOkAndHolds(ColorEnum::COLOR_RED));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_GREEN"), IsOkAndHolds(ColorEnum::COLOR_GREEN));
  EXPECT_THAT(descriptor.GetNameValue("COLOR_BLUE"), IsOkAndHolds(ColorEnum::COLOR_BLUE));
  EXPECT_THAT(descriptor.GetNameValue("foobar"), StatusIs(absl::StatusCode::kInvalidArgument));
  ColorEnum value = ColorEnum::COLOR_GREEN;
  EXPECT_OK(descriptor.SetValueByName(&value, "COLOR_BLUE"));
  EXPECT_EQ(value, ColorEnum::COLOR_BLUE);
  EXPECT_THAT(descriptor.SetValueByName(&value, "foobar"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_EQ(value, ColorEnum::COLOR_BLUE);
}

TEST(ReflectionTest, EmptyMessage) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::EmptyMessage>(),
            &tsdb2::proto::test::EmptyMessage::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::EmptyMessage::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), IsEmpty());
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, OptionalField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalField>(),
            &tsdb2::proto::test::OptionalField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalInt32Field));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kInt32Field,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kInt32Field));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalField) {
  auto const& descriptor = tsdb2::proto::test::OptionalField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalField message;
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<int32_t> const*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<int32_t>*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<int32_t>*>(status_or_field.value())->emplace(42);
  EXPECT_THAT(message.field, Optional(42));
}

TEST(ReflectionTest, OptionalFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalField message{.field = 42};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<int32_t> const*>(Pointee(Optional(42)))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<int32_t>*>(Pointee(Optional(42)))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<int32_t>*>(status_or_field.value())->emplace(43);
  EXPECT_THAT(message.field, Optional(43));
}

TEST(ReflectionTest, RawField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::DefaultedField>(),
            &tsdb2::proto::test::DefaultedField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::DefaultedField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRawInt32Field));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kInt32Field,
                                BaseMessageDescriptor::FieldKind::kRaw)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kInt32Field));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRaw));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RawFieldValue) {
  auto const& descriptor = tsdb2::proto::test::DefaultedField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::DefaultedField message{.field = 24};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<int32_t const*>(Pointee(24))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<int32_t*>(Pointee(24))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  *std::get<int32_t*>(status_or_field.value()) = 25;
  EXPECT_THAT(message.field, 25);
}

TEST(ReflectionTest, RepeatedField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RepeatedField>(),
            &tsdb2::proto::test::RepeatedField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RepeatedField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRepeatedInt32Field));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kInt32Field,
                                BaseMessageDescriptor::FieldKind::kRepeated)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kInt32Field));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRepeated));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedFieldValues) {
  auto const& descriptor = tsdb2::proto::test::RepeatedField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedField message{.field{12, 34, 56}};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(
      descriptor.GetConstFieldValue(ref, "field"),
      IsOkAndHolds(VariantWith<std::vector<int32_t> const*>(Pointee(ElementsAre(12, 34, 56)))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::vector<int32_t>*>(Pointee(ElementsAre(12, 34, 56)))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::vector<int32_t>*>(status_or_field.value())->emplace_back(78);
  EXPECT_THAT(message.field, ElementsAre(12, 34, 56, 78));
}

TEST(ReflectionTest, OptionalEnumField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalEnumField>(),
            &tsdb2::proto::test::OptionalEnumField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalEnumField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("color"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("color"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalEnumField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("color"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kEnumField,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("color"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kEnumField));
  EXPECT_THAT(descriptor.GetFieldType("field"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("color"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("field"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalEnumField) {
  auto const& descriptor = tsdb2::proto::test::OptionalEnumField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalEnumField message;
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "color");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::OptionalEnum const>(status_or_const_field.value());
  EXPECT_FALSE(const_field.HasValue());
  EXPECT_FALSE(const_field.HasKnownValue());
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto status_or_field = descriptor.GetFieldValue(ptr, "color");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::OptionalEnum>(status_or_field.value());
  EXPECT_FALSE(field.HasValue());
  EXPECT_FALSE(field.HasKnownValue());
  EXPECT_OK(field.SetValue("COLOR_BLUE"));
  EXPECT_TRUE(field.HasValue());
  EXPECT_TRUE(field.HasKnownValue());
  EXPECT_THAT(field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_TRUE(const_field.HasValue());
  EXPECT_TRUE(const_field.HasKnownValue());
  EXPECT_THAT(const_field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(const_field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_THAT(message.color, Optional(ColorEnum::COLOR_BLUE));
}

TEST(ReflectionTest, OptionalEnumFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalEnumField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalEnumField message{.color = ColorEnum::COLOR_GREEN};
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "color");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::OptionalEnum const>(status_or_const_field.value());
  EXPECT_TRUE(const_field.HasValue());
  EXPECT_TRUE(const_field.HasKnownValue());
  EXPECT_THAT(const_field.GetValue(), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_EQ(const_field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_GREEN));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto status_or_field = descriptor.GetFieldValue(ptr, "color");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::OptionalEnum>(status_or_field.value());
  EXPECT_TRUE(field.HasValue());
  EXPECT_TRUE(field.HasKnownValue());
  EXPECT_THAT(field.GetValue(), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_EQ(field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_GREEN));
  EXPECT_OK(field.SetValue("COLOR_BLUE"));
  EXPECT_TRUE(field.HasValue());
  EXPECT_TRUE(field.HasKnownValue());
  EXPECT_THAT(field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_TRUE(const_field.HasValue());
  EXPECT_TRUE(const_field.HasKnownValue());
  EXPECT_THAT(const_field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(const_field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_THAT(message.color, Optional(ColorEnum::COLOR_BLUE));
}

TEST(ReflectionTest, RawEnumField) {
  auto const& descriptor = tsdb2::proto::test::DefaultedEnumField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::DefaultedEnumField message{.color = ColorEnum::COLOR_GREEN};
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "color");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::RawEnum const>(status_or_const_field.value());
  EXPECT_TRUE(const_field.HasKnownValue());
  EXPECT_THAT(const_field.GetValue(), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_EQ(const_field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_GREEN));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto status_or_field = descriptor.GetFieldValue(ptr, "color");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::RawEnum>(status_or_field.value());
  EXPECT_TRUE(field.HasKnownValue());
  EXPECT_THAT(field.GetValue(), IsOkAndHolds("COLOR_GREEN"));
  EXPECT_EQ(field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_GREEN));
  EXPECT_OK(field.SetValue("COLOR_BLUE"));
  EXPECT_TRUE(field.HasKnownValue());
  EXPECT_THAT(field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_TRUE(const_field.HasKnownValue());
  EXPECT_THAT(const_field.GetValue(), IsOkAndHolds("COLOR_BLUE"));
  EXPECT_EQ(const_field.GetUnderlyingValue(), tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE));
  EXPECT_THAT(message.color, ColorEnum::COLOR_BLUE);
}

TEST(ReflectionTest, RepeatedEnumField) {
  auto const& descriptor = tsdb2::proto::test::RepeatedEnumField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedEnumField message{.color{
      ColorEnum::COLOR_RED,
      ColorEnum::COLOR_GREEN,
      ColorEnum::COLOR_BLUE,
  }};
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "color");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::RepeatedEnum const>(status_or_const_field.value());
  EXPECT_TRUE(const_field.AllValuesAreKnown());
  EXPECT_THAT(const_field, ElementsAre("COLOR_RED", "COLOR_GREEN", "COLOR_BLUE"));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto status_or_field = descriptor.GetFieldValue(ptr, "color");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::RepeatedEnum>(status_or_field.value());
  EXPECT_TRUE(field.AllValuesAreKnown());
  EXPECT_THAT(field, ElementsAre("COLOR_RED", "COLOR_GREEN", "COLOR_BLUE"));
  EXPECT_OK(field.SetAllValues({"COLOR_CYAN", "COLOR_MAGENTA"}));
  EXPECT_TRUE(field.AllValuesAreKnown());
  EXPECT_THAT(field, ElementsAre("COLOR_CYAN", "COLOR_MAGENTA"));
  EXPECT_TRUE(const_field.AllValuesAreKnown());
  EXPECT_THAT(const_field, ElementsAre("COLOR_CYAN", "COLOR_MAGENTA"));
  EXPECT_THAT(message.color, ElementsAre(ColorEnum::COLOR_CYAN, ColorEnum::COLOR_MAGENTA));
  EXPECT_OK(field.SetAllValues({"COLOR_RED", "COLOR_CYAN", "COLOR_GREEN", "COLOR_MAGENTA"}));
  EXPECT_TRUE(field.AllValuesAreKnown());
  EXPECT_THAT(field, ElementsAre("COLOR_RED", "COLOR_CYAN", "COLOR_GREEN", "COLOR_MAGENTA"));
  EXPECT_TRUE(const_field.AllValuesAreKnown());
  EXPECT_THAT(const_field, ElementsAre("COLOR_RED", "COLOR_CYAN", "COLOR_GREEN", "COLOR_MAGENTA"));
  EXPECT_THAT(message.color, ElementsAre(ColorEnum::COLOR_RED, ColorEnum::COLOR_CYAN,
                                         ColorEnum::COLOR_GREEN, ColorEnum::COLOR_MAGENTA));
}

TEST(ReflectionTest, ManyFields) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::ManyFields>(),
            &tsdb2::proto::test::ManyFields::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::ManyFields::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(),
              UnorderedElementsAre("int32_field", "uint32_field", "int64_field", "uint64_field",
                                   "sint32_field", "sint64_field", "optional_fixed32_field",
                                   "defaulted_fixed32_field", "repeated_fixed32_field",
                                   "required_fixed32_field", "sfixed32_field", "fixed64_field",
                                   "sfixed64_field", "enum_field", "double_field", "float_field",
                                   "optional_bool_field", "defaulted_bool_field",
                                   "repeated_bool_field", "required_bool_field"));
}

TEST(ReflectionTest, OptionalStringField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalStringField>(),
            &tsdb2::proto::test::OptionalStringField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalStringField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalStringField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kStringField,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kStringField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalStringField) {
  auto const& descriptor = tsdb2::proto::test::OptionalStringField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalStringField message;
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<std::string> const*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<std::string>*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<std::string>*>(status_or_field.value())->emplace("lorem");
  EXPECT_THAT(message.field, Optional(Eq("lorem")));
}

TEST(ReflectionTest, OptionalStringFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalStringField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalStringField message{.field = "ipsum"};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(
      descriptor.GetConstFieldValue(ref, "field"),
      IsOkAndHolds(VariantWith<std::optional<std::string> const*>(Pointee(Optional(Eq("ipsum"))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(
      descriptor.GetFieldValue(ptr, "field"),
      IsOkAndHolds(VariantWith<std::optional<std::string>*>(Pointee(Optional(Eq("ipsum"))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<std::string>*>(status_or_field.value())->emplace("ipsum");
  EXPECT_THAT(message.field, Optional(Eq("ipsum")));
}

TEST(ReflectionTest, RawStringField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::DefaultedStringField>(),
            &tsdb2::proto::test::DefaultedStringField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::DefaultedStringField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRawStringField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kStringField,
                                BaseMessageDescriptor::FieldKind::kRaw)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kStringField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRaw));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RawStringFieldValue) {
  auto const& descriptor = tsdb2::proto::test::DefaultedStringField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::DefaultedStringField message{.field = "dolor"};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::string const*>(Pointee(Eq("dolor")))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::string*>(Pointee(Eq("dolor")))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  *std::get<std::string*>(status_or_field.value()) = "dolor";
  EXPECT_EQ(message.field, "dolor");
}

TEST(ReflectionTest, RepeatedStringField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RepeatedStringField>(),
            &tsdb2::proto::test::RepeatedStringField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RepeatedStringField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRepeatedStringField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kStringField,
                                BaseMessageDescriptor::FieldKind::kRepeated)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kStringField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRepeated));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedStringFieldValues) {
  auto const& descriptor = tsdb2::proto::test::RepeatedStringField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedStringField message{.field{"lorem", "ipsum", "dolor"}};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::vector<std::string> const*>(
                  Pointee(ElementsAre("lorem", "ipsum", "dolor")))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::vector<std::string>*>(
                  Pointee(ElementsAre("lorem", "ipsum", "dolor")))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::vector<std::string>*>(status_or_field.value())->emplace_back("amet");
  EXPECT_THAT(message.field, ElementsAre("lorem", "ipsum", "dolor", "amet"));
}

TEST(ReflectionTest, OptionalSubMessageField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalSubMessageField>(),
            &tsdb2::proto::test::OptionalSubMessageField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalSubMessageField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalSubMessageField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kSubMessageField,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kSubMessageField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalSubMessageField) {
  auto const& descriptor = tsdb2::proto::test::OptionalSubMessageField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalSubMessageField message;
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "field");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::OptionalSubMessage const>(status_or_const_field.value());
  EXPECT_FALSE(const_field.has_value());
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  auto status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::OptionalSubMessage>(status_or_field.value());
  EXPECT_FALSE(field.has_value());
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  field.Reset();
  EXPECT_TRUE(field.has_value());
  EXPECT_TRUE(const_field.has_value());
  EXPECT_THAT(message.field, Optional(tsdb2::proto::test::OptionalEnumField{}));
}

TEST(ReflectionTest, OptionalSubMessageFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalSubMessageField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalSubMessageField message{.field{
      std::in_place, tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_GREEN}}};
  tsdb2::proto::Message const& ref = message;
  auto const status_or_const_field = descriptor.GetConstFieldValue(ref, "field");
  EXPECT_OK(status_or_const_field);
  auto const& const_field =
      std::get<BaseMessageDescriptor::OptionalSubMessage const>(status_or_const_field.value());
  EXPECT_TRUE(const_field.has_value());
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  auto status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  auto& field = std::get<BaseMessageDescriptor::OptionalSubMessage>(status_or_field.value());
  EXPECT_TRUE(field.has_value());
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  field.Erase();
  EXPECT_FALSE(field.has_value());
  EXPECT_FALSE(const_field.has_value());
  EXPECT_THAT(message.field, std::nullopt);
}

TEST(ReflectionTest, RawSubMessageField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RequiredSubMessageField>(),
            &tsdb2::proto::test::RequiredSubMessageField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RequiredSubMessageField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRawSubMessageField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kSubMessageField,
                                BaseMessageDescriptor::FieldKind::kRaw)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kSubMessageField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRaw));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RawSubMessageFieldValue) {
  auto const& descriptor = tsdb2::proto::test::RequiredSubMessageField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RequiredSubMessageField message{.field{.color = ColorEnum::COLOR_GREEN}};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<BaseMessageDescriptor::RawSubMessage const>(_)));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<BaseMessageDescriptor::RawSubMessage>(_)));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedSubMessageField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RepeatedSubMessageField>(),
            &tsdb2::proto::test::RepeatedSubMessageField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RepeatedSubMessageField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRepeatedSubMessageField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kSubMessageField,
                                BaseMessageDescriptor::FieldKind::kRepeated)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kSubMessageField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRepeated));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedSubMessageFieldValues) {
  auto const& descriptor = tsdb2::proto::test::RepeatedSubMessageField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedSubMessageField message{.field{
      tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_RED},
      tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_GREEN},
      tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_BLUE},
  }};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<BaseMessageDescriptor::RepeatedSubMessage const>(
                  Property(&BaseMessageDescriptor::RepeatedSubMessage::size, 3))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<BaseMessageDescriptor::RepeatedSubMessage>(
                  Property(&BaseMessageDescriptor::RepeatedSubMessage::size, 3))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<BaseMessageDescriptor::RepeatedSubMessage>(status_or_field.value()).Clear();
  EXPECT_THAT(message.field, IsEmpty());
}

TEST(ReflectionTest, OptionalTimestampField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalTimestampField>(),
            &tsdb2::proto::test::OptionalTimestampField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalTimestampField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalTimeField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kTimeField,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kTimeField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalTimestampField) {
  auto const& descriptor = tsdb2::proto::test::OptionalTimestampField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalTimestampField message;
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Time> const*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Time>*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<absl::Time>*>(status_or_field.value())
      ->emplace(absl::UnixEpoch() + absl::Seconds(42));
  EXPECT_THAT(message.field, Optional(absl::UnixEpoch() + absl::Seconds(42)));
}

TEST(ReflectionTest, OptionalTimestampFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalTimestampField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalTimestampField message{
      .field = absl::UnixEpoch() + absl::Seconds(123),
  };
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Time> const*>(
                  Pointee(Optional(absl::UnixEpoch() + absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Time>*>(
                  Pointee(Optional(absl::UnixEpoch() + absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<absl::Time>*>(status_or_field.value())
      ->emplace(absl::UnixEpoch() + absl::Seconds(456));
  EXPECT_THAT(message.field, Optional(absl::UnixEpoch() + absl::Seconds(456)));
}

TEST(ReflectionTest, RawTimestampField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RequiredTimestampField>(),
            &tsdb2::proto::test::RequiredTimestampField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RequiredTimestampField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRawTimeField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kTimeField,
                                BaseMessageDescriptor::FieldKind::kRaw)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kTimeField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRaw));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RawTimestampFieldValue) {
  auto const& descriptor = tsdb2::proto::test::RequiredTimestampField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RequiredTimestampField message{
      .field = absl::UnixEpoch() + absl::Seconds(123),
  };
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<absl::Time const*>(
                  Pointee(Eq(absl::UnixEpoch() + absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(
      descriptor.GetFieldValue(ptr, "field"),
      IsOkAndHolds(VariantWith<absl::Time*>(Pointee(Eq(absl::UnixEpoch() + absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  *std::get<absl::Time*>(status_or_field.value()) = absl::UnixEpoch() + absl::Seconds(456);
  EXPECT_EQ(message.field, absl::UnixEpoch() + absl::Seconds(456));
}

TEST(ReflectionTest, RepeatedTimestampField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RepeatedTimestampField>(),
            &tsdb2::proto::test::RepeatedTimestampField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RepeatedTimestampField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRepeatedTimeField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kTimeField,
                                BaseMessageDescriptor::FieldKind::kRepeated)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kTimeField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRepeated));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedTimestampFieldValues) {
  auto const& descriptor = tsdb2::proto::test::RepeatedTimestampField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedTimestampField message{.field{
      absl::UnixEpoch() + absl::Seconds(12),
      absl::UnixEpoch() + absl::Seconds(34),
      absl::UnixEpoch() + absl::Seconds(56),
  }};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::vector<absl::Time> const*>(Pointee(ElementsAre(
                  absl::UnixEpoch() + absl::Seconds(12), absl::UnixEpoch() + absl::Seconds(34),
                  absl::UnixEpoch() + absl::Seconds(56))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::vector<absl::Time>*>(Pointee(ElementsAre(
                  absl::UnixEpoch() + absl::Seconds(12), absl::UnixEpoch() + absl::Seconds(34),
                  absl::UnixEpoch() + absl::Seconds(56))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::vector<absl::Time>*>(status_or_field.value())
      ->emplace_back(absl::UnixEpoch() + absl::Seconds(78));
  EXPECT_THAT(
      message.field,
      ElementsAre(absl::UnixEpoch() + absl::Seconds(12), absl::UnixEpoch() + absl::Seconds(34),
                  absl::UnixEpoch() + absl::Seconds(56), absl::UnixEpoch() + absl::Seconds(78)));
}

TEST(ReflectionTest, OptionalDurationField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::OptionalDurationField>(),
            &tsdb2::proto::test::OptionalDurationField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::OptionalDurationField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kOptionalDurationField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kDurationField,
                                BaseMessageDescriptor::FieldKind::kOptional)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kDurationField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kOptional));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, EmptyOptionalDurationField) {
  auto const& descriptor = tsdb2::proto::test::OptionalDurationField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalDurationField message;
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(
      descriptor.GetConstFieldValue(ref, "field"),
      IsOkAndHolds(VariantWith<std::optional<absl::Duration> const*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Duration>*>(Pointee(std::nullopt))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<absl::Duration>*>(status_or_field.value())->emplace(absl::Seconds(42));
  EXPECT_THAT(message.field, Optional(absl::Seconds(42)));
}

TEST(ReflectionTest, OptionalDurationFieldValue) {
  auto const& descriptor = tsdb2::proto::test::OptionalDurationField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::OptionalDurationField message{
      .field = absl::Seconds(123),
  };
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Duration> const*>(
                  Pointee(Optional(absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::optional<absl::Duration>*>(
                  Pointee(Optional(absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::optional<absl::Duration>*>(status_or_field.value())->emplace(absl::Seconds(456));
  EXPECT_THAT(message.field, Optional(absl::Seconds(456)));
}

TEST(ReflectionTest, RawDurationField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RequiredDurationField>(),
            &tsdb2::proto::test::RequiredDurationField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RequiredDurationField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRawDurationField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kDurationField,
                                BaseMessageDescriptor::FieldKind::kRaw)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kDurationField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRaw));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RawDurationFieldValue) {
  auto const& descriptor = tsdb2::proto::test::RequiredDurationField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RequiredDurationField message{
      .field = absl::Seconds(123),
  };
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<absl::Duration const*>(Pointee(Eq(absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<absl::Duration*>(Pointee(Eq(absl::Seconds(123))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  *std::get<absl::Duration*>(status_or_field.value()) = absl::Seconds(456);
  EXPECT_EQ(message.field, absl::Seconds(456));
}

TEST(ReflectionTest, RepeatedDurationField) {
  EXPECT_EQ(&tsdb2::proto::GetMessageDescriptor<tsdb2::proto::test::RepeatedDurationField>(),
            &tsdb2::proto::test::RepeatedDurationField::MESSAGE_DESCRIPTOR);
  auto const& descriptor = tsdb2::proto::test::RepeatedDurationField::MESSAGE_DESCRIPTOR;
  EXPECT_THAT(descriptor.GetAllFieldNames(), ElementsAre("field"));
  EXPECT_THAT(descriptor.GetLabeledFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::LabeledFieldType::kRepeatedDurationField));
  EXPECT_THAT(descriptor.GetLabeledFieldType("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("field"),
              IsOkAndHolds(Pair(BaseMessageDescriptor::FieldType::kDurationField,
                                BaseMessageDescriptor::FieldKind::kRepeated)));
  EXPECT_THAT(descriptor.GetFieldTypeAndKind("lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldType("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldType::kDurationField));
  EXPECT_THAT(descriptor.GetFieldType("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(descriptor.GetFieldKind("field"),
              IsOkAndHolds(BaseMessageDescriptor::FieldKind::kRepeated));
  EXPECT_THAT(descriptor.GetFieldKind("lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ReflectionTest, RepeatedDurationFieldValues) {
  auto const& descriptor = tsdb2::proto::test::RepeatedDurationField::MESSAGE_DESCRIPTOR;
  tsdb2::proto::test::RepeatedDurationField message{.field{
      absl::Seconds(12),
      absl::Seconds(34),
      absl::Seconds(56),
  }};
  tsdb2::proto::Message const& ref = message;
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "field"),
              IsOkAndHolds(VariantWith<std::vector<absl::Duration> const*>(
                  Pointee(ElementsAre(absl::Seconds(12), absl::Seconds(34), absl::Seconds(56))))));
  EXPECT_THAT(descriptor.GetConstFieldValue(ref, "lorem"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  tsdb2::proto::Message* const ptr = &message;
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "field"),
              IsOkAndHolds(VariantWith<std::vector<absl::Duration>*>(
                  Pointee(ElementsAre(absl::Seconds(12), absl::Seconds(34), absl::Seconds(56))))));
  EXPECT_THAT(descriptor.GetFieldValue(ptr, "lorem"), StatusIs(absl::StatusCode::kInvalidArgument));
  auto const status_or_field = descriptor.GetFieldValue(ptr, "field");
  EXPECT_OK(status_or_field);
  std::get<std::vector<absl::Duration>*>(status_or_field.value())->emplace_back(absl::Seconds(78));
  EXPECT_THAT(message.field, ElementsAre(absl::Seconds(12), absl::Seconds(34), absl::Seconds(56),
                                         absl::Seconds(78)));
}

}  // namespace
