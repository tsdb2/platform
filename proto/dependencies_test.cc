#include "proto/dependencies.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
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
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("ipsum", "lorem"));
}

TEST_F(DependencyManagerTest, OppositeDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem", "ipsum"));
}

TEST_F(DependencyManagerTest, DivergingDependencies) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem", "ipsum", "dolor"));
}

TEST_F(DependencyManagerTest, ConvergingDependencies) {
  dependencies_.AddNode({"sator"});
  dependencies_.AddNode({"arepo"});
  dependencies_.AddNode({"tenet"});
  dependencies_.AddDependency({"arepo"}, {"sator"}, "foo");
  dependencies_.AddDependency({"tenet"}, {"sator"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("sator", "arepo", "tenet"));
}

TEST_F(DependencyManagerTest, DoubleDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("ipsum", "lorem"));
}

TEST_F(DependencyManagerTest, TransitiveDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("dolor", "ipsum", "lorem"));
}

TEST_F(DependencyManagerTest, DiamondDependencies1) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddNode({"amet"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"amet"}, "baz");
  dependencies_.AddDependency({"dolor"}, {"amet"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("amet", "dolor", "ipsum", "lorem"));
}

TEST_F(DependencyManagerTest, DiamondDependencies2) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddNode({"amet"});
  dependencies_.AddDependency({"amet"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"amet"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "baz");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("lorem", "dolor", "ipsum", "amet"));
}

TEST_F(DependencyManagerTest, SelfDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddDependency({"lorem"}, {"lorem"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}),
              ElementsAre(ElementsAre(Pair(ElementsAre("lorem"), "foo"))));
}

TEST_F(DependencyManagerTest, CircularDependency) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}),
              ElementsAre(UnorderedElementsAre(Pair(ElementsAre("lorem"), "bar"),
                                               Pair(ElementsAre("ipsum"), "bar"))));
}

TEST_F(DependencyManagerTest, TwoCycles) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddNode({"amet"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "bar");
  dependencies_.AddDependency({"dolor"}, {"amet"}, "baz");
  dependencies_.AddDependency({"amet"}, {"dolor"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}),
              UnorderedElementsAre(UnorderedElementsAre(Pair(ElementsAre("lorem"), "foo"),
                                                        Pair(ElementsAre("ipsum"), "bar")),
                                   UnorderedElementsAre(Pair(ElementsAre("dolor"), "baz"),
                                                        Pair(ElementsAre("amet"), "qux"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode1) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "bar");
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "baz");
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "qux");
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("dolor"), "baz"), Pair(ElementsAre("ipsum"), "bar"),
                      Pair(ElementsAre("lorem"), "foo")),
          ElementsAre(Pair(ElementsAre("dolor"), "baz"), Pair(ElementsAre("ipsum"), "qux"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode2) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "foo");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "baz");
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "qux");
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("dolor"), "bar"), Pair(ElementsAre("lorem"), "foo")),
          ElementsAre(Pair(ElementsAre("dolor"), "qux"), Pair(ElementsAre("ipsum"), "baz"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode3) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "foo");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "bar");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "baz");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}),
              UnorderedElementsAre(UnorderedElementsAre(Pair(ElementsAre("dolor"), "baz"),
                                                        Pair(ElementsAre("lorem"), "bar"),
                                                        Pair(ElementsAre("ipsum"), "foo")),
                                   UnorderedElementsAre(Pair(ElementsAre("dolor"), "baz"),
                                                        Pair(ElementsAre("lorem"), "qux"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode4) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "foo");
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "bar");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "baz");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}),
              UnorderedElementsAre(UnorderedElementsAre(Pair(ElementsAre("dolor"), "bar"),
                                                        Pair(ElementsAre("ipsum"), "foo")),
                                   UnorderedElementsAre(Pair(ElementsAre("dolor"), "qux"),
                                                        Pair(ElementsAre("lorem"), "baz"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode5) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "foo");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "baz");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}),
              UnorderedElementsAre(UnorderedElementsAre(Pair(ElementsAre("dolor"), "foo"),
                                                        Pair(ElementsAre("lorem"), "bar")),
                                   UnorderedElementsAre(Pair(ElementsAre("dolor"), "foo"),
                                                        Pair(ElementsAre("lorem"), "qux"),
                                                        Pair(ElementsAre("ipsum"), "baz"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode6) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "baz");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "qux");
  EXPECT_THAT(dependencies_.FindCycles({}),
              UnorderedElementsAre(UnorderedElementsAre(Pair(ElementsAre("dolor"), "foo"),
                                                        Pair(ElementsAre("ipsum"), "bar")),
                                   UnorderedElementsAre(Pair(ElementsAre("dolor"), "foo"),
                                                        Pair(ElementsAre("ipsum"), "qux"),
                                                        Pair(ElementsAre("lorem"), "baz"))));
}

// TODO

}  // namespace
