#include "net/ssl_sockets.h"

#include <errno.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/no_destructor.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "net/base_sockets.h"
#include "net/epoll_server.h"
#include "net/ssl.h"

ABSL_FLAG(absl::Duration, ssl_handshake_timeout, absl::Seconds(120), "Timeout for SSL handshakes.");

namespace tsdb2 {
namespace net {

namespace {

using ::tsdb2::common::Scheduler;

std::string_view constexpr kHandshakeTimeoutMessage = "SSL handshake timeout";
std::string_view constexpr kReadTimeoutMessage = "read timeout";
std::string_view constexpr kWriteTimeoutMessage = "write timeout";

}  // namespace

ABSL_CONST_INIT absl::Mutex SSLSocket::socket_mutex_{absl::kConstInit};

SSLSocket::SocketSet SSLSocket::handshaking_sockets_;

SSLSocket::~SSLSocket() {
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

SSLSocket::SSLSocket(EpollServer* const parent, AcceptTag /*accept_tag*/, FD fd, internal::SSL ssl,
                     absl::Duration const handshake_timeout, InternalConnectCallback callback)
    : BaseSocket(parent, std::move(fd)),
      ssl_(std::move(ssl)),
      connect_state_(std::in_place, ConnectState::Mode::kAccepting, std::move(callback),
                     handshake_timeout) {}

SSLSocket::SSLSocket(EpollServer* const parent, ConnectTag /*connect_tag*/, FD fd,
                     internal::SSL ssl, absl::Duration const handshake_timeout,
                     InternalConnectCallback callback)
    : BaseSocket(parent, std::move(fd)),
      ssl_(std::move(ssl)),
      connect_state_(std::in_place, ConnectState::Mode::kConnecting, std::move(callback),
                     handshake_timeout) {}

void SSLSocket::EmplaceHandshakingSocket(tsdb2::common::reffed_ptr<SSLSocket> socket) {
  absl::MutexLock lock{&socket_mutex_};
  handshaking_sockets_.emplace(std::move(socket));
}

tsdb2::common::reffed_ptr<SSLSocket> SSLSocket::ExtractHandshakingSocket(SSLSocket* const socket) {
  absl::MutexLock lock{&socket_mutex_};
  auto node = handshaking_sockets_.extract(socket);
  return std::move(node.value());
}

SSLSocket::PendingState SSLSocket::ExpungeAllPendingState() {
  if (connect_state_) {
    MaybeCancelTimeout(&connect_state_->timeout_handle);
  }
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

absl::Status SSLSocket::AbortCallbacks(PendingState states, absl::Status status) {
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

bool SSLSocket::CloseInternal(absl::Status status) {
  bool result = false;
  PendingState states;
  {
    absl::MutexLock lock{&mutex_};
    states = ExpungeAllPendingState();
    if (fd_) {
      result = true;
      // NOTE: this is a fast shutdown, as per `SSL_shutdown` docs. Is this susceptible to
      // truncation attacks even if we always disable renegotiations & resumptions?
      // TODO: maybe we should implement the full shutdown anyway.
      SSL_shutdown(ssl_.get());
      ::shutdown(*fd_, SHUT_RDWR);
      KillSocket();
    }
  }
  AbortCallbacks(std::move(states), std::move(status)).IgnoreError();
  return result;
}

absl::StatusOr<bool> SSLSocket::Handshake() {
  auto const mode = connect_state_->mode;
  int const result =
      mode != ConnectState::Mode::kAccepting ? SSL_connect(ssl_.get()) : SSL_accept(ssl_.get());
  if (result > 0) {
    return true;
  } else if (result < 0) {
    auto const saved_errno = errno;
    int const error = SSL_get_error(ssl_.get(), result);
    char const* const handshake_function_name =
        mode != ConnectState::Mode::kAccepting ? "SSL_connect" : "SSL_accept";
    switch (error) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        return false;
      case SSL_ERROR_SYSCALL:
        if (saved_errno != 0) {
          return absl::ErrnoToStatus(saved_errno, handshake_function_name);
        } else {
          return absl::UnknownError(handshake_function_name);
        }
      case SSL_ERROR_ZERO_RETURN:
        return absl::CancelledError("SSL socket shutdown");
      default:
        TSDB2_SSL_LOG_ERRORS();
        return absl::InternalError(absl::StrCat(handshake_function_name, " protocol error"));
    }
  } else {
    return absl::CancelledError("SSL socket shutdown");
  }
}

absl::Status SSLSocket::StartHandshake() {
  absl::ReleasableMutexLock lock{&mutex_};
  if (connect_state_) {
    connect_state_->timeout_handle =
        ScheduleTimeout(connect_state_->timeout, kHandshakeTimeoutMessage);
    return ContinueHandshake(&lock);
  } else {
    return absl::OkStatus();
  }
}

absl::Status SSLSocket::ContinueHandshake(absl::ReleasableMutexLock* const lock)
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  auto status_or_ready = Handshake();
  if (status_or_ready.ok()) {
    bool const ready = status_or_ready.value();
    if (ready) {
      auto callback = std::move(connect_state_->callback);
      connect_state_.reset();
      lock->Release();
      callback(this, absl::OkStatus());
    }
    return absl::OkStatus();
  } else {
    auto state = ExpungeAllPendingState();
    KillSocket();
    lock->Release();
    return AbortCallbacks(std::move(state), std::move(status_or_ready).status());
  }
}

void SSLSocket::ScheduleRead(Buffer buffer, ReadCallback callback,
                             std::optional<absl::Duration> const timeout) {
  Scheduler::Handle timeout_handle = Scheduler::kInvalidHandle;
  if (timeout) {
    timeout_handle = ScheduleTimeout(*timeout, kReadTimeoutMessage);
  }
  read_state_.emplace(std::move(buffer), std::move(callback), timeout, timeout_handle);
}

void SSLSocket::ScheduleWrite(Buffer buffer, size_t const remaining, WriteCallback callback,
                              std::optional<absl::Duration> const timeout) {
  Scheduler::Handle timeout_handle = Scheduler::kInvalidHandle;
  if (timeout) {
    timeout_handle = ScheduleTimeout(*timeout, kWriteTimeoutMessage);
  }
  write_state_.emplace(std::move(buffer), remaining, std::move(callback), timeout, timeout_handle);
}

void SSLSocket::OnError() {
  PendingState states;
  {
    absl::MutexLock lock{&mutex_};
    states = ExpungeAllPendingState();
    KillSocket();
  }
  AbortCallbacks(std::move(states), absl::AbortedError("SSL socket shutdown")).IgnoreError();
}

void SSLSocket::OnInput() {
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    auto states = ExpungeAllPendingState();
    lock.Release();
    return AbortCallbacks(std::move(states), absl::AbortedError("this socket has been shut down"))
        .IgnoreError();
  }
  if (connect_state_) {
    return ContinueHandshake(&lock).IgnoreError();
  }
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
    int const result = SSL_read(ssl_.get(), buffer.as_byte_array() + offset, remaining);
    if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        auto state = ExpungeReadState();
        lock.Release();
        return state.callback(std::move(state.buffer));
      }
    } else {
      auto const saved_errno = errno;
      int const error = SSL_get_error(ssl_.get(), result);
      if (error == SSL_ERROR_WANT_READ) {
        if (read_state_->timeout) {
          read_state_->timeout_handle = ScheduleTimeout(*read_state_->timeout, kReadTimeoutMessage);
        }
        return;
      }
      auto state = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      switch (error) {
        case SSL_ERROR_WANT_WRITE:
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_read wants write"))
              .IgnoreError();
        case SSL_ERROR_SYSCALL:
          if (saved_errno != 0) {
            return AbortCallbacks(std::move(state), absl::ErrnoToStatus(saved_errno, "SSL_read"))
                .IgnoreError();
          } else {
            return AbortCallbacks(std::move(state), absl::UnknownError("SSL_read")).IgnoreError();
          }
        case SSL_ERROR_ZERO_RETURN:
          return AbortCallbacks(std::move(state), absl::CancelledError("SSL socket peer hung up"))
              .IgnoreError();
        default:
          TSDB2_SSL_LOG_ERRORS();
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_read protocol error"))
              .IgnoreError();
      }
    }
  }
}

void SSLSocket::OnOutput() {
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    auto states = ExpungeAllPendingState();
    lock.Release();
    return AbortCallbacks(std::move(states), absl::AbortedError("this socket has been shut down"))
        .IgnoreError();
  }
  if (connect_state_) {
    return ContinueHandshake(&lock).IgnoreError();
  }
  if (!write_state_) {
    return;
  }
  MaybeCancelTimeout(&write_state_->timeout_handle);
  while (true) {
    auto& buffer = write_state_->buffer;
    auto& remaining = write_state_->remaining;
    CHECK_LE(remaining, buffer.size());
    size_t const offset = buffer.size() - remaining;
    int const result = SSL_write(ssl_.get(), buffer.as_byte_array() + offset, remaining);
    if (result > 0) {
      CHECK_LE(result, remaining);
      remaining -= result;
      if (remaining == 0) {
        auto state = ExpungeWriteState();
        lock.Release();
        return state.callback(absl::OkStatus());
      }
    } else {
      auto const saved_errno = errno;
      int const error = SSL_get_error(ssl_.get(), result);
      if (error == SSL_ERROR_WANT_WRITE) {
        if (write_state_->timeout) {
          write_state_->timeout_handle =
              ScheduleTimeout(*write_state_->timeout, kWriteTimeoutMessage);
        }
        return;
      }
      auto state = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      switch (error) {
        case SSL_ERROR_WANT_READ:
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_write wants read"))
              .IgnoreError();
        case SSL_ERROR_SYSCALL:
          if (saved_errno != 0) {
            return AbortCallbacks(std::move(state), absl::ErrnoToStatus(saved_errno, "SSL_write"))
                .IgnoreError();
          } else {
            return AbortCallbacks(std::move(state), absl::UnknownError("SSL_write")).IgnoreError();
          }
        case SSL_ERROR_ZERO_RETURN:
          return AbortCallbacks(std::move(state), absl::CancelledError("SSL socket peer hung up"))
              .IgnoreError();
        default:
          TSDB2_SSL_LOG_ERRORS();
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_write protocol error"))
              .IgnoreError();
      }
    }
  }
}

Scheduler::Handle SSLSocket::ScheduleTimeout(absl::Duration const timeout,
                                             std::string_view const status_message) {
  auto const handle = tsdb2::common::default_scheduler->ScheduleIn(
      absl::bind_front(&SSLSocket::Timeout, this, status_message), timeout);
  active_timeouts_.emplace(handle);
  return handle;
}

bool SSLSocket::MaybeCancelTimeout(Scheduler::Handle* const handle_ptr) {
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

void SSLSocket::Timeout(std::string_view const status_message) {
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

absl::Status SSLSocket::ReadInternal(size_t length, ReadCallback callback,
                                     std::optional<absl::Duration> const timeout) {
  if (length == 0) {
    return absl::InvalidArgumentError("the number of bytes to read must be at least 1");
  }
  if (!callback) {
    return absl::InvalidArgumentError("the read callback must not be empty");
  }
  if (timeout && *timeout <= absl::ZeroDuration()) {
    return absl::InvalidArgumentError("the I/O timeout must be greater than zero");
  }
  Buffer buffer{length};
  absl::ReleasableMutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  if (connect_state_) {
    return absl::FailedPreconditionError("SSL handshake incomplete");
  }
  if (read_state_) {
    return absl::FailedPreconditionError("another read operation is already in progress");
  }
  while (true) {
    size_t const offset = buffer.size();
    CHECK_LT(offset, length);
    int const result = SSL_read(ssl_.get(), buffer.as_byte_array() + offset, length - offset);
    if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        lock.Release();
        callback(std::move(buffer));
        return absl::OkStatus();
      }
    } else {
      auto const saved_errno = errno;
      int const error = SSL_get_error(ssl_.get(), result);
      if (error == SSL_ERROR_WANT_READ) {
        ScheduleRead(std::move(buffer), std::move(callback), timeout);
        return absl::OkStatus();
      }
      auto state = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      switch (error) {
        case SSL_ERROR_WANT_WRITE:
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_read wants write"));
        case SSL_ERROR_ZERO_RETURN:
          return AbortCallbacks(std::move(state), absl::CancelledError("SSL socket peer hung up"));
        case SSL_ERROR_SYSCALL:
          if (saved_errno != 0) {
            return AbortCallbacks(std::move(state), absl::ErrnoToStatus(saved_errno, "SSL_read"));
          } else {
            return AbortCallbacks(std::move(state), absl::UnknownError("SSL_read"));
          }
        default:
          TSDB2_SSL_LOG_ERRORS();
          return AbortCallbacks(std::move(state), absl::AbortedError("SSL read protocol error"));
      }
    }
  }
}

absl::Status SSLSocket::WriteInternal(Buffer buffer, WriteCallback callback,
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
  if (connect_state_) {
    return absl::FailedPreconditionError("SSL handshake incomplete");
  }
  if (write_state_) {
    return absl::FailedPreconditionError("another write operation is already in progress");
  }
  size_t offset = 0;
  while (true) {
    CHECK_LT(offset, buffer.size());
    size_t const remaining = buffer.size() - offset;
    int const result = SSL_write(ssl_.get(), buffer.as_byte_array() + offset, remaining);
    if (result > 0) {
      offset += result;
      if (!(offset < buffer.size())) {
        lock.Release();
        callback(absl::OkStatus());
        return absl::OkStatus();
      }
    } else {
      auto const saved_errno = errno;
      int const error = SSL_get_error(ssl_.get(), result);
      if (error == SSL_ERROR_WANT_WRITE) {
        ScheduleWrite(std::move(buffer), remaining, std::move(callback), timeout);
        return absl::OkStatus();
      }
      auto state = ExpungeAllPendingState();
      KillSocket();
      lock.Release();
      switch (error) {
        case SSL_ERROR_WANT_READ:
          return AbortCallbacks(std::move(state), absl::InternalError("SSL_write wants read"));
        case SSL_ERROR_ZERO_RETURN:
          return AbortCallbacks(std::move(state), absl::CancelledError("SSL socket peer hung up"));
        case SSL_ERROR_SYSCALL:
          if (saved_errno != 0) {
            return AbortCallbacks(std::move(state), absl::ErrnoToStatus(saved_errno, "SSL_write"));
          } else {
            return AbortCallbacks(std::move(state), absl::UnknownError("SSL_write"));
          }
        default:
          TSDB2_SSL_LOG_ERRORS();
          return AbortCallbacks(std::move(state), absl::AbortedError("SSL write protocol error"));
      }
    }
  }
}

tsdb2::common::NoDestructor<SSLSocketModule> SSLSocketModule::instance_;

}  // namespace net
}  // namespace tsdb2
