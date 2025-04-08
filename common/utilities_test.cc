#include "common/utilities.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::tsdb2::util::IsIntegralStrictV;

absl::Status Foo(bool const fail) {
  if (fail) {
    return absl::AbortedError("failed");
  } else {
    return absl::OkStatus();
  }
}

absl::StatusOr<int> FooOr(bool const fail) {
  if (fail) {
    return absl::AbortedError("failed");
  } else {
    return 42;
  }
}

absl::Status ReturnIfError(bool const fail) {
  RETURN_IF_ERROR(Foo(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfError) {
  EXPECT_OK(ReturnIfError(false));
  EXPECT_THAT(ReturnIfError(true), StatusIs(absl::StatusCode::kAborted));
}

absl::Status ReturnIfErrorOr(bool const fail) {
  RETURN_IF_ERROR(FooOr(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfErrorOr) {
  EXPECT_OK(ReturnIfErrorOr(false));
  EXPECT_THAT(ReturnIfErrorOr(true), StatusIs(absl::StatusCode::kAborted));
}

absl::Status ReplaceError(bool const fail) {
  REPLACE_ERROR(Foo(fail), absl::FailedPreconditionError("test"));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReplaceError) {
  EXPECT_OK(ReplaceError(false));
  EXPECT_THAT(ReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::Status ReplaceErrorOr(bool const fail) {
  REPLACE_ERROR(FooOr(fail), absl::FailedPreconditionError("test"));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReplaceErrorOr) {
  EXPECT_OK(ReplaceErrorOr(false));
  EXPECT_THAT(ReplaceErrorOr(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::StatusOr<int> AssignOrReturn(bool const fail) {
  int n;
  ASSIGN_OR_RETURN(n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, AssignOrReturn) {
  EXPECT_THAT(AssignOrReturn(false), IsOkAndHolds(42));
  EXPECT_THAT(AssignOrReturn(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> AssignOrReplaceError(bool const fail) {
  int n;
  ASSIGN_OR_REPLACE_ERROR(n, FooOr(fail), absl::FailedPreconditionError("test"));
  return n;
}

TEST(UtilitiesTest, AssignOrReplaceError) {
  EXPECT_THAT(AssignOrReplaceError(false), IsOkAndHolds(42));
  EXPECT_THAT(AssignOrReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::StatusOr<int> AssignVarOrReturn(bool const fail) {
  ASSIGN_VAR_OR_RETURN(int, n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, AssignVarOrReturn) {
  EXPECT_THAT(AssignVarOrReturn(false), IsOkAndHolds(42));
  EXPECT_THAT(AssignVarOrReturn(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> AssignVarOrReplaceError(bool const fail) {
  ASSIGN_VAR_OR_REPLACE_ERROR(int, n, FooOr(fail), absl::FailedPreconditionError("test"));
  return n;
}

TEST(UtilitiesTest, AssignVarOrReplaceError) {
  EXPECT_THAT(AssignVarOrReplaceError(false), IsOkAndHolds(42));
  EXPECT_THAT(AssignVarOrReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::StatusOr<int> DefineVarOrReturn(bool const fail) {
  DEFINE_VAR_OR_RETURN(n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, DefineVarOrReturn) {
  EXPECT_THAT(DefineVarOrReturn(false), IsOkAndHolds(42));
  EXPECT_THAT(DefineVarOrReturn(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> DefineVarOrReplaceError(bool const fail) {
  DEFINE_VAR_OR_REPLACE_ERROR(n, FooOr(fail), absl::FailedPreconditionError("test"));
  return n;
}

TEST(UtilitiesTest, DefineVarOrReplaceError) {
  EXPECT_THAT(DefineVarOrReplaceError(false), IsOkAndHolds(42));
  EXPECT_THAT(DefineVarOrReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::StatusOr<int> DefineConstOrReturn(bool const fail) {
  DEFINE_CONST_OR_RETURN(n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, DefineConstOrReturn) {
  EXPECT_THAT(DefineConstOrReturn(false), IsOkAndHolds(42));
  EXPECT_THAT(DefineConstOrReturn(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> DefineConstOrReplaceError(bool const fail) {
  DEFINE_CONST_OR_REPLACE_ERROR(n, FooOr(fail), absl::FailedPreconditionError("test"));
  return n;
}

TEST(UtilitiesTest, DefineConstOrReplaceError) {
  EXPECT_THAT(DefineConstOrReplaceError(false), IsOkAndHolds(42));
  EXPECT_THAT(DefineConstOrReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

absl::Status DefineOrReturn(bool const fail) {
  DEFINE_OR_RETURN(n, FooOr(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, DefineOrReturn) {
  EXPECT_OK(DefineOrReturn(false));
  EXPECT_THAT(DefineOrReturn(true), StatusIs(absl::StatusCode::kAborted));
}

absl::Status DefineOrReplaceError(bool const fail) {
  DEFINE_OR_REPLACE_ERROR(n, FooOr(fail), absl::FailedPreconditionError("test"));
  return absl::OkStatus();
}

TEST(UtilitiesTest, DefineOrReplaceError) {
  EXPECT_OK(DefineOrReplaceError(false));
  EXPECT_THAT(DefineOrReplaceError(true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(UtilitiesTest, IsIntegralStrict) {
  enum class E { k1, k2, k3 };
  EXPECT_FALSE(IsIntegralStrictV<E>);
  EXPECT_FALSE(IsIntegralStrictV<bool>);
  EXPECT_TRUE(IsIntegralStrictV<int8_t>);
  EXPECT_TRUE(IsIntegralStrictV<int16_t>);
  EXPECT_TRUE(IsIntegralStrictV<int32_t>);
  EXPECT_TRUE(IsIntegralStrictV<int64_t>);
  EXPECT_TRUE(IsIntegralStrictV<uint8_t>);
  EXPECT_TRUE(IsIntegralStrictV<uint16_t>);
  EXPECT_TRUE(IsIntegralStrictV<uint32_t>);
  EXPECT_TRUE(IsIntegralStrictV<uint64_t>);
}

}  // namespace
