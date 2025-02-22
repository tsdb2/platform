#include "proto/generator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/status/status_matchers.h"
#include "common/fingerprint.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests.pb.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::tsdb2::proto::generator::MakeHeaderFileName;
using ::tsdb2::proto::generator::MakeSourceFileName;

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

TEST(GeneratorTest, TestEmpty) {
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

TEST(GeneratorTest, TestOptionalField) {
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

TEST(GeneratorTest, TestDefaultedField) {
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

TEST(GeneratorTest, TestRepeatedField) {
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

TEST(GeneratorTest, TestRequiredField) {
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
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().fixed32_field),
                      std::optional<uint32_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sfixed32_field),
                      std::optional<int32_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().fixed64_field),
                      std::optional<uint64_t>>));
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::ManyFields>().sfixed64_field),
                      std::optional<int64_t>>));
  tsdb2::proto::test::ManyFields m;
  EXPECT_FALSE(m.int32_field.has_value());
  EXPECT_FALSE(m.uint32_field.has_value());
  EXPECT_FALSE(m.int64_field.has_value());
  EXPECT_FALSE(m.uint64_field.has_value());
  EXPECT_FALSE(m.sint32_field.has_value());
  EXPECT_FALSE(m.sint64_field.has_value());
  EXPECT_FALSE(m.fixed32_field.has_value());
  EXPECT_FALSE(m.sfixed32_field.has_value());
  EXPECT_FALSE(m.fixed64_field.has_value());
  EXPECT_FALSE(m.sfixed64_field.has_value());
  m.int32_field = -12;
  m.uint32_field = 34;
  m.int64_field = -56;
  m.uint64_field = 78;
  m.sint32_field = -12;
  m.sint64_field = -34;
  m.fixed32_field = 12;
  m.sfixed32_field = -34;
  m.fixed64_field = 56;
  m.sfixed64_field = -78;
  auto const encoded = tsdb2::proto::test::ManyFields::Encode(m).Flatten();
  EXPECT_THAT(tsdb2::proto::test::ManyFields::Decode(encoded.span()), IsOkAndHolds(m));
}

TEST(GeneratorTest, TestOptionalStringField) {
  EXPECT_TRUE(
      (std::is_same_v<decltype(std::declval<tsdb2::proto::test::OptionalStringField>().field),
                      std::optional<std::string>>));
  tsdb2::proto::test::OptionalStringField m1;
  tsdb2::proto::test::OptionalStringField m2{.field{"lorem"}};
  EXPECT_FALSE(m1.field.has_value());
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

TEST(GeneratorTest, TestDefaultedStringField) {
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

TEST(GeneratorTest, TestRepeatedStringField) {
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

TEST(GeneratorTest, TestRequiredStringField) {
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

// TODO

}  // namespace
