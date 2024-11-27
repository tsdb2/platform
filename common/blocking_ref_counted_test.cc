#include "common/blocking_ref_counted.h"

#include <string>
#include <string_view>
#include <thread>

#include "absl/synchronization/notification.h"
#include "common/reffed_ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::BlockingRefCounted;
using ::tsdb2::common::reffed_ptr;

class TestObject {
 public:
  explicit TestObject(std::string_view const label = "") : label_(label) {}

  virtual ~TestObject() = default;

  std::string_view label() const { return label_; }

 private:
  std::string const label_;
};

TEST(BlockingRefCountedTest, Constructor) {
  BlockingRefCounted<TestObject> rc{"foo"};
  EXPECT_EQ(rc.label(), "foo");
}

TEST(BlockingRefCountedTest, ReferenceCount) {
  BlockingRefCounted<TestObject> rc;
  EXPECT_EQ(rc.ref_count(), 0);
  rc.Ref();
  EXPECT_EQ(rc.ref_count(), 1);
  rc.Ref();
  EXPECT_EQ(rc.ref_count(), 2);
  EXPECT_FALSE(rc.Unref());
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_TRUE(rc.Unref());
  EXPECT_EQ(rc.ref_count(), 0);
}

TEST(BlockingRefCountedTest, Destructor) {
  absl::Notification started;
  absl::Notification finished;
  reffed_ptr<BlockingRefCounted<TestObject>> ptr;
  std::thread thread{[&] {
    {
      BlockingRefCounted<TestObject> rc{"foo"};
      ptr = &rc;
      started.Notify();
    }
    finished.Notify();
  }};
  started.WaitForNotification();
  EXPECT_EQ(ptr->label(), "foo");
  EXPECT_FALSE(finished.HasBeenNotified());
  ptr.reset();
  thread.join();
}

}  // namespace
