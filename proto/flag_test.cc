#include "absl/flags/flag.h"

#include <string>

#include "common/flag_override.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests/field_test.pb.h"

ABSL_FLAG(tsdb2::proto::test::OptionalStringField, test_message_flag, {.field = "lorem"},
          "test message flag");

ABSL_FLAG(tsdb2::proto::test::ColorEnum, test_enum_flag, tsdb2::proto::test::ColorEnum::COLOR_GREEN,
          "test enum flag");

namespace {

using ::testing::Field;
using ::testing::Optional;
using ::tsdb2::common::testing::FlagOverride;
using ::tsdb2::proto::test::ColorEnum;
using ::tsdb2::proto::test::OptionalStringField;

TEST(ProtoFlagTest, DefaultMessage) {
  EXPECT_THAT(absl::GetFlag(FLAGS_test_message_flag),
              Field(&OptionalStringField::field, Optional<std::string>("lorem")));
}

TEST(ProtoFlagTest, OverriddenMessage) {
  FlagOverride fo{&FLAGS_test_message_flag, OptionalStringField{.field = "ipsum"}};
  EXPECT_THAT(absl::GetFlag(FLAGS_test_message_flag),
              Field(&OptionalStringField::field, Optional<std::string>("ipsum")));
}

TEST(ProtoFlagTest, DefaultEnum) {
  EXPECT_THAT(absl::GetFlag(FLAGS_test_enum_flag), ColorEnum::COLOR_GREEN);
}

TEST(ProtoFlagTest, OverriddenEnum) {
  FlagOverride fo{&FLAGS_test_enum_flag, ColorEnum::COLOR_CYAN};
  EXPECT_THAT(absl::GetFlag(FLAGS_test_enum_flag), ColorEnum::COLOR_CYAN);
}

}  // namespace
