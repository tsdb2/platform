#include "server/init_tsdb2.h"

#include <string_view>
#include <thread>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/synchronization/notification.h"
#include "common/scoped_override.h"
#include "common/singleton.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "server/base_module.h"
#include "server/module_manager.h"

namespace tsdb2 {
namespace init {

namespace {

using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::tsdb2::init::BaseModule;
using ::tsdb2::init::Dependency;
using ::tsdb2::init::ModuleManager;
using ::tsdb2::init::RegisterModule;
using ::tsdb2::init::ReverseDependency;

struct InlineRegistration {};
inline InlineRegistration constexpr kInlineRegistration;

}  // namespace

class MockModule : public BaseModule {
 public:
  explicit MockModule(std::string_view const name) : BaseModule(name) {}

  template <typename... Args>
  explicit MockModule(InlineRegistration /*inline*/, std::string_view const name, Args&&... args)
      : BaseModule(name) {
    RegisterModule(this, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Register(Args&&... args) {
    RegisterModule(this, std::forward<Args>(args)...);
  }

  MOCK_METHOD(absl::Status, Initialize, (), (override));
  MOCK_METHOD(absl::Status, InitializeForTesting, (), (override));
};

class InitTest : public ::testing::Test {
 protected:
  ModuleManager manager_;
  tsdb2::common::ScopedOverride<tsdb2::common::Singleton<ModuleManager>> override_manager{
      ModuleManager::GetSingleton(), &manager_};
};

TEST_F(InitTest, ModuleName) {
  MockModule m1{"foo"};
  EXPECT_EQ(m1.name(), "foo");
  MockModule m2{"bar"};
  EXPECT_EQ(m2.name(), "bar");
}

TEST_F(InitTest, Initialize) {
  MockModule m{kInlineRegistration, "test"};
  EXPECT_CALL(m, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, InitializeForTesting) {
  MockModule m{kInlineRegistration, "test"};
  EXPECT_CALL(m, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m, Initialize).Times(0);
  EXPECT_OK(manager_.InitializeModulesForTesting());
}

TEST_F(InitTest, FailInitialization) {
  MockModule m{kInlineRegistration, "test"};
  EXPECT_CALL(m, Initialize).Times(1).WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(m, InitializeForTesting).Times(0);
  EXPECT_THAT(manager_.InitializeModules(), StatusIs(absl::StatusCode::kInternal));
}

TEST_F(InitTest, FailTestingInitialization) {
  MockModule m{kInlineRegistration, "test"};
  EXPECT_CALL(m, InitializeForTesting).Times(1).WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(m, Initialize).Times(0);
  EXPECT_THAT(manager_.InitializeModulesForTesting(), StatusIs(absl::StatusCode::kInternal));
}

TEST_F(InitTest, DoubleRegistration) {
  MockModule m{kInlineRegistration, "test"};
  EXPECT_DEATH(m.Register(), _);
}

TEST_F(InitTest, SelfDependency) {
  MockModule m{"test"};
  m.Register(&m);
  ON_CALL(m, Initialize).WillByDefault(Return(absl::OkStatus()));
  EXPECT_CALL(m, InitializeForTesting).Times(0);
  EXPECT_THAT(manager_.InitializeModules(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(InitTest, SelfDependencyInTesting) {
  MockModule m{"test"};
  m.Register(&m);
  ON_CALL(m, InitializeForTesting).WillByDefault(Return(absl::OkStatus()));
  EXPECT_CALL(m, Initialize).Times(0);
  EXPECT_THAT(manager_.InitializeModulesForTesting(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(InitTest, SimpleDependency) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  m1.Register();
  m2.Register(&m1);
  InSequence s;
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, SimpleDependencyInTesting) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  m1.Register();
  m2.Register(&m1);
  InSequence s;
  EXPECT_CALL(m1, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, Initialize).Times(0);
  EXPECT_CALL(m2, Initialize).Times(0);
  EXPECT_OK(manager_.InitializeModulesForTesting());
}

TEST_F(InitTest, ReverseDependency) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  m1.Register();
  m2.Register(ReverseDependency(&m1));
  InSequence s;
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, ReverseDependencyInTesting) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  m1.Register();
  m2.Register(ReverseDependency(&m1));
  InSequence s;
  EXPECT_CALL(m2, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(0);
  EXPECT_CALL(m1, Initialize).Times(0);
  EXPECT_OK(manager_.InitializeModulesForTesting());
}

TEST_F(InitTest, DirectAndReverseDependencies) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  m1.Register();
  m2.Register();
  m3.Register(Dependency(&m1), ReverseDependency(&m2));
  InSequence s;
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, DirectAndReverseDependenciesInTesting) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  m1.Register();
  m2.Register();
  m3.Register(Dependency(&m1), ReverseDependency(&m2));
  InSequence s;
  EXPECT_CALL(m1, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, InitializeForTesting).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, Initialize).Times(0);
  EXPECT_CALL(m3, Initialize).Times(0);
  EXPECT_CALL(m2, Initialize).Times(0);
  EXPECT_OK(manager_.InitializeModulesForTesting());
}

TEST_F(InitTest, MutualDependency) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  m1.Register(&m2);
  m2.Register(&m1);
  ON_CALL(m1, Initialize).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(m2, Initialize).WillByDefault(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_THAT(manager_.InitializeModules(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(InitTest, ForwardTriangle) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  m1.Register();
  m2.Register(&m1);
  m3.Register(&m1, &m2);
  InSequence s;
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, TwoRoots) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  MockModule m4{"test4"};
  MockModule m5{"test5"};
  m1.Register();
  m2.Register(&m1);
  m3.Register();
  m4.Register(&m3);
  m5.Register(&m3, &m4);
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m4, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m5, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_CALL(m4, InitializeForTesting).Times(0);
  EXPECT_CALL(m5, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, ManyDependencies) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  MockModule m4{"test4"};
  MockModule m5{"test5"};
  m1.Register();
  m2.Register();
  m3.Register(&m1, &m2);
  m4.Register(&m1, &m3);
  m5.Register(&m2, &m3);
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m4, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m5, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_CALL(m4, InitializeForTesting).Times(0);
  EXPECT_CALL(m5, InitializeForTesting).Times(0);
  EXPECT_OK(manager_.InitializeModules());
}

TEST_F(InitTest, StopsAtFailingDependency) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  MockModule m4{"test4"};
  MockModule m5{"test5"};
  m1.Register();
  m2.Register();
  m3.Register(&m1, &m2);
  m4.Register(&m1, &m3);
  m5.Register(&m2, &m3);
  EXPECT_CALL(m1, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m2, Initialize).Times(1).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(1).WillOnce(Return(absl::CancelledError("")));
  EXPECT_CALL(m4, Initialize).Times(0);
  EXPECT_CALL(m5, Initialize).Times(0);
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_CALL(m4, InitializeForTesting).Times(0);
  EXPECT_CALL(m5, InitializeForTesting).Times(0);
  EXPECT_THAT(manager_.InitializeModules(), StatusIs(absl::StatusCode::kCancelled));
}

TEST_F(InitTest, TwoRootsOneCycle) {
  MockModule m1{"test1"};
  MockModule m2{"test2"};
  MockModule m3{"test3"};
  MockModule m4{"test4"};
  MockModule m5{"test5"};
  m1.Register();
  m2.Register(&m1);
  m3.Register(&m4);
  m4.Register(&m5);
  m5.Register(&m3);
  ON_CALL(m1, Initialize).WillByDefault(Return(absl::OkStatus()));
  ON_CALL(m2, Initialize).WillByDefault(Return(absl::OkStatus()));
  EXPECT_CALL(m3, Initialize).Times(0);
  EXPECT_CALL(m4, Initialize).Times(0);
  EXPECT_CALL(m5, Initialize).Times(0);
  EXPECT_CALL(m1, InitializeForTesting).Times(0);
  EXPECT_CALL(m2, InitializeForTesting).Times(0);
  EXPECT_CALL(m3, InitializeForTesting).Times(0);
  EXPECT_CALL(m4, InitializeForTesting).Times(0);
  EXPECT_CALL(m5, InitializeForTesting).Times(0);
  EXPECT_THAT(manager_.InitializeModules(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(InitTest, WaitAndDone) {
  absl::Notification done;
  std::thread t{[&] {
    tsdb2::init::Wait();
    done.Notify();
  }};
  EXPECT_FALSE(done.HasBeenNotified());
  EXPECT_FALSE(tsdb2::init::IsDone());
  tsdb2::init::InitForTesting();
  done.WaitForNotification();
  EXPECT_TRUE(tsdb2::init::IsDone());
  t.join();
}

}  // namespace init
}  // namespace tsdb2
