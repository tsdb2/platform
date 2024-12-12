#include "net/sockets.h"

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/scheduler.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace net {

namespace {

using ::tsdb2::common::Scheduler;

std::string_view constexpr kReadTimeoutMessage = "read timeout";
std::string_view constexpr kWriteTimeoutMessage = "write timeout";

}  // namespace

Socket::~Socket() {
  TimeoutSet timeouts;
  {
    absl::MutexLock lock{&mutex_};
    std::swap(timeouts, active_timeouts_);
  }
  for (auto const handle : timeouts) {
    tsdb2::common::default_scheduler->Cancel(handle);
  }
  for (auto const handle : timeouts) {
    tsdb2::common::default_scheduler->BlockingCancel(handle);
  }
}

Socket::PendingState Socket::ExpungeAllPendingState() {
  if (read_state_) {
    MaybeCancelTimeout(&read_state_->timeout_handle);
  }
  if (write_state_) {
    MaybeCancelTimeout(&write_state_->timeout_handle);
  }
  PendingState states =
      std::make_tuple(std::move(connect_state_), std::move(read_state_), std::move(write_state_));
  connect_state_ = std::nullopt;
  read_state_ = std::nullopt;
  write_state_ = std::nullopt;
  return states;
}

absl::Status Socket::AbortCallbacks(PendingState states, absl::Status status) {
  auto& [connect_state, read_state, write_state] = states;
  ReadCallback read_callback;
  if (read_state) {
    read_callback = std::move(read_state->callback);
    read_state.reset();
  }
  WriteCallback write_callback;
  if (write_state) {
    write_callback = std::move(write_state->callback);
    write_state.reset();
  }
  if (connect_state) {
    connect_state->callback(this, status);
  }
  if (read_callback) {
    read_callback(status);
  }
  if (write_callback) {
    write_callback(status);
  }
  return status;
}

bool Socket::CloseInternal(absl::Status status) {
  bool result = false;
  PendingState states;
  {
    absl::MutexLock lock{&mutex_};
    states = ExpungeAllPendingState();
    if (fd_) {
      result = true;
      ::shutdown(*fd_, SHUT_RDWR);
      KillSocket();
    }
  }
  AbortCallbacks(std::move(states), std::move(status)).IgnoreError();
  return result;
}

void Socket::MaybeFinalizeConnect() {
  if (!connect_state_) {
    return;
  }
  int error = 0;
  socklen_t size = sizeof(error);
  if (::getsockopt(*fd_, SOL_SOCKET, SO_ERROR, &error, &size) < 0) {
    connect_state_->callback(this, absl::UnknownError("connect() failed"));
  } else if (error != 0) {
    connect_state_->callback(this, absl::ErrnoToStatus(error, "connect()"));
  } else {
    connect_state_->callback(this, absl::OkStatus());
  }
  connect_state_.reset();
}

void Socket::OnError() {
  PendingState states;
  {
    absl::MutexLock lock{&mutex_};
    states = ExpungeAllPendingState();
    KillSocket();
  }
  AbortCallbacks(std::move(states), absl::AbortedError("socket shutdown")).IgnoreError();
}

void Socket::OnInput() {
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    auto states = ExpungeAllPendingState();
    lock.Release();
    return AbortCallbacks(std::move(states), absl::AbortedError("this socket has been shut down"))
        .IgnoreError();
  }
  MaybeFinalizeConnect();
  if (!read_state_) {
    return;
  }
  MaybeCancelTimeout(&read_state_->timeout_handle);
  while (true) {
    auto& buffer = read_state_->buffer;
    size_t const offset = buffer.size();
    CHECK_LT(offset, buffer.capacity());
    size_t const remaining = buffer.capacity() - offset;
    CHECK_GT(remaining, 0);
    ssize_t const result = ::recv(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        auto status = absl::ErrnoToStatus(errno, "recv");
        auto states = ExpungeAllPendingState();
        KillSocket();
        lock.Release();
        return AbortCallbacks(std::move(states), std::move(status)).IgnoreError();
      } else {
        if (read_state_->timeout) {
          read_state_->timeout_handle = ScheduleTimeout(*read_state_->timeout, kReadTimeoutMessage);
        }
        return;
      }
    } else if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        auto state = ExpungeReadState();
        lock.Release();
        return state.callback(std::move(state.buffer));
      }
    } else {
      auto states = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      return AbortCallbacks(std::move(states), absl::AbortedError("the peer hung up"))
          .IgnoreError();
    }
  }
}

void Socket::OnOutput() {
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    auto states = ExpungeAllPendingState();
    lock.Release();
    return AbortCallbacks(std::move(states), absl::AbortedError("this socket has been shut down"))
        .IgnoreError();
  }
  MaybeFinalizeConnect();
  if (!write_state_) {
    return;
  }
  MaybeCancelTimeout(&write_state_->timeout_handle);
  while (true) {
    auto& buffer = write_state_->buffer;
    auto& remaining = write_state_->remaining;
    CHECK_LE(remaining, buffer.size());
    size_t const offset = buffer.size() - remaining;
    ssize_t const result = ::send(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        auto status = absl::ErrnoToStatus(errno, "send");
        auto states = ExpungeAllPendingState();
        KillSocket();
        lock.Release();
        return AbortCallbacks(std::move(states), std::move(status)).IgnoreError();
      } else {
        if (write_state_->timeout) {
          write_state_->timeout_handle =
              ScheduleTimeout(*write_state_->timeout, kWriteTimeoutMessage);
        }
        return;
      }
    } else if (result > 0) {
      CHECK_LE(result, remaining);
      remaining -= result;
      if (remaining == 0) {
        auto state = ExpungeWriteState();
        lock.Release();
        return state.callback(absl::OkStatus());
      }
    } else {
      auto states = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      return AbortCallbacks(std::move(states), absl::AbortedError("the peer hung up"))
          .IgnoreError();
    }
  }
}

Scheduler::Handle Socket::ScheduleTimeout(absl::Duration const timeout,
                                          std::string_view const status_message) {
  auto const handle = tsdb2::common::default_scheduler->ScheduleIn(
      absl::bind_front(&Socket::Timeout, this, status_message), timeout);
  active_timeouts_.emplace(handle);
  return handle;
}

bool Socket::MaybeCancelTimeout(Scheduler::Handle* const handle_ptr) {
  auto& handle = *handle_ptr;
  if (handle != Scheduler::kInvalidHandle) {
    active_timeouts_.erase(handle);
    tsdb2::common::default_scheduler->Cancel(handle);
    handle = Scheduler::kInvalidHandle;
    return true;
  } else {
    return false;
  }
}

void Socket::Timeout(std::string_view const status_message) {
  absl::ReleasableMutexLock lock{&mutex_};
  auto const it = active_timeouts_.find(Scheduler::current_task_handle());
  if (it == active_timeouts_.end()) {
    return;
  }
  active_timeouts_.erase(it);
  auto state = ExpungeAllPendingState();
  ::shutdown(*fd_, SHUT_RDWR);
  KillSocket();
  lock.Release();
  AbortCallbacks(std::move(state), absl::DeadlineExceededError(status_message)).IgnoreError();
}

absl::Status Socket::ReadInternal(size_t const length, ReadCallback callback,
                                  std::optional<absl::Duration> const timeout) {
  if (length == 0) {
    return absl::InvalidArgumentError("the number of bytes to read must be at least 1");
  }
  if (!callback) {
    return absl::InvalidArgumentError("socket I/O callbacks must not be empty");
  }
  if (timeout && *timeout <= absl::ZeroDuration()) {
    return absl::InvalidArgumentError("the I/O timeout must be greater than zero");
  }
  Buffer buffer{length};
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  if (read_state_) {
    return absl::FailedPreconditionError("another read operation is already in progress");
  }
  while (true) {
    size_t const offset = buffer.size();
    CHECK_GT(length, offset);
    ssize_t const result =
        ::recv(*fd_, buffer.as_byte_array() + offset, length - offset, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        auto status = absl::ErrnoToStatus(errno, "recv");
        auto states = ExpungeAllPendingState();
        KillSocket();
        lock.Release();
        return AbortCallbacks(std::move(states), std::move(status));
      } else {
        Scheduler::Handle timeout_handle = Scheduler::kInvalidHandle;
        if (timeout) {
          timeout_handle = ScheduleTimeout(*timeout, kReadTimeoutMessage);
        }
        read_state_.emplace(std::move(buffer), std::move(callback), timeout, timeout_handle);
        return absl::OkStatus();
      }
    } else if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        lock.Release();
        callback(std::move(buffer));
        return absl::OkStatus();
      }
    } else {
      auto states = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      return AbortCallbacks(std::move(states), absl::AbortedError("the peer hung up"));
    }
  }
}

absl::Status Socket::WriteInternal(Buffer buffer, WriteCallback callback,
                                   std::optional<absl::Duration> const timeout) {
  if (buffer.empty()) {
    return absl::InvalidArgumentError("the number of bytes to write must be at least 1");
  }
  if (!callback) {
    return absl::InvalidArgumentError("socket I/O callbacks must not be empty.");
  }
  if (timeout && *timeout <= absl::ZeroDuration()) {
    return absl::InvalidArgumentError("the I/O timeout must be greater than zero");
  }
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  if (write_state_) {
    return absl::FailedPreconditionError("another write operation is already in progress");
  }
  size_t offset = 0;
  while (true) {
    CHECK_LT(offset, buffer.size());
    size_t const remaining = buffer.size() - offset;
    ssize_t const result = ::send(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        auto status = absl::ErrnoToStatus(errno, "send");
        auto states = ExpungeAllPendingState();
        KillSocket();
        lock.Release();
        return AbortCallbacks(std::move(states), std::move(status));
      } else {
        Scheduler::Handle timeout_handle = Scheduler::kInvalidHandle;
        if (timeout) {
          timeout_handle = ScheduleTimeout(*timeout, kWriteTimeoutMessage);
        }
        write_state_.emplace(std::move(buffer), remaining, std::move(callback), timeout,
                             timeout_handle);
        return absl::OkStatus();
      }
    } else if (result > 0) {
      offset += result;
      if (!(offset < buffer.size())) {
        lock.Release();
        callback(absl::OkStatus());
        return absl::OkStatus();
      }
    } else {
      auto states = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      return AbortCallbacks(std::move(states), absl::AbortedError("the peer hung up"));
    }
  }
}

}  // namespace net
}  // namespace tsdb2
