#ifndef __TSDB2_COMMON_NO_DESTRUCTOR_H__
#define __TSDB2_COMMON_NO_DESTRUCTOR_H__

#include <utility>

namespace tsdb2 {
namespace common {

// Wraps an object making it trivially destructible.
//
// You would typically use `NoDestructor` to instantiate a class in global scope even if it's not
// trivially destructible.
//
// WARNING: since the original destructor is never executed you should only wrap objects that are
// meant to stay alive for the whole life of the process, otherwise it may result in serious memory
// leaks.
template <typename T>
class NoDestructor {
 public:
  template <typename... Args>
  explicit constexpr NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  T* Get() { return reinterpret_cast<T*>(storage_); }
  T const* Get() const { return reinterpret_cast<T const*>(storage_); }

  T* operator->() { return Get(); }
  T const* operator->() const { return Get(); }

  T& operator*() { return *Get(); }
  T const& operator*() const { return *Get(); }

 private:
  NoDestructor(NoDestructor const&) = delete;
  NoDestructor& operator=(NoDestructor const&) = delete;
  NoDestructor(NoDestructor&&) = delete;
  NoDestructor& operator=(NoDestructor&&) = delete;

  alignas(T) char storage_[sizeof(T)];
};

}  // namespace common
}  // namespace tsdb2

#endif  //__TSDB2_COMMON_NO_DESTRUCTOR_H__
