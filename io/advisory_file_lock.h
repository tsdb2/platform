#ifndef __TSDB2_IO_ADVISORY_FILE_LOCK_H__
#define __TSDB2_IO_ADVISORY_FILE_LOCK_H__

#include <fcntl.h>
#include <sys/stat.h>

#include <cstddef>
#include <tuple>
#include <utility>

#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "io/fd.h"

namespace tsdb2 {
namespace io {

// Holds an exclusive advisory record lock on a file.
//
// See the "Advisory record locking" section in the man page for `fcntl` for more information.
//
// This class is reentrant: internally it deduplicates locks created on different file descriptors
// but referring to the same inode by actually querying the inode number associated to the file
// descriptor and reference counting the number of identical locks acquired on each inode. It's
// perfectly fine to create nested locks like these:
//
//   {
//     auto const status_or_lock1 = ExclusiveFileLock::Acquire(fd);
//     {
//       auto const status_or_lock2 = ExclusiveFileLock::Acquire(fd);
//       // ...
//     }
//     // the file is still locked
//   }
//
//
// Or to create locks concurrently in different threads:
//
//   // These are OK even if fd1 and fd2 refer to the same inode.
//   // t1 and t2 don't block each other, but will be blocked by other processes that hold exclusive
//   // advisory locks on the same inode.
//   std::thread t1{[&] {
//     auto const status_or_lock = ExclusiveFileLock::Acquire(fd);
//     // ...
//   }};
//   std::thread t2{[&] {
//     auto const status_or_lock = ExclusiveFileLock::Acquire(fd);
//     // ...
//   }};
//
//
// Note that the kernel implements deadlock detection, causing `ExclusiveFileLock::Acquire` to fail
// when two or more processes try to acquire mutually conflicting locks. At least two processes are
// needed to trigger deadlock detection because trying to acquire a lock within the same process
// that holds a conflicting lock will simply update the existing lock.
//
// `ExclusiveFileLock` is movable and swappable, but not copyable. Moving and swapping is not
// thread-safe.
class ExclusiveFileLock final {
 public:
  // Acquires the lock on the range `[start, start+length)` of the file referred to by `fd`,
  // blocking and waiting if a conflicting lock is held somewhere else.
  static absl::StatusOr<ExclusiveFileLock> Acquire(FD const &fd, off_t start, off_t length);

  // Acquires the lock on all bytes of the file referred to by `fd`, blocking and waiting if a
  // conflicting lock is held somewhere else.
  static absl::StatusOr<ExclusiveFileLock> Acquire(FD const &fd) { return Acquire(fd, 0, 0); }

  // Creates an empty `ExclusiveFileLock` object. The `Release` method and the destructor of an
  // empty lock are no-ops unless another `ExclusiveFileLock` object is moved into this one.
  explicit ExclusiveFileLock() = default;

  // Moving moves ownership of the lock.
  ExclusiveFileLock(ExclusiveFileLock &&other) noexcept = default;
  ExclusiveFileLock &operator=(ExclusiveFileLock &&other) noexcept = default;

  void swap(ExclusiveFileLock &other) noexcept { internal_lock_.swap(other.internal_lock_); }
  friend void swap(ExclusiveFileLock &lhs, ExclusiveFileLock &rhs) noexcept { lhs.swap(rhs); }

  [[nodiscard]] bool empty() const { return internal_lock_.empty(); }
  explicit operator bool() const { return internal_lock_ != nullptr; }

  // Empties this `ExclusiveFileLock` object, releasing the corresponding lock. No-op if the object
  // is already empty.
  void Clear() { internal_lock_.reset(); }

  FD const &fd() const { return internal_lock_->fd(); }
  off_t start() const { return internal_lock_->start(); }
  off_t length() const { return internal_lock_->length(); }

 private:
  struct LockInfo final {
    template <typename H>
    friend H AbslHashValue(H h, LockInfo const &lock_info) {
      return H::combine(std::move(h), lock_info.inode_number, lock_info.start, lock_info.length);
    }

    auto tie() const { return std::tie(inode_number, start, length); }

    friend bool operator==(LockInfo const &lhs, LockInfo const &rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(LockInfo const &lhs, LockInfo const &rhs) {
      return lhs.tie() != rhs.tie();
    }

    ino_t inode_number;
    off_t start;
    off_t length;
  };

  class InternalLock final : public tsdb2::common::RefCounted {
   public:
    static absl::StatusOr<tsdb2::common::reffed_ptr<InternalLock>> Acquire(FD const &fd,
                                                                           off_t start,
                                                                           off_t length)
        ABSL_LOCKS_EXCLUDED(mutex_);

    explicit InternalLock(ino_t const inode_number, off_t const start, off_t const length, FD fd)
        : inode_number_(inode_number), start_(start), length_(length), fd_(std::move(fd)) {}

    template <typename H>
    friend H AbslHashValue(H h, InternalLock const &lock) {
      return H::combine(std::move(h), lock.inode_number_, lock.start_, lock.length_);
    }

    auto tie() const { return std::tie(inode_number_, start_, length_); }

    friend bool operator==(InternalLock const &lhs, InternalLock const &rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(InternalLock const &lhs, InternalLock const &rhs) {
      return lhs.tie() != rhs.tie();
    }

    FD const &fd() const { return fd_; }
    off_t start() const { return start_; }
    off_t length() const { return length_; }

   private:
    // Custom hash & equal functors to index locks by inode number and byte range in the `locks_`
    // hash table below.
    struct HashEq {
      struct Hash {
        using is_transparent = void;
        size_t operator()(InternalLock const &lock) const { return absl::HashOf(lock); }
        size_t operator()(LockInfo const &lock_info) const { return absl::HashOf(lock_info); }
      };

      struct Eq {
        using is_transparent = void;

        static auto Tie(InternalLock const &lock) { return lock.tie(); }
        static auto Tie(LockInfo const &lock_info) { return lock_info.tie(); }

        template <typename LHS, typename RHS>
        bool operator()(LHS &&lhs, RHS &&rhs) const {
          return Tie(std::forward<LHS>(lhs)) == Tie(std::forward<RHS>(rhs));
        }
      };
    };

    using LockSet = absl::node_hash_set<InternalLock, HashEq::Hash, HashEq::Eq>;

    static absl::Mutex mutex_;
    static LockSet locks_ ABSL_GUARDED_BY(mutex_);

    InternalLock(InternalLock const &) = delete;
    InternalLock &operator=(InternalLock const &) = delete;
    InternalLock(InternalLock &&) = delete;
    InternalLock &operator=(InternalLock &&) = delete;

    void OnLastUnref() ABSL_LOCKS_EXCLUDED(mutex_);

    void Release();

    ino_t const inode_number_;
    off_t const start_;
    off_t const length_;
    FD const fd_;
  };

  explicit ExclusiveFileLock(tsdb2::common::reffed_ptr<InternalLock> internal_lock)
      : internal_lock_(std::move(internal_lock)) {}

  ExclusiveFileLock(ExclusiveFileLock const &) = delete;
  ExclusiveFileLock &operator=(ExclusiveFileLock const &) = delete;

  tsdb2::common::reffed_ptr<InternalLock> internal_lock_;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_ADVISORY_FILE_LOCK_H__
