#ifndef __TSDB2_COMMON_REFFED_PTR_H__
#define __TSDB2_COMMON_REFFED_PTR_H__

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace tsdb2 {
namespace common {

// `reffed_ptr` is a smart pointer class that behaves almost identically to `std::shared_ptr` except
// that it defers all reference counting to the wrapped object rather than implementing its own. As
// such it has an intrusive API requiring that the wrapped object provides two methods called `Ref`
// and `Unref`.
//
// This has multiple benefits:
//
// * Unlike `std::shared_ptr` there's no risk of keeping multiple separate reference counts because
//   the reference count value is managed by the wrapped object rather than by `reffed_ptr` itself.
// * `reffed_ptr` doesn't need to allocate a separate memory block for the target / reference count.
// * `reffed_ptr` allows implementing custom reference counting schemes, e.g. the destructor of the
//   wrapped object may block until the reference count drops to zero (see `BlockingRefCounted`).
template <typename T>
class reffed_ptr final {
 public:
  reffed_ptr() : ptr_(nullptr) {}

  reffed_ptr(std::nullptr_t) : ptr_(nullptr) {}  // NOLINT(google-explicit-constructor)

  template <typename U>
  explicit reffed_ptr(U* const ptr) : ptr_(ptr) {
    MaybeRef();
  }

  reffed_ptr(reffed_ptr const& other) : ptr_(other.ptr_) { MaybeRef(); }

  template <typename U>
  reffed_ptr(  // NOLINT(google-explicit-constructor)
      reffed_ptr<U> const& other)
      : ptr_(other.get()) {
    MaybeRef();
  }

  template <typename U, typename Deleter>
  reffed_ptr(  // NOLINT(google-explicit-constructor)
      std::unique_ptr<U, Deleter>&& ptr)
      : ptr_(ptr.release()) {
    MaybeRef();
  }

  reffed_ptr(reffed_ptr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

  template <typename U>
  reffed_ptr(  // NOLINT(google-explicit-constructor)
      reffed_ptr<U>&& other) noexcept
      : ptr_(other.release()) {}

  ~reffed_ptr() { MaybeUnref(); }

  reffed_ptr& operator=(reffed_ptr const& other) {
    if (this != &other) {
      reset(other.ptr_);
    }
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(reffed_ptr<U> const& other) {
    reset(other.get());
    return *this;
  }

  reffed_ptr& operator=(reffed_ptr&& other) noexcept {
    if (this != &other) {
      MaybeUnref();
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(reffed_ptr<U>&& other) noexcept {
    MaybeUnref();
    ptr_ = other.release();
    return *this;
  }

  reffed_ptr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(U* const ptr) {
    reset(ptr);
    return *this;
  }

  template <typename U, typename Deleter>
  reffed_ptr& operator=(std::unique_ptr<U, Deleter>&& ptr) {
    reset(ptr.release());
    return *this;
  }

  // Releases and returns the wrapped pointer without unreffing it.
  T* release() {
    T* const ptr = ptr_;
    ptr_ = nullptr;
    return ptr;
  }

  // Unrefs the wrapped pointer if it's not null and empties the `reffed_ptr`.
  //
  // This is the same as `reset(nullptr)`.
  void reset() {
    MaybeUnref();
    ptr_ = nullptr;
  }

  // Unrefs the wrapped pointer if any and wraps the provided one, reffing it if not null.
  template <typename U>
  void reset(U* const ptr) {
    MaybeUnref();
    ptr_ = ptr;
    MaybeRef();
  }

  // Swaps two `reffed_ptr`s. The reference counts are not changed.
  void swap(reffed_ptr& other) { std::swap(ptr_, other.ptr_); }
  friend void swap(reffed_ptr& lhs, reffed_ptr& rhs) noexcept { std::swap(lhs.ptr_, rhs.ptr_); }

  // Returns the wrapped raw pointer.
  T* get() const { return ptr_; }

  // Returns `true` iff the wrapped pointer is == nullptr.
  [[nodiscard]] bool empty() const { return ptr_ == nullptr; }

  // Returns `true` iff the wrapped pointer is != nullptr.
  explicit operator bool() const { return ptr_ != nullptr; }

  // Dereference the wrapped pointer.
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  // Casts the wrapped type `T` to any type `U` that inherits `T`.
  //
  // REQUIRES: the pointed object MUST have type `U`.
  //
  // WARNING: this function doesn't check the actual type of the pointed value in any way.
  // Downcasting to an incorrect type will result in undefined behavior.
  //
  // NOTE: we use `reinterpret_cast` rather than `dynamic_cast` because the latter requires the base
  // class to be polymorphic (i.e. it needs to have a vtable), which we can't assume.
  template <typename U, std::enable_if_t<std::is_base_of_v<T, U>, bool> = true>
  reffed_ptr<U> downcast() const& {
    return reffed_ptr<U>(reinterpret_cast<U*>(ptr_));
  }

  // Casts the wrapped type `T` to any type `U` that inherits `T`.
  //
  // REQUIRES: the pointed object MUST have type `U`.
  //
  // WARNING: this function doesn't check the actual type of the pointed value in any way.
  // Downcasting to an incorrect type will result in undefined behavior.
  //
  // NOTE: we use `reinterpret_cast` rather than `dynamic_cast` because the latter requires the base
  // class to be polymorphic (i.e. it needs to have a vtable), which we can't assume.
  template <typename U, std::enable_if_t<std::is_base_of_v<T, U>, bool> = true>
  reffed_ptr<U> downcast() && {
    U* const ptr = reinterpret_cast<U*>(ptr_);
    ptr_ = nullptr;
    return reffed_ptr<U>(reffed_ptr<U>::kAdoptPointer, ptr);
  }

  template <typename H>
  friend H AbslHashValue(H h, reffed_ptr<T> const& ptr) {
    return H::combine(std::move(h), ptr.get());
  }

  template <typename U>
  friend bool operator==(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() == rhs.get();
  }

  template <typename U>
  friend bool operator!=(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() != rhs.get();
  }

  template <typename U>
  friend bool operator<(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() < rhs.get();
  }

  template <typename U>
  friend bool operator>(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() > rhs.get();
  }

  template <typename U>
  friend bool operator<=(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() <= rhs.get();
  }

  template <typename U>
  friend bool operator>=(reffed_ptr<T> const& lhs, reffed_ptr<U> const& rhs) noexcept {
    return lhs.get() >= rhs.get();
  }

  friend bool operator==(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
  }

  friend bool operator==(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept {
    return nullptr == rhs.get();
  }

  friend bool operator!=(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
  }

  friend bool operator!=(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept {
    return nullptr != rhs.get();
  }

  friend bool operator<(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept { return false; }

  friend bool operator<(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept {
    return rhs.get() != nullptr;
  }

  friend bool operator>(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
  }

  friend bool operator>(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept { return false; }

  friend bool operator<=(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
  }

  friend bool operator<=(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept { return true; }

  friend bool operator>=(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept { return true; }

  friend bool operator>=(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept {
    return rhs.get() == nullptr;
  }

 private:
  template <typename U>
  friend class reffed_ptr;

  struct AdoptPointer {};
  static inline AdoptPointer constexpr kAdoptPointer{};

  explicit reffed_ptr(AdoptPointer /*adopt_pointer*/, T* const ptr) : ptr_(ptr) {}

  void MaybeRef() {
    if (ptr_) {
      ptr_->Ref();
    }
  }

  void MaybeUnref() {
    if (ptr_) {
      ptr_->Unref();
    }
  }

  T* ptr_;
};

// Wraps a raw pointer in a `reffed_ptr`, incrementing the reference count by 1.
template <typename T>
reffed_ptr<T> WrapReffed(T* const value) {
  return reffed_ptr<T>(value);
}

// Constructs a new object of type T and wraps it in a `reffed_ptr<T>`.
//
// WARNING: since the wrapped object is constructed using the `new` operator, the implementation of
// `Unref` in the wrapped object MUST delete the object using the `delete` operator when the
// reference count drops to zero, otherwise the object's memory gets leaked.
//
// NOTE: the reference count of `T` must be initialized to 0. The returned `reffed_ptr` will bump it
// to 1 when acquiring the pointer.
template <typename T, typename... Args>
reffed_ptr<T> MakeReffed(Args&&... args) {
  return reffed_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_REFFED_PTR_H__
