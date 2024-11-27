#ifndef __TSDB2_COMMON_OVERRIDABLE_H__
#define __TSDB2_COMMON_OVERRIDABLE_H__

#include <atomic>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

// Allows overriding a value of type T for testing purposes, e.g. replacing with a mock
// implementation.
//
// Note that `Overridable` makes `T` non-copyable and non-movable because it encapsulates an atomic
// and a mutex, and also because `ScopedOverride` objects will retain pointers to it.
//
// The internal mutex is used only to prevent concurrent constructions from `Override()` calls, but
// retrieving the regular value in the absence of an override does not block and is very fast.
template <typename T>
class Overridable {
 public:
  template <typename... Args>
  explicit Overridable(Args&&... args) : value_(std::forward<Args>(args)...) {}

  // TEST ONLY: replace the wrapped value with a different one.
  void Override(T* const value) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    override_ = value;
    overridden_.store(true, std::memory_order_release);
  }

  // TEST ONLY: replace the wrapped value with a different one, checkfailing if a different override
  // is already in place.
  void OverrideOrDie(T* const value) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    CHECK(!override_);
    override_ = value;
    overridden_.store(true, std::memory_order_release);
  }

  // TEST ONLY: restore the original value and destroy the override, if any.
  void Restore() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    override_ = nullptr;
    overridden_.store(false, std::memory_order_release);
  }

  // Retrieves the wrapped value.
  T* Get() { return GetInternal(); }
  T const* Get() const { return GetInternal(); }

  T* operator->() { return Get(); }
  T const* operator->() const { return Get(); }

  T& operator*() { return *Get(); }
  T const& operator*() const { return *Get(); }

 private:
  Overridable(Overridable const&) = delete;
  Overridable& operator=(Overridable const&) = delete;
  Overridable(Overridable&&) = delete;
  Overridable& operator=(Overridable&&) = delete;

  T* GetInternal() const ABSL_LOCKS_EXCLUDED(mutex_) {
    if (ABSL_PREDICT_FALSE(overridden_.load(std::memory_order_relaxed))) {
      absl::MutexLock lock{&mutex_};
      if (override_) {
        return override_;
      } else {
        return &value_;
      }
    } else {
      return &value_;
    }
  }

  T mutable value_;

  std::atomic<bool> overridden_{false};
  absl::Mutex mutable mutex_;
  T* override_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_OVERRIDABLE_H__
