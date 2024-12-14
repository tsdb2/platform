#ifndef __TSDB2_IO_FD_H__
#define __TSDB2_IO_FD_H__

#include <errno.h>
#include <unistd.h>

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace tsdb2 {
namespace io {

// Manages a Unix file descriptor, closing it automatically upon destruction.
//
// FD is movable but not copyable. Moving transfers ownership of the wrapped file descriptor number
// to another instance, which becomes responsible for closing it.
//
// FD is hashable and comparable, so it's suitable for use in most containers.
class FD {
 public:
  // Creates an empty FD object that doesn't wrap any file descriptor.
  explicit FD() : fd_(-1) {}

  // Creates an FD object wrapping the provided file descriptor.
  explicit FD(int const fd) : fd_(fd) {}

  // Closes the wrapped file descriptor, if present.
  ~FD() { MaybeClose(); }

  // Moving transfers ownership of the wrapped file descriptor number.

  FD(FD&& other) noexcept : fd_(other.Release()) {}

  FD& operator=(FD&& other) noexcept {
    if (this != &other) {
      MaybeClose();
      fd_ = other.Release();
    }
    return *this;
  }

  void swap(FD& other) noexcept { std::swap(fd_, other.fd_); }
  friend void swap(FD& lhs, FD& rhs) noexcept { lhs.swap(rhs); }

  template <typename H>
  friend H AbslHashValue(H h, FD const& fd) {
    return H::combine(std::move(h), fd.fd_);
  }

  friend bool operator==(FD const& lhs, FD const& rhs) { return lhs.fd_ == rhs.fd_; }
  friend bool operator!=(FD const& lhs, FD const& rhs) { return lhs.fd_ != rhs.fd_; }
  friend bool operator<(FD const& lhs, FD const& rhs) { return lhs.fd_ < rhs.fd_; }
  friend bool operator<=(FD const& lhs, FD const& rhs) { return lhs.fd_ <= rhs.fd_; }
  friend bool operator>(FD const& lhs, FD const& rhs) { return lhs.fd_ > rhs.fd_; }
  friend bool operator>=(FD const& lhs, FD const& rhs) { return lhs.fd_ >= rhs.fd_; }

  // Indicates whether this object wraps a file descriptor.
  [[nodiscard]] bool empty() const { return fd_ < 0; }
  explicit operator bool() const { return fd_ >= 0; }

  // Returns the wrapped file descriptor number. Undefined behavior if the FD is empty.
  int get() const { return fd_; }
  int operator*() const { return fd_; }

  // Closes the wrapped file descriptor and empties this `FD` object. Does nothing if the `FD` is
  // already empty.
  void Close() {
    MaybeClose();
    fd_ = -1;
  }

  // Releases ownership of the wrapped file descriptor number. The caller receives the number and
  // becomes responsible for closing it.
  //
  // Undefined behavior when empty.
  int Release() {
    int const fd = fd_;
    fd_ = -1;
    return fd;
  }

  absl::StatusOr<FD> Clone() const {
    if (fd_ < 0) {
      return absl::FailedPreconditionError("cannot clone an empty file descriptor");
    }
    int const fd2 = ::dup(fd_);
    if (fd2 < 0) {
      return absl::ErrnoToStatus(errno, "dup");
    }
    return FD(fd2);
  }

 private:
  FD(FD const&) = delete;
  FD& operator=(FD const&) = delete;

  void MaybeClose() const {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  int fd_;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_FD_H__
