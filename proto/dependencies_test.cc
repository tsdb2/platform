#include "proto/dependencies.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::tsdb2::proto::internal::DependencyManager;

class DependencyManagerTest : public ::testing::Test {
 protected:
  DependencyManager dependencies_;
};

TEST_F(DependencyManagerTest, Empty) {
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), IsEmpty());
}

TEST_F(DependencyManagerTest, OneDependency) {
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}),
              AnyOf(ElementsAre("lorem", "ipsum"), ElementsAre("ipsum", "lorem")));
}

// TODO

}  // namespace
