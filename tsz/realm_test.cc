#include "tsz/realm.h"

#include <thread>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/synchronization/notification.h"
#include "common/reffed_ptr.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace tsz {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::Pointee2;
using ::testing::Property;
using ::tsdb2::common::reffed_ptr;
using ::tsz::Realm;

class RealmTest : public ::testing::Test {};

TEST_F(RealmTest, Constructor) {
  Realm const realm{"foo"};
  EXPECT_EQ(realm.name(), "foo");
}

TEST_F(RealmTest, ReferenceCount) {
  Realm const realm{"foo"};
  EXPECT_EQ(realm.ref_count(), 0);
  realm.Ref();
  EXPECT_EQ(realm.ref_count(), 1);
  realm.Ref();
  EXPECT_EQ(realm.ref_count(), 2);
  realm.Unref();
  EXPECT_EQ(realm.ref_count(), 1);
  realm.Unref();
  EXPECT_EQ(realm.ref_count(), 0);
}

TEST_F(RealmTest, Destructor) {
  absl::Notification started;
  absl::Notification finished;
  reffed_ptr<Realm const> ptr;
  std::thread thread{[&] {
    {
      Realm const realm{"foo"};
      ptr = &realm;
      started.Notify();
    }
    finished.Notify();
  }};
  started.WaitForNotification();
  EXPECT_EQ(ptr->name(), "foo");
  EXPECT_FALSE(finished.HasBeenNotified());
  ptr.reset();
  thread.join();
}

TEST_F(RealmTest, Name) {
  Realm const r1{"foo"};
  Realm const r2{"bar"};
  EXPECT_EQ(r1.name(), "foo");
  EXPECT_EQ(r2.name(), "bar");
}

TEST_F(RealmTest, NameCollision) {
  Realm const r1{"lorem"};
  EXPECT_DEATH({ Realm("lorem"); }, _);
}

TEST_F(RealmTest, GetRef) {
  Realm const realm{"lorem"};
  EXPECT_THAT(realm.get_ref(), Pointee2(Property(&Realm::name, "lorem")));
}

TEST_F(RealmTest, GetByName) {
  Realm const realm{"ipsum"};
  EXPECT_THAT(Realm::GetByName("ipsum"), IsOkAndHolds(Pointee2(Property(&Realm::name, "ipsum"))));
}

TEST_F(RealmTest, GetMissingByName) {
  EXPECT_THAT(Realm::GetByName("ipsum"), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(RealmTest, PredefinedRealms) {
  EXPECT_THAT(Realm::Default(), Pointee2(Property(&Realm::name, "default")));
  EXPECT_THAT(Realm::Meta(), Pointee2(Property(&Realm::name, "meta")));
  EXPECT_THAT(Realm::Huge(), Pointee2(Property(&Realm::name, "huge")));
}

}  // namespace tsz
