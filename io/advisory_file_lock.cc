#include "io/advisory_file_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <tuple>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"
#include "io/fd.h"

namespace tsdb2 {
namespace io {

absl::Status AdvisoryLockAcquireExclusive(FD const& fd) {
  int result;
  do {
    result = ::flock(*fd, LOCK_EX);
  } while (result < 0 && errno == EINTR);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "flock(LOCK_EX)");
  } else {
    return absl::OkStatus();
  }
}

absl::Status AdvisoryLockRelease(FD const& fd) {
  int result;
  do {
    result = ::flock(*fd, LOCK_UN);
  } while (result < 0 && errno == EINTR);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "flock(LOCK_UN)");
  } else {
    return absl::OkStatus();
  }
}

absl::StatusOr<ExclusiveFileLock> ExclusiveFileLock::Acquire(FD const& fd) {
  DEFINE_VAR_OR_RETURN(internal_lock, InternalLock::Acquire(fd));
  return ExclusiveFileLock(std::move(internal_lock));
}

ABSL_CONST_INIT absl::Mutex ExclusiveFileLock::InternalLock::mutex_{absl::kConstInit};
ExclusiveFileLock::InternalLock::LockSet ExclusiveFileLock::InternalLock::locks_;

absl::StatusOr<tsdb2::common::reffed_ptr<ExclusiveFileLock::InternalLock>>
ExclusiveFileLock::InternalLock::Acquire(FD const& fd) {
  struct stat statbuf {};
  if (::fstat(*fd, &statbuf) < 0) {
    return absl::ErrnoToStatus(errno, "fstat");
  }
  absl::MutexLock lock{&mutex_};
  auto it = locks_.find(statbuf.st_ino);
  if (it != locks_.end()) {
    return tsdb2::common::WrapReffed(const_cast<InternalLock*>(&*it));
  }
  DEFINE_VAR_OR_RETURN(fd2, fd.Clone());
  RETURN_IF_ERROR(AdvisoryLockAcquireExclusive(fd2));
  bool inserted;
  std::tie(it, inserted) = locks_.emplace(statbuf.st_ino, std::move(fd2));
  return tsdb2::common::WrapReffed(const_cast<InternalLock*>(&*it));
}

void ExclusiveFileLock::InternalLock::OnLastUnref() {
  absl::MutexLock lock{&mutex_};
  Release();
  locks_.erase(*this);
}

void ExclusiveFileLock::InternalLock::Release() {
  auto const status = AdvisoryLockRelease(fd_);
  LOG_IF(ERROR, !status.ok()) << status;
}

}  // namespace io
}  // namespace tsdb2
