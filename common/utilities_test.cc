#include "common/utilities.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/testing.h"
#include "gtest/gtest.h"

namespace {

using ::testing::status::IsOk;
using ::testing::status::StatusIs;

absl::Status Foo(bool const fail) {
  if (fail) {
    return absl::AbortedError("failed");
  } else {
    return absl::OkStatus();
  }
}

absl::Status Bar(bool const fail) {
  RETURN_IF_ERROR(Foo(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfError) {
  EXPECT_THAT(Bar(false), IsOk());
  EXPECT_THAT(Bar(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> FooOr(bool const fail) {
  if (fail) {
    return absl::AbortedError("failed");
  } else {
    return 42;
  }
}

absl::Status BarOr(bool const fail) {
  RETURN_IF_ERROR(FooOr(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfErrorOr) {
  EXPECT_THAT(BarOr(false), IsOk());
  EXPECT_THAT(BarOr(true), StatusIs(absl::StatusCode::kAborted));
}

// TODO

}  // namespace
