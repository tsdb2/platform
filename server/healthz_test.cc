#include "server/healthz.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/scoped_override.h"
#include "common/singleton.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace tsdb2 {
namespace server {

using ::absl_testing::StatusIs;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::Singleton;
using ::tsdb2::server::Healthz;

class HealthzTest : public ::testing::Test {
 protected:
  Healthz healthz_;
  ScopedOverride<Singleton<Healthz>> healthz_override_{&Healthz::instance, &healthz_};
};

TEST_F(HealthzTest, NoChecks) { EXPECT_OK(Healthz::instance->RunChecks()); }

TEST_F(HealthzTest, OneSucceedingCheck) {
  Healthz::instance->AddCheck([] { return absl::OkStatus(); });
  EXPECT_OK(Healthz::instance->RunChecks());
}

TEST_F(HealthzTest, OneFailingCheck) {
  Healthz::instance->AddCheck([] { return absl::FailedPreconditionError("test"); });
  EXPECT_THAT(Healthz::instance->RunChecks(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(HealthzTest, TwoSucceedingChecks) {
  Healthz::instance->AddCheck([] { return absl::OkStatus(); });
  Healthz::instance->AddCheck([] { return absl::OkStatus(); });
  EXPECT_OK(Healthz::instance->RunChecks());
}

TEST_F(HealthzTest, TwoChecksFirstFails) {
  Healthz::instance->AddCheck([] { return absl::FailedPreconditionError("test"); });
  Healthz::instance->AddCheck([] { return absl::OkStatus(); });
  EXPECT_THAT(Healthz::instance->RunChecks(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(HealthzTest, TwoChecksSecondFails) {
  Healthz::instance->AddCheck([] { return absl::OkStatus(); });
  Healthz::instance->AddCheck([] { return absl::FailedPreconditionError("test"); });
  EXPECT_THAT(Healthz::instance->RunChecks(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(HealthzTest, TwoFailingChecks) {
  Healthz::instance->AddCheck([] { return absl::CancelledError("test"); });
  Healthz::instance->AddCheck([] { return absl::FailedPreconditionError("test"); });
  EXPECT_THAT(Healthz::instance->RunChecks(), StatusIs(absl::StatusCode::kCancelled));
}

// TODO: test the HTTP handler.

}  // namespace server
}  // namespace tsdb2
