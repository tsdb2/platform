#include "proto/flag.h"  // IWYU pragma: keep

#include <string>

#include "absl/flags/flag.h"
#include "common/flag_override.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests/field_test.pb.h"

ABSL_FLAG(tsdb2::proto::test::OptionalStringField, test_flag, {.field = "lorem"}, "test");

namespace {

using ::testing::Field;
using ::testing::Optional;
using ::tsdb2::common::testing::FlagOverride;
using ::tsdb2::proto::test::OptionalStringField;

TEST(ProtoFlagTest, DefaultValue) {
  EXPECT_THAT(absl::GetFlag(FLAGS_test_flag),
              Field(&OptionalStringField::field, Optional<std::string>("lorem")));
}

TEST(ProtoFlagTest, OverriddenValue) {
  FlagOverride fo{&FLAGS_test_flag, OptionalStringField{.field = "ipsum"}};
  EXPECT_THAT(absl::GetFlag(FLAGS_test_flag),
              Field(&OptionalStringField::field, Optional<std::string>("ipsum")));
}

}  // namespace
