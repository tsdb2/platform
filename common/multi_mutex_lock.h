#ifndef __TSDB2_COMMON_MULTI_MUTEX_LOCK_H__
#define __TSDB2_COMMON_MULTI_MUTEX_LOCK_H__

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

namespace internal {

template <typename... Mutexex>
class MultiMutexLockImpl;

template <>
class MultiMutexLockImpl<> {
 public:
  explicit MultiMutexLockImpl() = default;
  explicit MultiMutexLockImpl(absl::Condition const&) {}

 private:
  MultiMutexLockImpl(MultiMutexLockImpl const&) = delete;
  MultiMutexLockImpl& operator=(MultiMutexLockImpl const&) = delete;
  MultiMutexLockImpl(MultiMutexLockImpl&&) = delete;
  MultiMutexLockImpl& operator=(MultiMutexLockImpl&&) = delete;
};

template <typename First, typename... Rest>
class ABSL_SCOPED_LOCKABLE MultiMutexLockImpl<First, Rest...> : public MultiMutexLockImpl<Rest...> {
 public:
  explicit MultiMutexLockImpl(First* const first_mutex, Rest* const... other_mutexes)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(first_mutex)
      : MultiMutexLockImpl<Rest...>(other_mutexes...), mutex_(first_mutex) {
    mutex_->WriterLock();
  }

  explicit MultiMutexLockImpl(absl::Condition const& condition, First* const first_mutex,
                              Rest* const... other_mutexes)
      ABSL_EXCLUSIVE_LOCK_FUNCTION(first_mutex)
      : MultiMutexLockImpl<Rest...>(condition, other_mutexes...), mutex_(first_mutex) {
    mutex_->WriterLockWhen(condition);
  }

  ~MultiMutexLockImpl() ABSL_UNLOCK_FUNCTION(mutex_) { mutex_->WriterUnlock(); }

 private:
  MultiMutexLockImpl(MultiMutexLockImpl const&) = delete;
  MultiMutexLockImpl& operator=(MultiMutexLockImpl const&) = delete;
  MultiMutexLockImpl(MultiMutexLockImpl&&) = delete;
  MultiMutexLockImpl& operator=(MultiMutexLockImpl&&) = delete;

  First* const mutex_;
};

}  // namespace internal

template <typename... Mutex>
class MultiMutexLock : private internal::MultiMutexLockImpl<Mutex...> {
 public:
  explicit MultiMutexLock(Mutex* const... mutexes)
      : MultiMutexLock(Sort(mutexes...), std::make_index_sequence<sizeof...(Mutex)>()) {}

  explicit MultiMutexLock(absl::Condition const& condition, Mutex* const... mutexes)
      : MultiMutexLock(condition, Sort(mutexes...), std::make_index_sequence<sizeof...(Mutex)>()) {}

 private:
  using Impl = internal::MultiMutexLockImpl<Mutex...>;
  using Array = std::array<absl::Mutex*, sizeof...(Mutex)>;

  template <
      std::enable_if_t<(std::is_same_v<std::decay_t<Mutex>, absl::Mutex> && ...), bool> = true>
  static Array Sort(Mutex* const... mutexes) {
    Array array{mutexes...};
    std::sort(array.begin(), array.end());
    return array;
  }

  template <size_t... Is>
  explicit MultiMutexLock(Array const& array, std::index_sequence<Is...> const&)
      : Impl{array[Is]...} {}

  template <size_t... Is>
  explicit MultiMutexLock(absl::Condition const& condition, Array const& array,
                          std::index_sequence<Is...> const&)
      : Impl{condition, array[Is]...} {}
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_MULTI_MUTEX_LOCK_H__
