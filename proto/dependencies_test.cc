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
          ElementsAre(Pair(ElementsAre("ipsum"), "bar"), Pair(ElementsAre("lorem"), "foo")),
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
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("lorem"), "bar"), Pair(ElementsAre("ipsum"), "foo")),
          ElementsAre(Pair(ElementsAre("dolor"), "baz"), Pair(ElementsAre("lorem"), "qux"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode4) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "foo");
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "bar");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "baz");
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "qux");
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("dolor"), "bar"), Pair(ElementsAre("ipsum"), "foo")),
          ElementsAre(Pair(ElementsAre("dolor"), "qux"), Pair(ElementsAre("lorem"), "baz"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode5) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"dolor"}, {"lorem"}, "foo");
  dependencies_.AddDependency({"lorem"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "baz");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "qux");
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("dolor"), "foo"), Pair(ElementsAre("lorem"), "bar")),
          ElementsAre(Pair(ElementsAre("lorem"), "qux"), Pair(ElementsAre("ipsum"), "baz"))));
}

TEST_F(DependencyManagerTest, CyclesWithSharedNode6) {
  dependencies_.AddNode({"lorem"});
  dependencies_.AddNode({"ipsum"});
  dependencies_.AddNode({"dolor"});
  dependencies_.AddDependency({"dolor"}, {"ipsum"}, "foo");
  dependencies_.AddDependency({"ipsum"}, {"dolor"}, "bar");
  dependencies_.AddDependency({"lorem"}, {"ipsum"}, "baz");
  dependencies_.AddDependency({"ipsum"}, {"lorem"}, "qux");
  EXPECT_THAT(
      dependencies_.FindCycles({}),
      UnorderedElementsAre(
          ElementsAre(Pair(ElementsAre("dolor"), "foo"), Pair(ElementsAre("ipsum"), "bar")),
          ElementsAre(Pair(ElementsAre("ipsum"), "qux"), Pair(ElementsAre("lorem"), "baz"))));
}

TEST_F(DependencyManagerTest, CycleFurtherOn) {
  dependencies_.AddNode({"sator"});
  dependencies_.AddNode({"arepo"});
  dependencies_.AddNode({"tenet"});
  dependencies_.AddDependency({"arepo"}, {"sator"}, "foo");
  dependencies_.AddDependency({"sator"}, {"tenet"}, "bar");
  dependencies_.AddDependency({"tenet"}, {"sator"}, "baz");
  EXPECT_THAT(dependencies_.FindCycles({}),
              ElementsAre(ElementsAre(Pair(ElementsAre("sator"), "bar"),
                                      Pair(ElementsAre("tenet"), "baz"))));
}

TEST_F(DependencyManagerTest, NestedDependency) {
  dependencies_.AddNode({"sator", "arepo"});
  dependencies_.AddNode({"sator", "tenet"});
  dependencies_.AddDependency({"sator", "arepo"}, {"sator", "tenet"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("sator"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator"}), ElementsAre("tenet", "arepo"));
}

TEST_F(DependencyManagerTest, NestedCycle) {
  dependencies_.AddNode({"sator", "arepo"});
  dependencies_.AddNode({"sator", "tenet"});
  dependencies_.AddDependency({"sator", "arepo"}, {"sator", "tenet"}, "foo");
  dependencies_.AddDependency({"sator", "tenet"}, {"sator", "arepo"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}),
              ElementsAre(ElementsAre(Pair(ElementsAre("sator", "arepo"), "foo"),
                                      Pair(ElementsAre("sator", "tenet"), "bar"))));
}

TEST_F(DependencyManagerTest, CrossScopeDependency) {
  dependencies_.AddNode({"sator", "arepo"});
  dependencies_.AddNode({"sator", "tenet"});
  dependencies_.AddNode({"opera", "arepo"});
  dependencies_.AddNode({"opera", "tenet"});
  dependencies_.AddDependency({"opera", "arepo"}, {"sator", "tenet"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"opera"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("sator", "opera"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator"}), ElementsAre("arepo", "tenet"));
  EXPECT_THAT(dependencies_.MakeOrder({"opera"}), ElementsAre("arepo", "tenet"));
}

TEST_F(DependencyManagerTest, OppositeCrossScopeDependency) {
  dependencies_.AddNode({"sator", "arepo"});
  dependencies_.AddNode({"sator", "tenet"});
  dependencies_.AddNode({"opera", "arepo"});
  dependencies_.AddNode({"opera", "tenet"});
  dependencies_.AddDependency({"sator", "arepo"}, {"opera", "tenet"}, "foo");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"opera"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("opera", "sator"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator"}), ElementsAre("arepo", "tenet"));
  EXPECT_THAT(dependencies_.MakeOrder({"opera"}), ElementsAre("arepo", "tenet"));
}

TEST_F(DependencyManagerTest, CrossScopeCycle) {
  dependencies_.AddNode({"sator", "arepo"});
  dependencies_.AddNode({"sator", "tenet"});
  dependencies_.AddNode({"opera", "arepo"});
  dependencies_.AddNode({"opera", "tenet"});
  dependencies_.AddDependency({"opera", "arepo"}, {"sator", "tenet"}, "foo");
  dependencies_.AddDependency({"sator", "tenet"}, {"opera", "arepo"}, "bar");
  EXPECT_THAT(dependencies_.FindCycles({}),
              ElementsAre(ElementsAre(Pair(ElementsAre("opera"), "arepo.foo"),
                                      Pair(ElementsAre("sator"), "tenet.bar"))));
  EXPECT_THAT(dependencies_.FindCycles({"sator"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"opera"}), IsEmpty());
}

TEST_F(DependencyManagerTest, CrossScopeDependencyWithCommonAncestor) {
  dependencies_.AddNode({"sator", "arepo", "tenet"});
  dependencies_.AddNode({"sator", "tenet", "opera"});
  dependencies_.AddDependency({"sator", "arepo", "tenet"}, {"sator", "tenet", "opera"}, "rotas");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "arepo"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "arepo", "tenet"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "tenet"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "tenet", "opera"}), IsEmpty());
  EXPECT_THAT(dependencies_.MakeOrder({}), ElementsAre("sator"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator"}), ElementsAre("tenet", "arepo"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator", "arepo"}), ElementsAre("tenet"));
  EXPECT_THAT(dependencies_.MakeOrder({"sator", "tenet"}), ElementsAre("opera"));
}

TEST_F(DependencyManagerTest, CrossScopeCycleWithCommonAncestor) {
  dependencies_.AddNode({"sator", "arepo", "tenet"});
  dependencies_.AddNode({"sator", "tenet", "opera"});
  dependencies_.AddDependency({"sator", "arepo", "tenet"}, {"sator", "tenet", "opera"}, "rotas");
  dependencies_.AddDependency({"sator", "tenet", "opera"}, {"sator", "arepo", "tenet"}, "rotas");
  EXPECT_THAT(dependencies_.FindCycles({}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator"}),
              ElementsAre(ElementsAre(Pair(ElementsAre("sator", "arepo"), "tenet.rotas"),
                                      Pair(ElementsAre("sator", "tenet"), "opera.rotas"))));
  EXPECT_THAT(dependencies_.FindCycles({"sator", "arepo"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "arepo", "tenet"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "tenet"}), IsEmpty());
  EXPECT_THAT(dependencies_.FindCycles({"sator", "tenet", "opera"}), IsEmpty());
}

}  // namespace
