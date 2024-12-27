#include "server/module.h"

#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/scoped_override.h"
#include "common/singleton.h"
#include "common/testing.h"
#include "common/type_string.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "server/module_manager.h"

namespace tsdb2 {
namespace init {
namespace module_internal {

namespace {

using ::absl_testing::StatusIs;
using ::testing::InSequence;
using ::testing::Return;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::TypeStringMatcher;
using ::tsdb2::common::TypeStringT;
using ::tsdb2::init::Dependency;
using ::tsdb2::init::Module;
using ::tsdb2::init::ModuleManager;
using ::tsdb2::init::ReverseDependency;
using ::tsdb2::init::module_internal::ModuleImpl;

char constexpr kTestModule1[] = "test1";
char constexpr kTestModule2[] = "test2";
char constexpr kTestModule3[] = "test3";

template <typename Name>
class MockTraits;

template <char... ch>
class MockTraits<TypeStringMatcher<ch...>> {
 public:
  static char constexpr name[] = {ch..., 0};
  MOCK_METHOD(absl::Status, Initialize, ());
  MOCK_METHOD(absl::Status, InitializeForTesting, ());
};

using MockTraits1 = MockTraits<TypeStringT<kTestModule1>>;
using MockTraits2 = MockTraits<TypeStringT<kTestModule2>>;
using MockTraits3 = MockTraits<TypeStringT<kTestModule3>>;

}  // namespace

class ModuleTest : public ::testing::Test {
 protected:
  MockTraits1& traits1() { return mock_instance1_.traits_; }
  MockTraits2& traits2() { return mock_instance2_.traits_; }
  MockTraits3& traits3() { return mock_instance3_.traits_; }

  ModuleManager module_manager_;
  ScopedOverride<tsdb2::common::Singleton<ModuleManager>> const override_manager_{
      ModuleManager::GetSingleton(), &module_manager_};

  ModuleImpl<MockTraits1> mock_instance1_;
  ScopedOverride<tsdb2::common::Singleton<ModuleImpl<MockTraits1>>> const override_instance1_{
      ModuleImpl<MockTraits1>::GetSingleton(), &mock_instance1_};

  ModuleImpl<MockTraits2> mock_instance2_;
  ScopedOverride<tsdb2::common::Singleton<ModuleImpl<MockTraits2>>> const override_instance2_{
      ModuleImpl<MockTraits2>::GetSingleton(), &mock_instance2_};

  ModuleImpl<MockTraits3> mock_instance3_;
  ScopedOverride<tsdb2::common::Singleton<ModuleImpl<MockTraits3>>> const override_instance3_{
      ModuleImpl<MockTraits3>::GetSingleton(), &mock_instance3_};
};

TEST_F(ModuleTest, TriviallyDestructible) {
  EXPECT_TRUE(std::is_trivially_destructible_v<Module<MockTraits1>>);
  EXPECT_TRUE(std::is_trivially_destructible_v<Module<MockTraits2>>);
  EXPECT_TRUE(std::is_trivially_destructible_v<Module<MockTraits3>>);
}

TEST_F(ModuleTest, Initialize) {
  Module<MockTraits1> module;
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, Testing) {
  Module<MockTraits1> module;
  EXPECT_CALL(traits1(), Initialize).Times(0);
  EXPECT_CALL(traits1(), InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModulesForTesting());
}

TEST_F(ModuleTest, InitializationFails) {
  Module<MockTraits1> module;
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::AbortedError("test")));
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_THAT(module_manager_.InitializeModules(), StatusIs(absl::StatusCode::kAborted));
}

TEST_F(ModuleTest, TestingInitializationFails) {
  Module<MockTraits1> module;
  EXPECT_CALL(traits1(), Initialize).Times(0);
  EXPECT_CALL(traits1(), InitializeForTesting)
      .Times(1)
      .WillOnce(Return(absl::AbortedError("test")));
  EXPECT_THAT(module_manager_.InitializeModulesForTesting(), StatusIs(absl::StatusCode::kAborted));
}

TEST_F(ModuleTest, Dependency) {
  Module<MockTraits1> module1;
  Module<MockTraits2, MockTraits1> module2;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, OppositeDependency) {
  Module<MockTraits1, MockTraits2> module1;
  Module<MockTraits2> module2;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, CircularDependency) {
  Module<MockTraits1, MockTraits2> module1;
  Module<MockTraits2, MockTraits1> module2;
  EXPECT_CALL(traits1(), Initialize).Times(0);
  EXPECT_CALL(traits2(), Initialize).Times(0);
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  EXPECT_THAT(module_manager_.InitializeModules(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ModuleTest, SelfDependency) {
  Module<MockTraits1, MockTraits1> module1;
  EXPECT_CALL(traits1(), Initialize).Times(0);
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_THAT(module_manager_.InitializeModules(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ModuleTest, DependencyOrder) {
  Module<MockTraits1, MockTraits2> module1;
  Module<MockTraits2> module2;
  Module<MockTraits3, MockTraits1, MockTraits2> module3;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits3(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, DependencyTag) {
  Module<MockTraits1> module1;
  Module<MockTraits2, Dependency<MockTraits1>> module2;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, ReverseDependency) {
  Module<MockTraits1> module1;
  Module<MockTraits2, ReverseDependency<MockTraits1>> module2;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

TEST_F(ModuleTest, DirectAndReverseDependencies) {
  Module<MockTraits1, Dependency<MockTraits2>, ReverseDependency<MockTraits3>> module1;
  Module<MockTraits2> module2;
  Module<MockTraits3> module3;
  EXPECT_CALL(traits1(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits2(), InitializeForTesting).Times(0);
  EXPECT_CALL(traits3(), InitializeForTesting).Times(0);
  InSequence s;
  EXPECT_CALL(traits2(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits1(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(traits3(), Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_OK(module_manager_.InitializeModules());
}

}  // namespace module_internal
}  // namespace init
}  // namespace tsdb2
