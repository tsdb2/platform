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

// Internal implementation of `MultiMutexLock`.
template <typename... Mutexex>
class MultiMutexLockImpl;

template <>
class MultiMutexLockImpl<> {
 public:
  explicit MultiMutexLockImpl() = default;
  explicit MultiMutexLockImpl(absl::Condition const& /*condition*/) {}
  ~MultiMutexLockImpl() = default;

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

// A `MultiMutexLock` is a scoped object that can lock and unlock a set of mutexes using RAII in a
// deterministic order so as to avoid potential deadlocks. The mutexes are ordered by pointer and
// acquired in that order; upon destruction they are released in the inverse order.
//
// In the following example thread A and thread B are guaranteed to not deadlock:
//
//   absl::Mutex mutex1;
//   absl::Mutex mutex2;
//
//   {
//     // thread A
//     MultiMutexLock lock{&mutex1, &mutex2};
//   }
//
//   {
//     // thread B
//     MultiMutexLock lock{&mutex2, &mutex1};
//   }
//
// Conditional locking is supported and it's also guaranteed to not cause any deadlocks. The
// `absl::Condition` object must be the first argument:
//
//   absl::Condition condition{ /* ... */ };
//   MultiMutexLock lock{condition, &mutex1, &mutex2};
//
template <typename... Mutex>
class MultiMutexLock {
 public:
  static_assert((std::is_same_v<std::decay_t<Mutex>, absl::Mutex> && ...),
                "MultiMutexLock only works with absl::Mutex");

  explicit MultiMutexLock(Mutex* const... mutexes)
      : MultiMutexLock(Sort(mutexes...), std::make_index_sequence<sizeof...(Mutex)>()) {}

  explicit MultiMutexLock(absl::Condition const& condition, Mutex* const... mutexes)
      : MultiMutexLock(condition, Sort(mutexes...), std::make_index_sequence<sizeof...(Mutex)>()) {}

 private:
  using Array = std::array<absl::Mutex*, sizeof...(Mutex)>;

  template <
      typename... MutexAlias,
      std::enable_if_t<(std::is_same_v<std::decay_t<MutexAlias>, absl::Mutex> && ...), bool> = true>
  static Array Sort(MutexAlias* const... mutexes) {
    Array array{mutexes...};
    std::sort(array.begin(), array.end());
    return array;
  }

  template <size_t... Is>
  explicit MultiMutexLock(Array const& array, std::index_sequence<Is...> const& /*index_sequence*/)
      : impl_{array[Is]...} {}

  template <size_t... Is>
  explicit MultiMutexLock(absl::Condition const& condition, Array const& array,
                          std::index_sequence<Is...> const& /*index_sequence*/)
      : impl_{condition, array[Is]...} {}

  internal::MultiMutexLockImpl<Mutex...> impl_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_MULTI_MUTEX_LOCK_H__
