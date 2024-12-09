#ifndef __TSDB2_COMMON_LAZY_H__
#define __TSDB2_COMMON_LAZY_H__

#include <atomic>
#include <new>
#include <tuple>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

// Wraps an object that is constructed lazily on first access. This class is thread-safe.
//
// The `Lazy` can be constructed with a factory function that returns the wrapped object or with
// some arguments that are stored in the `Lazy` and then forwarded to the constructor upon first
// access.
//
// Once constructed the `Lazy` acts as a smart pointer with constness propagation. The `~Lazy`
// destructor will destroy the wrapped object only if it was constructed.
//
// Example with a factory function:
//
//   Lazy<Foo> lazy_foo{[] {
//     return Foo("bar", 42);
//   }};
//   lazy_foo->baz();
//
// Corresponding example with forwarded arguments:
//
//   Lazy<Foo> lazy_foo{std::in_place, "bar", 42};
//   lazy_foo->baz();
//
// The `std::in_place` argument resolves the `Lazy` constructor overload to the forwarding one.
//
// NOTE: accessing the object after it has been constructed is very fast, as it only requires one
// extra atomic read with acquire barrier. The internal mutex is used only if the object hasn't yet
// been constructed, and it ensures that construction happens only once.
template <typename Value>
class Lazy {
 public:
  using Factory = absl::AnyInvocable<Value()>;

  explicit Lazy(Factory factory) : factory_(std::move(factory)) {}

  template <typename... Args>
  explicit Lazy(std::in_place_t, Args&&... args)
      : factory_([args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
          return std::make_from_tuple<Value>(std::move(args));
        }) {}

  // Destroys the wrapped value.
  //
  // NOTE: the destructor assumes no other threads are using the object.
  ~Lazy() {
    if (constructed_.load(std::memory_order_relaxed)) {
      reinterpret_cast<Value*>(storage_)->~Value();
    }
  }

  // Determines whether the wrapped object has been constructed.
  //
  // NOTE: the returned value is merely advisory; by the time this function returns, any number of
  // accesses may have been performed concurrently by other threads, therefore triggering
  // construction.
  bool is_constructed() const { return constructed_.load(std::memory_order_relaxed); }

  // Alias for `is_constructed`.
  explicit operator bool() const { return constructed_.load(std::memory_order_relaxed); }

  Value* get() { return GetInternal(); }
  Value const* get() const { return GetInternal(); }

  Value* operator->() { return get(); }
  Value const* operator->() const { return get(); }

  Value& operator*() { return *get(); }
  Value const& operator*() const { return *get(); }

 private:
  Lazy(Lazy const&) = delete;
  Lazy& operator=(Lazy const&) = delete;
  Lazy(Lazy&&) = delete;
  Lazy& operator=(Lazy&&) = delete;

  void Construct() const {
    Factory discarded;
    absl::MutexLock lock{&mutex_};
    if (!constructed_.load(std::memory_order_relaxed)) {
      new (storage_) Value(factory_());
      constructed_.store(true, std::memory_order_release);
      factory_.swap(discarded);
    }
  }

  Value* GetInternal() const {
    if (ABSL_PREDICT_FALSE(!constructed_.load(std::memory_order_acquire))) {
      Construct();
    }
    return reinterpret_cast<Value*>(storage_);
  }

  absl::Mutex mutable mutex_;
  std::atomic<bool> mutable constructed_{false};
  Factory mutable factory_ ABSL_GUARDED_BY(mutex_);
  alignas(Value) char mutable storage_[sizeof(Value)];
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LAZY_H__
