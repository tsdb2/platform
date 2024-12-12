#include "common/ref_count.h"

#include <memory>

#include "common/reffed_ptr.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MakeReffed;
using ::tsdb2::common::RefCount;
using ::tsdb2::common::RefCounted;
using ::tsdb2::common::SimpleRefCounted;

TEST(RefCountTest, Initial) {
  RefCount rc;
  EXPECT_FALSE(rc.is_referenced());
  EXPECT_FALSE(rc.is_last());
}

TEST(RefCountTest, Ref) {
  RefCount rc;
  rc.Ref();
  EXPECT_TRUE(rc.is_referenced());
  EXPECT_TRUE(rc.is_last());
}

TEST(RefCountTest, RefUnref) {
  RefCount rc;
  rc.Ref();
  EXPECT_TRUE(rc.Unref());
  EXPECT_FALSE(rc.is_referenced());
  EXPECT_FALSE(rc.is_last());
}

TEST(RefCountTest, RefRef) {
  RefCount rc;
  rc.Ref();
  rc.Ref();
  EXPECT_TRUE(rc.is_referenced());
  EXPECT_FALSE(rc.is_last());
}

TEST(RefCountTest, RefRefUnref) {
  RefCount rc;
  rc.Ref();
  rc.Ref();
  EXPECT_FALSE(rc.Unref());
  EXPECT_TRUE(rc.is_referenced());
  EXPECT_TRUE(rc.is_last());
}

TEST(RefCountTest, RefRefUnrefUnref) {
  RefCount rc;
  rc.Ref();
  rc.Ref();
  EXPECT_FALSE(rc.Unref());
  EXPECT_TRUE(rc.Unref());
  EXPECT_FALSE(rc.is_referenced());
  EXPECT_FALSE(rc.is_last());
}

class TestRefCounted : public RefCounted {
 public:
  explicit TestRefCounted(bool *const flag) : flag_(flag) {}

 protected:
  void OnLastUnref() override { *flag_ = true; }

 private:
  bool *const flag_;
};

class RefCountedTest : public ::testing::Test {
 protected:
  bool flag_ = false;
  TestRefCounted rc_{&flag_};
};

TEST_F(RefCountedTest, Initial) {
  EXPECT_FALSE(flag_);
  EXPECT_FALSE(rc_.is_referenced());
  EXPECT_FALSE(rc_.is_last());
}

TEST_F(RefCountedTest, RefUnref) {
  rc_.Ref();
  EXPECT_FALSE(flag_);
  EXPECT_TRUE(rc_.is_referenced());
  EXPECT_TRUE(rc_.is_last());
  rc_.Unref();
  EXPECT_TRUE(flag_);
  EXPECT_FALSE(rc_.is_referenced());
  EXPECT_FALSE(rc_.is_last());
}

TEST_F(RefCountedTest, RefRefUnrefUnref) {
  rc_.Ref();
  rc_.Ref();
  EXPECT_FALSE(flag_);
  EXPECT_TRUE(rc_.is_referenced());
  EXPECT_FALSE(rc_.is_last());
  rc_.Unref();
  EXPECT_FALSE(flag_);
  EXPECT_TRUE(rc_.is_referenced());
  EXPECT_TRUE(rc_.is_last());
  rc_.Unref();
  EXPECT_TRUE(flag_);
  EXPECT_FALSE(rc_.is_referenced());
  EXPECT_FALSE(rc_.is_last());
}

class TestSimpleRefCounted : public SimpleRefCounted {
 public:
  explicit TestSimpleRefCounted(bool *const flag) : flag_(flag) {}
  ~TestSimpleRefCounted() override { *flag_ = true; }

 private:
  TestSimpleRefCounted(TestSimpleRefCounted const &) = delete;
  TestSimpleRefCounted &operator=(TestSimpleRefCounted const &) = delete;
  TestSimpleRefCounted(TestSimpleRefCounted &&) = delete;
  TestSimpleRefCounted &operator=(TestSimpleRefCounted &&) = delete;

  bool *const flag_;
};

class SimpleRefCountedTest : public ::testing::Test {
 protected:
  bool flag_ = false;
};

TEST_F(SimpleRefCountedTest, UniquePtr) {
  {
    auto rc = std::make_unique<TestSimpleRefCounted>(&flag_);
    EXPECT_FALSE(flag_);
  }
  EXPECT_TRUE(flag_);
}

TEST_F(SimpleRefCountedTest, RefUnref) {
  {
    auto rc = MakeReffed<TestSimpleRefCounted>(&flag_);
    EXPECT_FALSE(flag_);
  }
  EXPECT_TRUE(flag_);
}

TEST_F(SimpleRefCountedTest, RefRefUnrefUnref) {
  {
    auto rc1 = MakeReffed<TestSimpleRefCounted>(&flag_);
    {
      auto rc2 = rc1;  // NOLINT(performance-unnecessary-copy-initialization)
      EXPECT_FALSE(flag_);
    }
    EXPECT_FALSE(flag_);
  }
  EXPECT_TRUE(flag_);
}

}  // namespace
