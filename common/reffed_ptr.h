#ifndef __TSDB2_COMMON_REFFED_PTR_H__
#define __TSDB2_COMMON_REFFED_PTR_H__

#include <algorithm>
#include <cstddef>
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
//   the target containing the reference count value is managed by the wrapped object rather than by
//   `reffed_ptr`.
// * `reffed_ptr` doesn't need to allocate a separate memory block for the target / reference count.
// * `reffed_ptr` allows implementing custom reference counting schemes, e.g. the destructor of the
//   wrapped object may block until the reference count drops to zero (see `BlockingRefCounted`).
template <typename T>
class reffed_ptr final {
 public:
  reffed_ptr() : ptr_(nullptr) {}

  reffed_ptr(std::nullptr_t) : ptr_(nullptr) {}

  template <typename U>
  explicit reffed_ptr(U* const ptr) : ptr_(ptr) {
    MaybeRef();
  }

  reffed_ptr(reffed_ptr const& other) : ptr_(other.ptr_) { MaybeRef(); }

  template <typename U>
  reffed_ptr(reffed_ptr<U> const& other) : ptr_(other.get()) {
    MaybeRef();
  }

  reffed_ptr(reffed_ptr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

  template <typename U>
  reffed_ptr(reffed_ptr<U>&& other) noexcept : ptr_(other.release()) {}

  ~reffed_ptr() { MaybeUnref(); }

  reffed_ptr& operator=(reffed_ptr const& other) {
    reset(other.ptr_);
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(reffed_ptr<U> const& other) {
    reset(other.get());
    return *this;
  }

  reffed_ptr& operator=(reffed_ptr&& other) noexcept {
    MaybeUnref();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(reffed_ptr<U>&& other) noexcept {
    MaybeUnref();
    ptr_ = other.release();
    return *this;
  }

  template <typename U>
  reffed_ptr& operator=(U* const ptr) {
    reset(ptr);
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

  // Swaps two `reffed_ptr`s. No reference counts are changed.
  void swap(reffed_ptr& other) { std::swap(ptr_, other.ptr_); }

  // Returns the wrapped raw pointer.
  T* get() const { return ptr_; }

  // Dereference the wrapped pointer.
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  // Returns `true` iff the wrapped pointer is != nullptr.
  explicit operator bool() const { return ptr_ != nullptr; }

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
    return nullptr < rhs.get();
  }

  friend bool operator>(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() > nullptr;
  }

  friend bool operator>(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept { return false; }

  friend bool operator<=(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept {
    return lhs.get() <= nullptr;
  }

  friend bool operator<=(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept { return true; }

  friend bool operator>=(reffed_ptr<T> const& lhs, std::nullptr_t) noexcept { return true; }

  friend bool operator>=(std::nullptr_t, reffed_ptr<T> const& rhs) noexcept {
    return nullptr >= rhs.get();
  }

 private:
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

// Constructs a new object of type T and wraps it in a `reffed_ptr<T>`.
//
// WARNING: since the wrapped object is constructed using the `new` operator, the implementation of
// `Unref` in the wrapped object MUST delete the object using the `delete` operator when the
// reference count drops to zero, otherwise the object's memory gets leaked.
template <typename T, typename... Args>
auto MakeReffed(Args&&... args) {
  return reffed_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_REFFED_PTR_H__
