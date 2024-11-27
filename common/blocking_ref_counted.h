#ifndef __TSDB2_COMMON_BLOCKING_REF_COUNTED_H__
#define __TSDB2_COMMON_BLOCKING_REF_COUNTED_H__

#include <cstdint>
#include <utility>

#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"

namespace tsdb2 {
namespace common {

// Adds reference counting to a class, making it suitable for use with `reffed_ptr`, and prevents
// `this` from being deleted until the reference count drops to zero.
//
// We also provide `blocking_ptr<T>`, which is an alias for `reffed_ptr<BlockingRefCounted<T>>`.
//
// Example usage:
//
//   class MyClass {
//    public:
//     explicit MyClass(int arg1, double arg2);
//     virtual ~MyClass() = default;
//
//     void DoThisAndThat();
//   };
//
//   BlockingRefCounted<MyClass> rc{42, 3.14};
//   rc.DoThisAndThat();
//
//   void HandOut(blocking_ptr<MyClass> ptr);
//
//   HandOut(blocking_ptr<MyClass>(&rc));
//
//
// This can greatly simplify reference count-based object management because the owner of an object
// doesn't have to worry about waiting for all users to disappear before deleting the object. At the
// same time, care must be taken because destruction will be blocking, so the object owner must be
// aware that its destruction performance depends on how long users keep references for.
//
// `BlockingRefCounted` inherits the wrapped type T publicly, so all T members are accessible
// directly. This inheritance guarantees that none of the fields in T is destroyed before the
// reference count drops to zero, because the `~BlockingRefCounted` destructor is executed before
// the `~T` destructor and blocks until the reference count drops to zero.
//
// NOTE: for the design pattern to work, the "owner" must manage the BlockingRefCounted-wrapped
// object directly while all "users" have to use `reffed_ptr`. In particular, the implementation of
// `BlockingRefCounted::Unref` does NOT `delete this`. The owner is always in charge of deleting the
// object manually.
//
// NOTE: `BlockingRefCounted` REQUIRES that the wrapped type T has a virtual destructor.
template <typename T>
class BlockingRefCounted final : public T {
 public:
  // Perfect-fowards all arguments to the corresponding T constructor.
  template <typename... Args>
  explicit BlockingRefCounted(Args&&... args) : T(std::forward<Args>(args)...) {}

  // Blocks until the reference count drops to zero.
  ~BlockingRefCounted() override {
    absl::MutexLock(&mutex_, absl::Condition(this, &BlockingRefCounted::is_zero));
  }

  intptr_t ref_count() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    return ref_count_;
  }

  void Ref() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    ++ref_count_;
  }

  bool Unref() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    DCHECK_GT(ref_count_, 0);
    return --ref_count_ == 0;
  }

 private:
  BlockingRefCounted(BlockingRefCounted const&) = delete;
  BlockingRefCounted& operator=(BlockingRefCounted const&) = delete;
  BlockingRefCounted(BlockingRefCounted&&) = delete;
  BlockingRefCounted& operator=(BlockingRefCounted&&) = delete;

  bool is_zero() const ABSL_SHARED_LOCKS_REQUIRED(mutex_) { return ref_count_ == 0; }

  absl::Mutex mutable mutex_;
  intptr_t ref_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

template <typename T>
using blocking_ptr = reffed_ptr<BlockingRefCounted<T>>;

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_BLOCKING_REF_COUNTED_H__
