#include "io/advisory_file_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cstring>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"
#include "io/fd.h"

namespace tsdb2 {
namespace io {

absl::StatusOr<ExclusiveFileLock> ExclusiveFileLock::Acquire(FD const &fd, off_t const start,
                                                             off_t const length) {
  DEFINE_VAR_OR_RETURN(internal_lock, InternalLock::Acquire(fd, start, length));
  return ExclusiveFileLock(std::move(internal_lock));
}

ABSL_CONST_INIT absl::Mutex ExclusiveFileLock::InternalLock::mutex_{absl::kConstInit};
ExclusiveFileLock::InternalLock::LockSet ExclusiveFileLock::InternalLock::locks_;

absl::StatusOr<tsdb2::common::reffed_ptr<ExclusiveFileLock::InternalLock>>
ExclusiveFileLock::InternalLock::Acquire(FD const &fd, off_t const start, off_t const length) {
  struct stat statbuf;
  std::memset(&statbuf, 0, sizeof(struct stat));
  if (::fstat(*fd, &statbuf) < 0) {
    return absl::ErrnoToStatus(errno, "fstat");
  }
  absl::MutexLock lock{&mutex_};
  auto it = locks_.find(LockInfo{
      .inode_number = statbuf.st_ino,
      .start = start,
      .length = length,
  });
  if (it != locks_.end()) {
    return tsdb2::common::WrapReffed(const_cast<InternalLock *>(&*it));
  }
  DEFINE_VAR_OR_RETURN(fd2, fd.Clone());
  struct flock args;
  std::memset(&args, 0, sizeof(struct flock));
  args.l_type = F_WRLCK;
  args.l_whence = SEEK_SET;
  args.l_start = start;
  args.l_len = length;
  if (::fcntl(fd2.get(), F_SETLKW, &args) < 0) {
    return absl::ErrnoToStatus(errno, "fcntl(F_SETLKW, F_WRLCK)");
  }
  bool inserted;
  std::tie(it, inserted) = locks_.emplace(statbuf.st_ino, start, length, std::move(fd2));
  return tsdb2::common::WrapReffed(const_cast<InternalLock *>(&*it));
}

void ExclusiveFileLock::InternalLock::OnLastUnref() {
  absl::MutexLock lock{&mutex_};
  Release();
  locks_.erase(*this);
}

void ExclusiveFileLock::InternalLock::Release() {
  struct flock args;
  std::memset(&args, 0, sizeof(struct flock));
  args.l_type = F_UNLCK;
  args.l_whence = SEEK_SET;
  args.l_start = start_;
  args.l_len = length_;
  if (::fcntl(fd_.get(), F_SETLK, &args) < 0) {
    LOG(ERROR) << absl::ErrnoToStatus(errno, "fcntl(F_SETLK, F_UNLCK)");
  }
}

}  // namespace io
}  // namespace tsdb2
