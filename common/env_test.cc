#include "common/env.h"

#include <stdlib.h>

#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::StatusIs;
using ::testing::IsSupersetOf;
using ::testing::Optional;
using ::testing::Pair;
using ::tsdb2::common::Environ;
using ::tsdb2::common::GetEnv;
using ::tsdb2::common::SetEnv;
using ::tsdb2::common::SetEnvIfUnset;
using ::tsdb2::common::UnsetEnv;

TEST(EnvTest, GetEnv) {
  ASSERT_EQ(::setenv("LOREM", "IPSUM", 1), 0);
  EXPECT_THAT(GetEnv("LOREM"), Optional<std::string>("IPSUM"));
}

TEST(EnvTest, GetAnotherEnv) {
  ASSERT_EQ(::setenv("DOLOR", "AMET", 1), 0);
  EXPECT_EQ(GetEnv("DOLOR"), "AMET");
}

TEST(EnvTest, GetMissingEnv) {
  ASSERT_EQ(::unsetenv("LOREM"), 0);
  EXPECT_EQ(GetEnv("LOREM"), std::nullopt);
}

TEST(EnvTest, SetEnv) {
  ASSERT_EQ(::unsetenv("LOREM"), 0);
  EXPECT_OK(SetEnv("LOREM", "IPSUM"));
  EXPECT_THAT(GetEnv("LOREM"), Optional<std::string>("IPSUM"));
}

TEST(EnvTest, SetExistingEnv) {
  ASSERT_EQ(::setenv("LOREM", "IPSUM", 1), 0);
  EXPECT_OK(SetEnv("LOREM", "DOLOR"));
  EXPECT_THAT(GetEnv("LOREM"), Optional<std::string>("DOLOR"));
}

TEST(EnvTest, SetWithInvalidName) {
  EXPECT_THAT(SetEnv("LOREM=IPSUM", "DOLOR"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EnvTest, SetEnvIfUnset1) {
  ASSERT_EQ(::setenv("LOREM", "IPSUM", 1), 0);
  EXPECT_OK(SetEnvIfUnset("LOREM", "DOLOR"));
  EXPECT_THAT(GetEnv("LOREM"), Optional<std::string>("IPSUM"));
}

TEST(EnvTest, SetEnvIfUnset2) {
  ASSERT_EQ(::unsetenv("LOREM"), 0);
  EXPECT_OK(SetEnvIfUnset("LOREM", "DOLOR"));
  EXPECT_THAT(GetEnv("LOREM"), Optional<std::string>("DOLOR"));
}

TEST(EnvTest, SetEnvWithInvalidNameIfUnset) {
  EXPECT_THAT(SetEnvIfUnset("LOREM=IPSUM", "DOLOR"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(EnvTest, Environ) {
  ASSERT_OK(SetEnv("LOREM", "ipsum"));
  ASSERT_OK(SetEnv("DOLOR", ""));
  ASSERT_OK(SetEnv("ELIT", "adipisci"));
  EXPECT_THAT(Environ(), IsSupersetOf({
                             Pair("LOREM", "ipsum"),
                             Pair("DOLOR", ""),
                             Pair("ELIT", "adipisci"),
                         }));
}

}  // namespace
