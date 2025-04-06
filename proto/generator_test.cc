#include "proto/generator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/fingerprint.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/proto.h"
#include "proto/tests.pb.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::tsdb2::proto::generator::MakeHeaderFileName;
using ::tsdb2::proto::generator::MakeSourceFileName;
using ::tsdb2::proto::test::ColorEnum;

TEST(GeneratorTest, MakeHeaderFileName) {
  EXPECT_EQ(MakeHeaderFileName("foo.proto"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.bar"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.bar.proto"), "foo.bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.proto.bar"), "foo.proto.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.proto"), "foo/bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.baz"), "foo/bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.baz.proto"), "foo/bar.baz.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.proto.baz"), "foo/bar.proto.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar"), "foo/bar.pb.h");
}

TEST(GeneratorTest, MakeSourceFileName) {
  EXPECT_EQ(MakeSourceFileName("foo.proto"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.bar"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.bar.proto"), "foo.bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.proto.bar"), "foo.proto.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.proto"), "foo/bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.baz"), "foo/bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.baz.proto"), "foo/bar.baz.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.proto.baz"), "foo/bar.proto.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar"), "foo/bar.pb.cc");
}

TEST(GeneratorTest, ColorEnum) {
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_RED), 10);
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_GREEN), 20);
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_BLUE), 30);
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_CYAN), -10);
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_MAGENTA), -20);
  EXPECT_EQ(tsdb2::util::to_underlying(ColorEnum::COLOR_YELLOW), -30);
}

TEST(GeneratorTest, Empty) {
  tsdb2::proto::test::EmptyMessage m1;
  tsdb2::proto::test::EmptyMessage m2;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::EmptyMessage::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::EmptyMessage::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OptionalField) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalField>().field),
                              std::optional<int32_t>>));
  tsdb2::proto::test::OptionalField m1;
  tsdb2::proto::test::OptionalField m2{.field{123}};
  EXPECT_FALSE(m1.field.has_value());
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace(123);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, DefaultedField) {
  EXPECT_TRUE((
      std::is_same_v<decltype(std::declval<tsdb2::proto::test::DefaultedField>().field), int32_t>));
  tsdb2::proto::test::DefaultedField m1;
  tsdb2::proto::test::DefaultedField m2{.field = 123};
  EXPECT_EQ(m1.field, 42);
  EXPECT_EQ(m2.field, 123);
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field = 123;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::DefaultedField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::DefaultedField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedField) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedField>().field),
                              std::vector<int32_t>>));
  tsdb2::proto::test::RepeatedField m1;
  tsdb2::proto::test::RepeatedField m2{.field{12}};
  tsdb2::proto::test::RepeatedField m3{.field{34, 56}};
  EXPECT_THAT(m1.field, IsEmpty());
  EXPECT_THAT(m2.field, ElementsAre(12));
  EXPECT_THAT(m3.field, ElementsAre(34, 56));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.field = {34, 56};
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedField::Decode(encoded.span()), IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredField>().field), int32_t>));
  tsdb2::proto::test::RequiredField m1;
  tsdb2::proto::test::RequiredField m2{.field = 123};
  EXPECT_EQ(m1.field, 0);
  EXPECT_EQ(m2.field, 123);
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field = 123;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OptionalEnumField) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalEnumField>().color),
                              std::optional<ColorEnum>>));
  tsdb2::proto::test::OptionalEnumField m1;
  tsdb2::proto::test::OptionalEnumField m2{.color{ColorEnum::COLOR_GREEN}};
  EXPECT_FALSE(m1.color.has_value());
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.color.emplace(ColorEnum::COLOR_GREEN);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalEnumField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalEnumField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, DefaultedEnumField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::DefaultedEnumField>().color),
                      ColorEnum>));
  tsdb2::proto::test::DefaultedEnumField m1;
  tsdb2::proto::test::DefaultedEnumField m2{.color = ColorEnum::COLOR_GREEN};
  EXPECT_EQ(m1.color, ColorEnum::COLOR_CYAN);
  EXPECT_EQ(m2.color, ColorEnum::COLOR_GREEN);
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.color = ColorEnum::COLOR_GREEN;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::DefaultedEnumField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::DefaultedEnumField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedEnumField) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedEnumField>().color),
                              std::vector<ColorEnum>>));
  tsdb2::proto::test::RepeatedEnumField m1;
  tsdb2::proto::test::RepeatedEnumField m2{.color{ColorEnum::COLOR_RED}};
  tsdb2::proto::test::RepeatedEnumField m3{.color{ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE}};
  EXPECT_THAT(m1.color, IsEmpty());
  EXPECT_THAT(m2.color, ElementsAre(ColorEnum::COLOR_RED));
  EXPECT_THAT(m3.color, ElementsAre(ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.color = {ColorEnum::COLOR_GREEN, ColorEnum::COLOR_BLUE};
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedEnumField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedEnumField::Decode(encoded.span()), IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredEnumField) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredEnumField>().color),
                              ColorEnum>));
  tsdb2::proto::test::RequiredEnumField m1;
  tsdb2::proto::test::RequiredEnumField m2{.color = ColorEnum::COLOR_GREEN};
  EXPECT_EQ(m1.color, ColorEnum::COLOR_YELLOW);
  EXPECT_EQ(m2.color, ColorEnum::COLOR_GREEN);
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.color = ColorEnum::COLOR_GREEN;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredEnumField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredEnumField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, ManyFields) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().int32_field),
                              std::optional<int32_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().uint32_field),
                              std::optional<uint32_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().int64_field),
                              std::optional<int64_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().uint64_field),
                              std::optional<uint64_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sint32_field),
                              std::optional<int32_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sint64_field),
                              std::optional<int64_t>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::ManyFields>().optional_fixed32_field),
               std::optional<uint32_t>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::ManyFields>().defaulted_fixed32_field),
               uint32_t>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::ManyFields>().repeated_fixed32_field),
               std::vector<uint32_t>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::ManyFields>().required_fixed32_field),
               uint32_t>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sfixed32_field),
                      std::optional<int32_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().fixed64_field),
                      std::optional<uint64_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sfixed64_field),
                      std::optional<int64_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().enum_field),
                              std::optional<ColorEnum>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().double_field),
                              std::optional<double>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().float_field),
                              std::optional<float>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().optional_bool_field),
                      std::optional<bool>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().defaulted_bool_field),
                      bool>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().repeated_bool_field),
                      std::vector<bool>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().required_bool_field),
                      bool>));
  tsdb2::proto::test::ManyFields m;
  EXPECT_FALSE(m.int32_field.has_value());
  EXPECT_FALSE(m.uint32_field.has_value());
  EXPECT_FALSE(m.int64_field.has_value());
  EXPECT_FALSE(m.uint64_field.has_value());
  EXPECT_FALSE(m.sint32_field.has_value());
  EXPECT_FALSE(m.sint64_field.has_value());
  EXPECT_FALSE(m.optional_fixed32_field.has_value());
  EXPECT_EQ(m.defaulted_fixed32_field, 123);
  EXPECT_THAT(m.repeated_fixed32_field, IsEmpty());
  EXPECT_EQ(m.required_fixed32_field, 0);
  EXPECT_FALSE(m.sfixed32_field.has_value());
  EXPECT_FALSE(m.fixed64_field.has_value());
  EXPECT_FALSE(m.sfixed64_field.has_value());
  EXPECT_FALSE(m.enum_field.has_value());
  EXPECT_FALSE(m.double_field.has_value());
  EXPECT_FALSE(m.float_field.has_value());
  EXPECT_FALSE(m.optional_bool_field.has_value());
  EXPECT_EQ(m.defaulted_bool_field, true);
  EXPECT_THAT(m.repeated_bool_field, IsEmpty());
  EXPECT_EQ(m.required_bool_field, false);
  m.int32_field = -12;
  m.uint32_field = 34;
  m.int64_field = -56;
  m.uint64_field = 78;
  m.sint32_field = -12;
  m.sint64_field = -34;
  m.optional_fixed32_field = 12;
  m.defaulted_fixed32_field = 34;
  m.repeated_fixed32_field = {56, 34, 12};
  m.required_fixed32_field = 56;
  m.sfixed32_field = -78;
  m.fixed64_field = 12;
  m.sfixed64_field = -34;
  m.enum_field = ColorEnum::COLOR_GREEN;
  m.double_field = 3.141;
  m.float_field = 2.718;
  m.optional_bool_field = true;
  m.defaulted_bool_field = false;
  m.repeated_bool_field = {false, true, false, true};
  m.required_bool_field = true;
  auto const encoded = tsdb2::proto::test::ManyFields::Encode(m).Flatten();
  EXPECT_THAT(tsdb2::proto::test::ManyFields::Decode(encoded.span()),
              IsOkAndHolds(tsdb2::proto::test::ManyFields{
                  .int32_field = -12,
                  .uint32_field = 34,
                  .int64_field = -56,
                  .uint64_field = 78,
                  .sint32_field = -12,
                  .sint64_field = -34,
                  .optional_fixed32_field = 12,
                  .defaulted_fixed32_field = 34,
                  .repeated_fixed32_field{56, 34, 12},
                  .required_fixed32_field = 56,
                  .sfixed32_field = -78,
                  .fixed64_field = 12,
                  .sfixed64_field = -34,
                  .enum_field = ColorEnum::COLOR_GREEN,
                  .double_field = 3.141,
                  .float_field = 2.718,
                  .optional_bool_field = true,
                  .defaulted_bool_field = false,
                  .repeated_bool_field{false, true, false, true},
                  .required_bool_field = true,
              }));
}

TEST(GeneratorTest, OptionalStringField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalStringField>().field),
                      std::optional<std::string>>));
  tsdb2::proto::test::OptionalStringField m1;
  tsdb2::proto::test::OptionalStringField m2{.field{"lorem"}};
  EXPECT_FALSE(m1.field.has_value());
  EXPECT_THAT(m2.field, Optional<std::string>("lorem"));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace("lorem");
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalStringField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalStringField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, DefaultedStringField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::DefaultedStringField>().field),
                      std::string>));
  tsdb2::proto::test::DefaultedStringField m1;
  tsdb2::proto::test::DefaultedStringField m2{.field{"ipsum"}};
  EXPECT_EQ(m1.field, "lorem");
  EXPECT_EQ(m2.field, "ipsum");
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_FALSE(m1 <= m2);
  EXPECT_TRUE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  m1.field = "ipsum";
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::DefaultedStringField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::DefaultedStringField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedStringField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedStringField>().field),
                      std::vector<std::string>>));
  tsdb2::proto::test::RepeatedStringField m1;
  tsdb2::proto::test::RepeatedStringField m2{.field{"lorem"}};
  tsdb2::proto::test::RepeatedStringField m3{.field{"sator", "arepo"}};
  EXPECT_THAT(m1.field, IsEmpty());
  EXPECT_THAT(m2.field, ElementsAre("lorem"));
  EXPECT_THAT(m3.field, ElementsAre("sator", "arepo"));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.field = {"sator", "arepo"};
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedStringField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedStringField::Decode(encoded.span()), IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredStringField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredStringField>().field),
                      std::string>));
  tsdb2::proto::test::RequiredStringField m1;
  tsdb2::proto::test::RequiredStringField m2{.field = "lorem"};
  EXPECT_EQ(m1.field, "");
  EXPECT_EQ(m2.field, "lorem");
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field = "lorem";
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredStringField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredStringField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OptionalSubMessageField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalSubMessageField>().field),
                      std::optional<OptionalEnumField>>));
  tsdb2::proto::test::OptionalSubMessageField m1;
  tsdb2::proto::test::OptionalSubMessageField m2{
      .field{OptionalEnumField{.color = ColorEnum::COLOR_GREEN}}};
  EXPECT_FALSE(m1.field.has_value());
  EXPECT_THAT(m2.field,
              Optional(Field(&OptionalEnumField::color, Optional(ColorEnum::COLOR_GREEN))));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace(OptionalEnumField{.color = ColorEnum::COLOR_GREEN});
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalSubMessageField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalSubMessageField::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedSubMessageField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedSubMessageField>().field),
                      std::vector<OptionalEnumField>>));
  tsdb2::proto::test::RepeatedSubMessageField m1;
  tsdb2::proto::test::RepeatedSubMessageField m2{
      .field{OptionalEnumField{.color = ColorEnum::COLOR_RED}}};
  tsdb2::proto::test::RepeatedSubMessageField m3{.field{
      OptionalEnumField{.color = ColorEnum::COLOR_GREEN},
      OptionalEnumField{.color = ColorEnum::COLOR_BLUE},
  }};
  EXPECT_THAT(m1.field, IsEmpty());
  EXPECT_THAT(m2.field,
              ElementsAre(Field(&OptionalEnumField::color, Optional(ColorEnum::COLOR_RED))));
  EXPECT_THAT(m3.field,
              ElementsAre(Field(&OptionalEnumField::color, Optional(ColorEnum::COLOR_GREEN)),
                          Field(&OptionalEnumField::color, Optional(ColorEnum::COLOR_BLUE))));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.field = {
      OptionalEnumField{.color = ColorEnum::COLOR_GREEN},
      OptionalEnumField{.color = ColorEnum::COLOR_BLUE},
  };
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedSubMessageField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedSubMessageField::Decode(encoded.span()),
              IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredSubMessageField) {
  using ::tsdb2::proto::test::OptionalEnumField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredSubMessageField>().field),
                      OptionalEnumField>));
  tsdb2::proto::test::RequiredSubMessageField m1;
  tsdb2::proto::test::RequiredSubMessageField m2{.field{.color = ColorEnum::COLOR_GREEN}};
  EXPECT_EQ(m1.field.color, std::nullopt);
  EXPECT_THAT(m2.field.color, Optional(ColorEnum::COLOR_GREEN));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.color.emplace(ColorEnum::COLOR_GREEN);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredSubMessageField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredSubMessageField::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, OptionalTimestampField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalTimestampField>().field),
                      std::optional<absl::Time>>));
  tsdb2::proto::test::OptionalTimestampField m1;
  tsdb2::proto::test::OptionalTimestampField m2{.field{absl::UnixEpoch() + absl::Seconds(42)}};
  EXPECT_FALSE(m1.field.has_value());
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace(absl::UnixEpoch() + absl::Seconds(42));
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalTimestampField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalTimestampField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedTimestampField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedTimestampField>().field),
                      std::vector<absl::Time>>));
  tsdb2::proto::test::RepeatedTimestampField m1;
  tsdb2::proto::test::RepeatedTimestampField m2{.field{absl::UnixEpoch() + absl::Seconds(42)}};
  tsdb2::proto::test::RepeatedTimestampField m3{
      .field{absl::UnixEpoch() + absl::Seconds(12), absl::UnixEpoch() + absl::Seconds(34)}};
  EXPECT_THAT(m1.field, IsEmpty());
  EXPECT_THAT(m2.field, ElementsAre(absl::UnixEpoch() + absl::Seconds(42)));
  EXPECT_THAT(m3.field, ElementsAre(absl::UnixEpoch() + absl::Seconds(12),
                                    absl::UnixEpoch() + absl::Seconds(34)));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.field = {absl::UnixEpoch() + absl::Seconds(12), absl::UnixEpoch() + absl::Seconds(34)};
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedTimestampField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedTimestampField::Decode(encoded.span()), IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredTimestampField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredTimestampField>().field),
                      absl::Time>));
  tsdb2::proto::test::RequiredTimestampField m1;
  tsdb2::proto::test::RequiredTimestampField m2{.field = absl::UnixEpoch() + absl::Seconds(42)};
  EXPECT_EQ(m1.field, absl::UnixEpoch());
  EXPECT_EQ(m2.field, absl::UnixEpoch() + absl::Seconds(42));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field += absl::Seconds(42);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredTimestampField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredTimestampField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OptionalDurationField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalDurationField>().field),
                      std::optional<absl::Duration>>));
  tsdb2::proto::test::OptionalDurationField m1;
  tsdb2::proto::test::OptionalDurationField m2{.field{absl::Seconds(42)}};
  EXPECT_FALSE(m1.field.has_value());
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace(absl::Seconds(42));
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OptionalDurationField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OptionalDurationField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, RepeatedDurationField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RepeatedDurationField>().field),
                      std::vector<absl::Duration>>));
  tsdb2::proto::test::RepeatedDurationField m1;
  tsdb2::proto::test::RepeatedDurationField m2{.field{absl::Seconds(42)}};
  tsdb2::proto::test::RepeatedDurationField m3{.field{absl::Seconds(12), absl::Seconds(34)}};
  EXPECT_THAT(m1.field, IsEmpty());
  EXPECT_THAT(m2.field, ElementsAre(absl::Seconds(42)));
  EXPECT_THAT(m3.field, ElementsAre(absl::Seconds(12), absl::Seconds(34)));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m2.field = {absl::Seconds(12), absl::Seconds(34)};
  EXPECT_EQ(absl::HashOf(m2), absl::HashOf(m3));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m2), tsdb2::common::FingerprintOf(m3));
  EXPECT_TRUE(m2 == m3);
  EXPECT_FALSE(m2 != m3);
  EXPECT_FALSE(m2 < m3);
  EXPECT_TRUE(m2 <= m3);
  EXPECT_FALSE(m2 > m3);
  EXPECT_TRUE(m2 >= m3);
  auto const encoded = tsdb2::proto::test::RepeatedDurationField::Encode(m2).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RepeatedDurationField::Decode(encoded.span()), IsOkAndHolds(m3));
}

TEST(GeneratorTest, RequiredDurationField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::RequiredDurationField>().field),
                      absl::Duration>));
  tsdb2::proto::test::RequiredDurationField m1;
  tsdb2::proto::test::RequiredDurationField m2{.field = absl::Seconds(42)};
  EXPECT_EQ(m1.field, absl::ZeroDuration());
  EXPECT_EQ(m2.field, absl::Seconds(42));
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field += absl::Seconds(42);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::RequiredDurationField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::RequiredDurationField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldVariant1) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfField>().field),
                              std::variant<std::monostate, int32_t, int64_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  tsdb2::proto::test::OneOfField m1;
  tsdb2::proto::test::OneOfField m2{.field{std::in_place_index<1>, 42}};
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace<1>(42);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldVariant2) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfField>().field),
                              std::variant<std::monostate, int32_t, int64_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  tsdb2::proto::test::OneOfField m1;
  tsdb2::proto::test::OneOfField m2{.field{std::in_place_index<2>, 42}};
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace<2>(42);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldVariant3) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfField>().field),
                              std::variant<std::monostate, int32_t, int64_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  tsdb2::proto::test::OneOfField m1;
  tsdb2::proto::test::OneOfField m2{.field{std::in_place_index<3>, "lorem"}};
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace<3>("lorem");
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldVariant4) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfField>().field),
                              std::variant<std::monostate, int32_t, int64_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  tsdb2::proto::test::OneOfField m1;
  tsdb2::proto::test::OneOfField m2{.field{std::in_place_index<4>, ColorEnum::COLOR_GREEN}};
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace<4>(ColorEnum::COLOR_GREEN);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldVariant5) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfField>().field),
                              std::variant<std::monostate, int32_t, int64_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  tsdb2::proto::test::OneOfField m1;
  tsdb2::proto::test::OneOfField m2{
      .field{std::in_place_index<5>, tsdb2::proto::test::OptionalStringField{.field = "sator"}}};
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field.emplace<5>(tsdb2::proto::test::OptionalStringField{.field = "sator"});
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfField::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfField::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, SomeOneOfFields) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::SomeOneOfFields>().int32_field),
                      std::optional<int32_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::SomeOneOfFields>().int64_field),
                      std::optional<int64_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::SomeOneOfFields>().field),
                              std::variant<std::monostate, int32_t, std::string, ColorEnum,
                                           OptionalStringField, absl::Time, absl::Duration>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::SomeOneOfFields>().string_field),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::SomeOneOfFields>().bool_field),
                      std::optional<bool>>));
  tsdb2::proto::test::SomeOneOfFields m1;
  tsdb2::proto::test::SomeOneOfFields m2{
      .int32_field = 42,
      .int64_field = 24,
      .field{std::in_place_index<1>, 42},
      .string_field = "lorem",
      .bool_field = true,
  };
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.int32_field = 42;
  m1.int64_field = 24;
  m1.field.emplace<1>(42);
  m1.string_field = "lorem";
  m1.bool_field = true;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::SomeOneOfFields::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::SomeOneOfFields::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldWithRepeatedVariants1) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field1),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field1),
          std::optional<bool>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().field),
               std::variant<std::monostate, int32_t, int32_t, std::string, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field2),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field2),
          std::optional<bool>>));
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m1;
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m2{
      .string_field1 = "sator",
      .bool_field1 = false,
      .field{std::in_place_index<1>, 42},
      .string_field2 = "arepo",
      .bool_field2 = true,
  };
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.string_field1 = "sator";
  m1.bool_field1 = false;
  m1.field.emplace<1>(42);
  m1.string_field2 = "arepo";
  m1.bool_field2 = true;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldWithRepeatedVariants2) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field1),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field1),
          std::optional<bool>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().field),
               std::variant<std::monostate, int32_t, int32_t, std::string, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field2),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field2),
          std::optional<bool>>));
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m1;
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m2{
      .string_field1 = "sator",
      .bool_field1 = false,
      .field{std::in_place_index<2>, 24},
      .string_field2 = "arepo",
      .bool_field2 = true,
  };
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.string_field1 = "sator";
  m1.bool_field1 = false;
  m1.field.emplace<2>(24);
  m1.string_field2 = "arepo";
  m1.bool_field2 = true;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldWithRepeatedVariants3) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field1),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field1),
          std::optional<bool>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().field),
               std::variant<std::monostate, int32_t, int32_t, std::string, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field2),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field2),
          std::optional<bool>>));
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m1;
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m2{
      .string_field1 = "sator",
      .bool_field1 = false,
      .field{std::in_place_index<3>, "tenet"},
      .string_field2 = "arepo",
      .bool_field2 = true,
  };
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.string_field1 = "sator";
  m1.bool_field1 = false;
  m1.field.emplace<3>("tenet");
  m1.string_field2 = "arepo";
  m1.bool_field2 = true;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, OneOfFieldWithRepeatedVariants4) {
  using ::tsdb2::proto::test::OptionalStringField;
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field1),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field1),
          std::optional<bool>>));
  EXPECT_TRUE((std::is_same_v<
               decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().field),
               std::variant<std::monostate, int32_t, int32_t, std::string, std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>()
                                   .string_field2),
                      std::optional<std::string>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::OneOfFieldWithRepeatedVariants>().bool_field2),
          std::optional<bool>>));
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m1;
  tsdb2::proto::test::OneOfFieldWithRepeatedVariants m2{
      .string_field1 = "sator",
      .bool_field1 = false,
      .field{std::in_place_index<4>, "opera"},
      .string_field2 = "arepo",
      .bool_field2 = true,
  };
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.string_field1 = "sator";
  m1.bool_field1 = false;
  m1.field.emplace<4>("opera");
  m1.string_field2 = "arepo";
  m1.bool_field2 = true;
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::OneOfFieldWithRepeatedVariants::Decode(encoded.span()),
              IsOkAndHolds(m2));
}

TEST(GeneratorTest, NestedEnum) {
  using ::tsdb2::proto::test::Nesting;
  EXPECT_EQ(tsdb2::util::to_underlying(Nesting::SatorEnum::SATOR_AREPO), 0);
  EXPECT_EQ(tsdb2::util::to_underlying(Nesting::SatorEnum::SATOR_TENET), 1);
  EXPECT_EQ(tsdb2::util::to_underlying(Nesting::SatorEnum::SATOR_OPERA), 2);
  EXPECT_EQ(tsdb2::util::to_underlying(Nesting::SatorEnum::SATOR_ROTAS), 3);
}

TEST(GeneratorTest, NestedMessage) {
  using ::tsdb2::proto::test::Nesting;
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<Nesting::NestedMessage>().field1),
                              std::optional<int32_t>>));
  Nesting::NestedMessage m1;
  Nesting::NestedMessage m2{.field1{123}};
  EXPECT_FALSE(m1.field1.has_value());
  EXPECT_NE(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_NE(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_FALSE(m1 == m2);
  EXPECT_TRUE(m1 != m2);
  EXPECT_TRUE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_FALSE(m1 >= m2);
  m1.field1.emplace(123);
  EXPECT_EQ(absl::HashOf(m1), absl::HashOf(m2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(m1), tsdb2::common::FingerprintOf(m2));
  EXPECT_TRUE(m1 == m2);
  EXPECT_FALSE(m1 != m2);
  EXPECT_FALSE(m1 < m2);
  EXPECT_TRUE(m1 <= m2);
  EXPECT_FALSE(m1 > m2);
  EXPECT_TRUE(m1 >= m2);
  auto const encoded = Nesting::NestedMessage::Encode(m1).Flatten();
  EXPECT_THAT(Nesting::NestedMessage::Decode(encoded.span()), IsOkAndHolds(m2));
}

TEST(GeneratorTest, Nesting) {
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::Nesting>().field2),
                              std::optional<int32_t>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::Nesting>().field3),
                              std::optional<tsdb2::proto::test::Nesting::SatorEnum>>));
  EXPECT_TRUE((std::is_same_v<decltype(std::declval<tsdb2::proto::test::Nesting>().field4),
                              std::optional<tsdb2::proto::test::Nesting::NestedMessage>>));
  tsdb2::proto::test::Nesting m;
  EXPECT_FALSE(m.field2.has_value());
  EXPECT_FALSE(m.field3.has_value());
  EXPECT_FALSE(m.field4.has_value());
  m.field2 = -12;
  m.field3 = tsdb2::proto::test::Nesting::SatorEnum::SATOR_TENET;
  m.field4.emplace(tsdb2::proto::test::Nesting::NestedMessage{.field1 = 34});
  auto const encoded = tsdb2::proto::test::Nesting::Encode(m).Flatten();
  EXPECT_THAT(tsdb2::proto::test::Nesting::Decode(encoded.span()),
              IsOkAndHolds(tsdb2::proto::test::Nesting{
                  .field2 = -12,
                  .field3 = tsdb2::proto::test::Nesting::SatorEnum::SATOR_TENET,
                  .field4{tsdb2::proto::test::Nesting::NestedMessage{
                      .field1 = 34,
                  }},
              }));
}

TEST(GeneratorTest, Versions) {
  tsdb2::proto::test::Version1 const m1{
      .field1 = 123,
      .field2{tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_RED}},
      .field3{12, 34, 56},
      .field4{
          tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_GREEN},
          tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_BLUE},
      },
  };
  auto const data = tsdb2::proto::test::Version1::Encode(m1).Flatten();
  EXPECT_THAT(tsdb2::proto::test::Version2::Decode(data.span()),
              IsOkAndHolds(tsdb2::proto::test::Version2{
                  .field2{tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_RED}},
                  .field4{
                      tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_GREEN},
                      tsdb2::proto::test::OptionalEnumField{.color = ColorEnum::COLOR_BLUE},
                  },
              }));
}

TEST(GeneratorTest, MessageExtension) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ExtensibleMessage>().field1),
                      std::optional<int64_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ExtensibleMessage>().field2),
                      std::optional<std::string>>));
  EXPECT_TRUE((
      std::is_same_v<decltype(std::declval<tsdb2::proto::test::ExtensibleMessage>().extension_data),
                     tsdb2::proto::ExtensionData>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::tsdb2_proto_test_ExtensibleMessage_extension>()
                       .field3),
          std::optional<bool>>));
  EXPECT_TRUE(
      (std::is_same_v<
          decltype(std::declval<tsdb2::proto::test::tsdb2_proto_test_ExtensibleMessage_extension>()
                       .field4),
          std::optional<double>>));
  tsdb2::proto::test::ExtensibleMessage m;
  EXPECT_TRUE(m.extension_data.empty());
}

}  // namespace
