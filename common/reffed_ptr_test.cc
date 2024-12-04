#include "common/reffed_ptr.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/hash/hash.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MakeReffed;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::WrapReffed;

class RefCounted {
 public:
  void Ref() { ++ref_count_; }
  void Unref() { --ref_count_; }

  intptr_t ref_count() const { return ref_count_; }

 private:
  intptr_t ref_count_ = 0;
};

class Derived : public RefCounted {
 public:
  explicit Derived() : field_(0) {}
  explicit Derived(int const field) : field_(field) {}

  int field() const { return field_; }

 private:
  int field_;
};

TEST(ReffedPtrTest, DefaultConstructor) {
  reffed_ptr<RefCounted> ptr;
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_TRUE(ptr.empty());
  EXPECT_FALSE(ptr.operator bool());
}

TEST(ReffedPtrTest, NullptrConstructor) {
  reffed_ptr<RefCounted> ptr{nullptr};
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_TRUE(ptr.empty());
  EXPECT_FALSE(ptr.operator bool());
}

TEST(ReffedPtrTest, PointerConstructor) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr{&rc};
  EXPECT_EQ(ptr.get(), &rc);
  EXPECT_FALSE(ptr.empty());
  EXPECT_TRUE(ptr.operator bool());
  EXPECT_EQ(rc.ref_count(), 1);
}

TEST(ReffedPtrTest, UniquePointerConstructor) {
  auto uptr = std::make_unique<RefCounted>();
  auto const rc = uptr.get();
  reffed_ptr<RefCounted> ptr{std::move(uptr)};
  EXPECT_EQ(ptr.get(), rc);
  EXPECT_FALSE(ptr.empty());
  EXPECT_TRUE(ptr.operator bool());
  EXPECT_EQ(rc->ref_count(), 1);
}

TEST(ReffedPtrTest, ConstructFromEmptyUniquePointer) {
  std::unique_ptr<RefCounted> uptr;
  reffed_ptr<RefCounted> ptr{std::move(uptr)};
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_TRUE(ptr.empty());
  EXPECT_FALSE(ptr.operator bool());
}

TEST(ReffedPtrTest, CopyConstructor) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2{ptr1};
  EXPECT_EQ(rc.ref_count(), 2);
}

TEST(ReffedPtrTest, AssignableCopyConstructor) {
  Derived rc;
  reffed_ptr<Derived> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2{ptr1};
  EXPECT_EQ(rc.ref_count(), 2);
}

TEST(ReffedPtrTest, MoveConstructor) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2{std::move(ptr1)};
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_FALSE(ptr1.operator bool());
}

TEST(ReffedPtrTest, AssignableMoveConstructor) {
  Derived rc;
  reffed_ptr<Derived> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2{std::move(ptr1)};
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_FALSE(ptr1.operator bool());
}

TEST(ReffedPtrTest, Destructor) {
  RefCounted rc;
  {
    reffed_ptr<RefCounted> ptr{&rc};
    ASSERT_EQ(ptr.get(), &rc);
    ASSERT_TRUE(ptr.operator bool());
    ASSERT_EQ(rc.ref_count(), 1);
  }
  EXPECT_EQ(rc.ref_count(), 0);
}

TEST(ReffedPtrTest, Destructors) {
  RefCounted rc;
  {
    reffed_ptr<RefCounted> ptr1{&rc};
    ASSERT_EQ(ptr1.get(), &rc);
    ASSERT_TRUE(ptr1.operator bool());
    ASSERT_EQ(rc.ref_count(), 1);
    {
      reffed_ptr<RefCounted> ptr2{ptr1};
      ASSERT_EQ(ptr2.get(), &rc);
      ASSERT_TRUE(ptr2.operator bool());
      EXPECT_EQ(rc.ref_count(), 2);
    }
    EXPECT_EQ(rc.ref_count(), 1);
  }
  EXPECT_EQ(rc.ref_count(), 0);
}

TEST(ReffedPtrTest, CopyAssignmentOperator) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2;
  ptr2 = ptr1;
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 2);
}

TEST(ReffedPtrTest, AssignableCopyAssignmentOperator) {
  Derived rc;
  reffed_ptr<Derived> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2;
  ptr2 = ptr1;
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 2);
}

TEST(ReffedPtrTest, MoveOperator) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2;
  ptr2 = std::move(ptr1);
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_FALSE(ptr1.operator bool());
}

TEST(ReffedPtrTest, AssignableMoveOperator) {
  Derived rc;
  reffed_ptr<Derived> ptr1{&rc};
  reffed_ptr<RefCounted> ptr2;
  ptr2 = std::move(ptr1);
  EXPECT_EQ(ptr2.get(), &rc);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr1.get(), nullptr);
  EXPECT_FALSE(ptr1.operator bool());
}

TEST(ReffedPtrTest, PointerAssignmentOperator) {
  RefCounted rc1;
  Derived rc2;
  reffed_ptr<RefCounted> ptr{&rc1};
  ptr = &rc2;
  EXPECT_EQ(ptr.get(), &rc2);
  EXPECT_TRUE(ptr.operator bool());
  EXPECT_EQ(rc1.ref_count(), 0);
  EXPECT_EQ(rc2.ref_count(), 1);
}

TEST(ReffedPtrTest, NullptrAssignment) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr{&rc};
  ASSERT_EQ(rc.ref_count(), 1);
  ptr = nullptr;
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_TRUE(ptr.empty());
  EXPECT_FALSE(ptr.operator bool());
  EXPECT_EQ(rc.ref_count(), 0);
}

TEST(ReffedPtrTest, AssignUniquePointer) {
  auto uptr = std::make_unique<RefCounted>();
  auto const rc = uptr.get();
  reffed_ptr<RefCounted> ptr;
  ptr = std::move(uptr);
  EXPECT_EQ(ptr.get(), rc);
  EXPECT_FALSE(ptr.empty());
  EXPECT_TRUE(ptr.operator bool());
  EXPECT_EQ(rc->ref_count(), 1);
}

TEST(ReffedPtrTest, AssignEmptyUniquePointer) {
  std::unique_ptr<RefCounted> uptr;
  reffed_ptr<RefCounted> ptr;
  ptr = std::move(uptr);
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_TRUE(ptr.empty());
  EXPECT_FALSE(ptr.operator bool());
}

TEST(ReffedPtrTest, Release) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr{&rc};
  auto const released = ptr.release();
  EXPECT_EQ(released, &rc);
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_FALSE(ptr.operator bool());
}

TEST(ReffedPtrTest, Reset) {
  RefCounted rc;
  reffed_ptr<RefCounted> ptr{&rc};
  ptr.reset();
  EXPECT_EQ(ptr.get(), nullptr);
  EXPECT_FALSE(ptr.operator bool());
  EXPECT_EQ(rc.ref_count(), 0);
}

TEST(ReffedPtrTest, ResetWith) {
  RefCounted rc1;
  Derived rc2;
  reffed_ptr<RefCounted> ptr{&rc1};
  ptr.reset(&rc2);
  EXPECT_EQ(ptr.get(), &rc2);
  EXPECT_TRUE(ptr.operator bool());
  EXPECT_EQ(rc1.ref_count(), 0);
  EXPECT_EQ(rc2.ref_count(), 1);
}

TEST(ReffedPtrTest, Swap) {
  RefCounted rc1;
  RefCounted rc2;
  reffed_ptr<RefCounted> ptr1{&rc1};
  reffed_ptr<RefCounted> ptr2{&rc2};
  ptr1.swap(ptr2);
  EXPECT_EQ(ptr1.get(), &rc2);
  EXPECT_TRUE(ptr1.operator bool());
  EXPECT_EQ(ptr2.get(), &rc1);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc1.ref_count(), 1);
  EXPECT_EQ(rc2.ref_count(), 1);
}

TEST(ReffedPtrTest, AdlSwap) {
  static_assert(std::is_nothrow_swappable_v<reffed_ptr<RefCounted>>);
  RefCounted rc1;
  RefCounted rc2;
  reffed_ptr<RefCounted> ptr1{&rc1};
  reffed_ptr<RefCounted> ptr2{&rc2};
  swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &rc2);
  EXPECT_TRUE(ptr1.operator bool());
  EXPECT_EQ(ptr2.get(), &rc1);
  EXPECT_TRUE(ptr2.operator bool());
  EXPECT_EQ(rc1.ref_count(), 1);
  EXPECT_EQ(rc2.ref_count(), 1);
}

TEST(ReffedPtrTest, Dereference) {
  Derived rc{42};
  reffed_ptr<Derived> ptr{&rc};
  EXPECT_EQ((*ptr).field(), 42);
  EXPECT_EQ(ptr->field(), 42);
}

TEST(ReffedPtrTest, Hash) {
  RefCounted rc1;
  RefCounted rc2;
  reffed_ptr<RefCounted> ptr1{&rc1};
  reffed_ptr<RefCounted> ptr2{&rc1};
  reffed_ptr<RefCounted> ptr3{&rc2};
  EXPECT_EQ(absl::HashOf(ptr1), absl::HashOf(ptr2));
  EXPECT_NE(absl::HashOf(ptr1), absl::HashOf(ptr3));
}

TEST(ReffedPtrTest, EqualityOperators) {
  RefCounted rc1;
  reffed_ptr<RefCounted> ptr1{&rc1};
  reffed_ptr<RefCounted> ptr2{&rc1};
  RefCounted rc2;
  reffed_ptr<RefCounted> ptr3{&rc2};
  reffed_ptr<RefCounted> ptr4;
  EXPECT_FALSE(ptr1 == nullptr);
  EXPECT_FALSE(nullptr == ptr1);
  EXPECT_TRUE(ptr1 != nullptr);
  EXPECT_TRUE(nullptr != ptr1);
  EXPECT_TRUE(ptr1 == ptr2);
  EXPECT_FALSE(ptr1 != ptr2);
  EXPECT_FALSE(ptr1 == ptr3);
  EXPECT_TRUE(ptr1 != ptr3);
  EXPECT_FALSE(ptr2 == ptr3);
  EXPECT_TRUE(ptr2 != ptr3);
  EXPECT_TRUE(ptr4 == nullptr);
  EXPECT_TRUE(nullptr == ptr4);
  EXPECT_FALSE(ptr4 != nullptr);
  EXPECT_FALSE(nullptr != ptr4);
}

TEST(ReffedPtrTest, ComparisonOperators) {
  RefCounted rc1, rc2;
  reffed_ptr<RefCounted> ptr1{&rc1};
  reffed_ptr<RefCounted> ptr2{&rc2};
  reffed_ptr<RefCounted> ptr3;
  if (&rc2 < &rc1) {
    ptr1.swap(ptr2);
  }
  EXPECT_FALSE(ptr1 < nullptr);
  EXPECT_TRUE(nullptr < ptr1);
  EXPECT_FALSE(ptr1 <= nullptr);
  EXPECT_TRUE(nullptr <= ptr1);
  EXPECT_TRUE(ptr1 > nullptr);
  EXPECT_FALSE(nullptr > ptr1);
  EXPECT_TRUE(ptr1 >= nullptr);
  EXPECT_FALSE(nullptr >= ptr1);
  EXPECT_FALSE(ptr1 < ptr1);
  EXPECT_FALSE(ptr1 < ptr1);
  EXPECT_TRUE(ptr1 <= ptr1);
  EXPECT_TRUE(ptr1 <= ptr1);
  EXPECT_TRUE(ptr1 < ptr2);
  EXPECT_FALSE(ptr2 < ptr1);
  EXPECT_TRUE(ptr1 <= ptr2);
  EXPECT_FALSE(ptr2 <= ptr1);
  EXPECT_FALSE(ptr1 > ptr2);
  EXPECT_TRUE(ptr2 > ptr1);
  EXPECT_FALSE(ptr1 >= ptr2);
  EXPECT_TRUE(ptr2 >= ptr1);
  EXPECT_FALSE(ptr1 > ptr1);
  EXPECT_FALSE(ptr1 > ptr1);
  EXPECT_TRUE(ptr1 >= ptr1);
  EXPECT_TRUE(ptr1 >= ptr1);
  EXPECT_FALSE(ptr3 < nullptr);
  EXPECT_FALSE(nullptr < ptr3);
  EXPECT_TRUE(ptr3 <= nullptr);
  EXPECT_TRUE(nullptr <= ptr3);
  EXPECT_FALSE(ptr3 > nullptr);
  EXPECT_FALSE(nullptr > ptr3);
  EXPECT_TRUE(ptr3 >= nullptr);
  EXPECT_TRUE(nullptr >= ptr3);
}

TEST(ReffedPtrTest, Downcast) {
  Derived d{42};
  reffed_ptr<RefCounted> ptr1{&d};
  ASSERT_EQ(d.ref_count(), 1);
  {
    auto const ptr2 = ptr1.downcast<Derived>();
    EXPECT_TRUE(ptr2.operator bool());
    EXPECT_EQ(ptr2->field(), 42);
    EXPECT_EQ(d.ref_count(), 2);
    EXPECT_TRUE(ptr1.operator bool());
    EXPECT_EQ(ptr2->field(), 42);
  }
  EXPECT_EQ(d.ref_count(), 1);
}

TEST(ReffedPtrTest, DowncastTemp) {
  Derived d{42};
  reffed_ptr<RefCounted> ptr1{&d};
  ASSERT_EQ(d.ref_count(), 1);
  {
    auto const ptr2 = std::move(ptr1).downcast<Derived>();
    EXPECT_TRUE(ptr2.operator bool());
    EXPECT_EQ(ptr2->field(), 42);
    EXPECT_EQ(d.ref_count(), 1);
    EXPECT_FALSE(ptr1.operator bool());
  }
  EXPECT_EQ(d.ref_count(), 0);
}

TEST(ReffedPtrTest, WrapReffed) {
  Derived rc{42};
  ASSERT_EQ(rc.ref_count(), 0);
  auto ptr = WrapReffed(&rc);
  EXPECT_EQ(rc.ref_count(), 1);
  EXPECT_EQ(ptr->field(), 42);
}

class HeapRefCounted {
 public:
  explicit HeapRefCounted(int const label) : label_(label) {}

  intptr_t ref_count() const { return ref_count_; }

  void Ref() { ++ref_count_; }

  void Unref() {
    if (--ref_count_ < 1) {
      delete this;
    }
  }

  int label() const { return label_; }

 private:
  intptr_t ref_count_ = 0;
  int const label_;
};

TEST(ReffedPtrTest, MakeReffed) {
  auto ptr = MakeReffed<HeapRefCounted>(42);
  EXPECT_EQ(ptr->ref_count(), 1);
  EXPECT_EQ(ptr->label(), 42);
}

}  // namespace
