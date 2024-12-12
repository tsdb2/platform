#ifndef __TSDB2_COMMON_MUTEX_LOCK_H__
#define __TSDB2_COMMON_MUTEX_LOCK_H__

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

// A scoped object to acquire and release an exclusive lock on an `absl::Mutex`.
//
// In addition to providing all the functionality that `absl::WriterMutexLock` provides, this class
// is also movable and preemptible (the mutex can be unlocked manually before destruction by calling
// `Unlock`).
//
// WARNING: prefer using Abseil's classes. These classes can't be annotated with thread annotalysis
// because the compiler can't statically decide whether a mutex is still locked if unlocking is
// decided dynamically at runtime. Use this class only if you absolutely need to move a lock.
class WriterMutexLock final {
 public:
  explicit WriterMutexLock() : mutex_(nullptr) {}

  explicit WriterMutexLock(absl::Mutex *const mutex) : mutex_(mutex) { mutex_->WriterLock(); }

  explicit WriterMutexLock(absl::Mutex *const mutex, absl::Condition const &condition)
      : mutex_(mutex) {
    mutex_->WriterLockWhen(condition);
  }

  ~WriterMutexLock() { Unlock(); }

  WriterMutexLock(WriterMutexLock const &) = delete;
  WriterMutexLock &operator=(WriterMutexLock const &) = delete;

  WriterMutexLock(WriterMutexLock &&other) noexcept : mutex_(other.Release()) {}

  WriterMutexLock &operator=(WriterMutexLock &&other) noexcept {
    Unlock();
    mutex_ = other.Release();
    return *this;
  }

  absl::Mutex *Release() {
    auto *const mutex = mutex_;
    mutex_ = nullptr;
    return mutex;
  }

  void Unlock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    if (mutex_ != nullptr) {
      mutex_->WriterUnlock();
      mutex_ = nullptr;
    }
  }

 private:
  absl::Mutex *mutex_;
};

// A scoped object to acquire and release a shared lock on an `absl::Mutex`.
//
// In addition to providing all the functionality that `absl::ReaderMutexLock` provides, this class
// is also movable and preemptible (the mutex can be unlocked manually before destruction by calling
// `Unlock`).
//
// WARNING: prefer using Abseil's classes. These classes can't be annotated with thread annotalysis
// because the compiler can't statically decide whether a mutex is still locked if unlocking is
// decided dynamically at runtime. Use this class only if you absolutely need to move a lock.
class ReaderMutexLock final {
 public:
  explicit ReaderMutexLock() : mutex_(nullptr) {}

  explicit ReaderMutexLock(absl::Mutex *const mutex) : mutex_(mutex) { mutex_->ReaderLock(); }

  explicit ReaderMutexLock(absl::Mutex *const mutex, absl::Condition const &condition)
      : mutex_(mutex) {
    mutex_->ReaderLockWhen(condition);
  }

  ~ReaderMutexLock() { Unlock(); }

  ReaderMutexLock(ReaderMutexLock const &) = delete;
  ReaderMutexLock &operator=(ReaderMutexLock const &) = delete;

  ReaderMutexLock(ReaderMutexLock &&other) noexcept : mutex_(other.Release()) {}

  ReaderMutexLock &operator=(ReaderMutexLock &&other) noexcept {
    Unlock();
    mutex_ = other.Release();
    return *this;
  }

  absl::Mutex *Release() {
    auto *const mutex = mutex_;
    mutex_ = nullptr;
    return mutex;
  }

  void Unlock() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    if (mutex_ != nullptr) {
      mutex_->ReaderUnlock();
      mutex_ = nullptr;
    }
  }

 private:
  absl::Mutex *mutex_;
};

using MutexLock = WriterMutexLock;

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_MUTEX_LOCK_H__
