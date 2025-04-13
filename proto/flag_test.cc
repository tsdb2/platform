#include "proto/flag.h"

#include "absl/flags/flag.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "proto/tests/field_test.pb.h"

ABSL_FLAG(tsdb2::proto::test::OptionalStringField, test_flag, {.field = "lorem"}, "test");

namespace {

using ::testing::Field;
using ::testing::Optional;
using ::tsdb2::proto::test::OptionalStringField;

TEST(ProtoFlagTest, DefaultValue) {
  EXPECT_THAT(absl::GetFlag(FLAGS_test_flag),
              Field(&OptionalStringField::field, Optional("lorem")));
}

TEST(ProtoFlagTest, OverriddenValue) {
  absl::SetFlag(&FLAGS_test_flag, {.field = "ipsum"});
  EXPECT_THAT(absl::GetFlag(FLAGS_test_flag),
              Field(&OptionalStringField::field, Optional("ipsum")));
}

}  // namespace
