#include "common/utilities.h"

#include <cstdint>
#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOk;
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

absl::Status Bar(bool const fail) {
  RETURN_IF_ERROR(Foo(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfError) {
  EXPECT_THAT(Bar(false), IsOk());
  EXPECT_THAT(Bar(true), StatusIs(absl::StatusCode::kAborted));
}

absl::Status BarOr(bool const fail) {
  RETURN_IF_ERROR(FooOr(fail));
  return absl::OkStatus();
}

TEST(UtilitiesTest, ReturnIfErrorOr) {
  EXPECT_THAT(BarOr(false), IsOk());
  EXPECT_THAT(BarOr(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> Baz(bool const fail) {
  int n;
  ASSIGN_OR_RETURN(n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, AssignOrReturn) {
  EXPECT_THAT(Baz(false), IsOkAndHolds(42));
  EXPECT_THAT(Baz(true), StatusIs(absl::StatusCode::kAborted));
}

absl::StatusOr<int> BazVar(bool const fail) {
  ASSIGN_VAR_OR_RETURN(int, n, FooOr(fail));
  return n;
}

TEST(UtilitiesTest, AssignVarOrReturn) {
  EXPECT_THAT(BazVar(false), IsOkAndHolds(42));
  EXPECT_THAT(BazVar(true), StatusIs(absl::StatusCode::kAborted));
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
