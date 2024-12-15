#ifndef __TSDB2_IO_ADVISORY_FILE_LOCK_H__
#define __TSDB2_IO_ADVISORY_FILE_LOCK_H__

#include <sys/types.h>

#include <cstddef>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "io/fd.h"

namespace tsdb2 {
namespace io {

// Low-level routine to acquire an exclusive advisory lock using `flock`.
absl::Status AdvisoryLockAcquireExclusive(FD const& fd);

// Low-level routine to release any advisory locks held on a file using `flock`.
absl::Status AdvisoryLockRelease(FD const& fd);

// Holds an exclusive advisory lock on a file.
//
// Under the hood the lock is managed via the `flock` syscall.
//
// This class is reentrant: internally it deduplicates locks created on different file descriptors
// referring to the same inode by actually querying the inode number associated to the file
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
// The reentrancy implemented by `ExclusiveFileLock` changes the advisory semantics slightly: raw
// `flock`s are associated to a file description, but `ExclusiveFileLock` deduplicates them based on
// the inode number making it possible for a process to stack many locks even if they're associated
// to different file descriptions, e.g. if they were acquired on two file descriptors resulting from
// two different `open` calls. The following example does not block and stacks two locks on the same
// file correctly:
//
//   {
//     FD fd1{::open("/tmp/foo", O_RDWR, 0664)};
//     ExclusiveFileLock lock1{fd1};
//     {
//       FD fd2{::open("/tmp/foo", O_RDWR, 0664)};
//       ExclusiveFileLock lock2{fd2};
//       // `lock2` didn't block despite different file descriptions
//     }
//     // `lock1` is still locked
//   }
//
//
// Note that the kernel implements deadlock detection, causing `ExclusiveFileLock::Acquire` to fail
// when two or more processes try to acquire mutually conflicting locks. At least two processes are
// needed to trigger deadlock detection because `ExclusiveFileLock` is reentrant within each
// process.
//
// `ExclusiveFileLock` is movable and swappable, but not copyable. Moving and swapping are not
// thread-safe.
class ExclusiveFileLock {
 public:
  // Acquires the lock on all bytes of the file referred to by `fd`, blocking and waiting if a
  // conflicting lock is held somewhere else.
  static absl::StatusOr<ExclusiveFileLock> Acquire(FD const& fd);

  // Creates an empty `ExclusiveFileLock` object. The `Release` method and the destructor of an
  // empty lock are no-ops unless another `ExclusiveFileLock` object is moved into this one.
  explicit ExclusiveFileLock() = default;

  // Releases the lock if held.
  ~ExclusiveFileLock() = default;

  // Moving moves ownership of the lock.
  ExclusiveFileLock(ExclusiveFileLock&& other) noexcept = default;
  ExclusiveFileLock& operator=(ExclusiveFileLock&& other) noexcept = default;

  void swap(ExclusiveFileLock& other) noexcept { internal_lock_.swap(other.internal_lock_); }
  friend void swap(ExclusiveFileLock& lhs, ExclusiveFileLock& rhs) noexcept { lhs.swap(rhs); }

  [[nodiscard]] bool empty() const { return internal_lock_.empty(); }
  explicit operator bool() const { return internal_lock_ != nullptr; }

  // Empties this `ExclusiveFileLock` object, releasing the corresponding lock. No-op if the object
  // is already empty.
  void Clear() { internal_lock_.reset(); }

  FD const& fd() const { return internal_lock_->fd(); }

 private:
  class InternalLock final : public tsdb2::common::RefCounted {
   public:
    static absl::StatusOr<tsdb2::common::reffed_ptr<InternalLock>> Acquire(FD const& fd);

    explicit InternalLock(ino_t const inode_number, FD fd)
        : inode_number_(inode_number), fd_(std::move(fd)) {}

    ~InternalLock() override = default;

    FD const& fd() const { return fd_; }

   private:
    struct Hash {
      using is_transparent = void;
      size_t operator()(InternalLock const& lock) const { return absl::HashOf(lock.inode_number_); }
      size_t operator()(ino_t const inode_number) const { return absl::HashOf(inode_number); }
    };

    struct Equal {
      using is_transparent = void;

      static ino_t ToInodeNumber(InternalLock const& lock) { return lock.inode_number_; }
      static ino_t ToInodeNumber(ino_t const inode_number) { return inode_number; }

      template <typename LHS, typename RHS>
      bool operator()(LHS&& lhs, RHS&& rhs) const {
        return ToInodeNumber(std::forward<LHS>(lhs)) == ToInodeNumber(std::forward<RHS>(rhs));
      }
    };

    using LockSet = absl::node_hash_set<InternalLock, Hash, Equal>;

    static absl::Mutex mutex_;
    static LockSet locks_ ABSL_GUARDED_BY(mutex_);

    InternalLock(InternalLock const&) = delete;
    InternalLock& operator=(InternalLock const&) = delete;
    InternalLock(InternalLock&&) = delete;
    InternalLock& operator=(InternalLock&&) = delete;

    void OnLastUnref() override;

    void Release();

    ino_t const inode_number_;
    FD const fd_;
  };

  explicit ExclusiveFileLock(tsdb2::common::reffed_ptr<InternalLock> internal_lock)
      : internal_lock_(std::move(internal_lock)) {}

  ExclusiveFileLock(ExclusiveFileLock const&) = delete;
  ExclusiveFileLock& operator=(ExclusiveFileLock const&) = delete;

  tsdb2::common::reffed_ptr<InternalLock> internal_lock_;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_ADVISORY_FILE_LOCK_H__
