#include "proto/dependencies.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::proto::internal::DependencyManager;

class DependencyManagerTest : public ::testing::Test {
 protected:
  DependencyManager dependencies_;
};

TEST_F(DependencyManagerTest, AddDependency) {
  // TODO
}

// TODO

}  // namespace
