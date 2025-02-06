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

TEST_F(DependencyManagerTest, OneNode) {
  dependencies_.AddNode({"lorem"});
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem"));
  EXPECT_THAT(dependencies_.MakeOrder({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"lorem", "ipsum"}), IsEmpty());
}

TEST_F(DependencyManagerTest, NestedNode) {
  dependencies_.AddNode({"lorem", "ipsum"});
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem"));
  EXPECT_THAT(dependencies_.MakeOrder({"lorem"}), ElementsAre("ipsum"));
  EXPECT_THAT(dependencies_.MakeOrder({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"lorem", "ipsum"}), IsEmpty());
}

TEST_F(DependencyManagerTest, NestedNodes) {
  dependencies_.AddNode({"lorem", "ipsum"});
  dependencies_.AddNode({"lorem", "dolor"});
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"dolor"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem", "dolor"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem"));
  EXPECT_THAT(dependencies_.MakeOrder({"lorem"}), ElementsAre("dolor", "ipsum"));
  EXPECT_THAT(dependencies_.MakeOrder({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"dolor"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"lorem", "dolor"}), IsEmpty());
}

TEST_F(DependencyManagerTest, TwoNodes) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"ipsum", "lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("ipsum", "lorem"));
  EXPECT_THAT(dependencies_.MakeOrder({"lorem"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"lorem", "ipsum"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({"ipsum", "lorem"}), IsEmpty());
}

TEST_F(DependencyManagerTest, OneDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}),
              AnyOf(ElementsAre("lorem", "ipsum"), ElementsAre("ipsum", "lorem")));
}

// TODO

}  // namespace
