#include "net/select_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "common/buffer.h"
#include "net/fd.h"

ABSL_FLAG(uint16_t, tsdb2_select_server_num_workers, 10, "Number of I/O worker threads.");

namespace tsdb2 {
namespace net {

namespace {

size_t constexpr kMaxEvents = 1024;

}  // namespace

absl::Status ListenerSocket::Accept(Callback callback) {
  absl::MutexLock lock{&mutex_};
  if (callback_) {
    return absl::FailedPreconditionError("another Accept call is already in progress");
  }
  int const result = accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (result < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      return absl::ErrnoToStatus(errno, "accept4() failed");
    } else {
      callback_.emplace(std::move(callback));
      return absl::OkStatus();
    }
  } else {
    callback(FD(result));
    return absl::OkStatus();
  }
}

void ListenerSocket::OnInput() {
  absl::MutexLock lock{&mutex_};
  if (callback_) {
    int const result = accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        (*callback_)(absl::ErrnoToStatus(errno, "accept4() failed"));
        callback_.reset();
      }
    } else {
      (*callback_)(FD(result));
      callback_.reset();
    }
  }
}

void ListenerSocket::OnOutput() {
  // Nothing to do here.
}

absl::StatusOr<ListenerSocket *> ListenerSocket::Create(std::string const &address,
                                                        uint16_t const port) {
  struct sockaddr_in6 sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(port);
  if (address.empty()) {
    sa.sin6_addr = IN6ADDR_ANY_INIT;
  } else if (inet_pton(AF_INET6, address.c_str(), &sa.sin6_addr) < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid address: \"", absl::CEscape(address), "\""));
  }
  int const fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return absl::ErrnoToStatus(errno, "socket(AF_INET6, SOCK_STREAM) failed");
  }
  int opt = 0;
  if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
    close(fd);
    return absl::ErrnoToStatus(errno, "setsockopt() failed");
  }
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    close(fd);
    return absl::ErrnoToStatus(errno, "bind() failed");
  }
  if (listen(fd, SOMAXCONN) < 0) {
    close(fd);
    return absl::ErrnoToStatus(errno, "listen() failed");
  }
  return new ListenerSocket(FD{fd});
}

absl::Status Socket::Read(size_t const length, ReadCallback callback) {
  Buffer buffer{length};
  absl::MutexLock lock{&read_mutex_};
  if (read_status_) {
    return absl::FailedPreconditionError("another read is already in progress on the same socket");
  }
  while (true) {
    size_t const offset = buffer.size();
    ssize_t const result =
        recv(*fd_, buffer.as_byte_array() + offset, length - offset, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return absl::ErrnoToStatus(errno, "recv() failed");
      } else {
        read_status_.emplace(std::move(buffer), std::move(callback));
        return absl::OkStatus();
      }
    }
    buffer.Advance(result);
    if (buffer.is_full()) {
      callback(std::move(buffer));
      return absl::OkStatus();
    }
  }
}

bool Socket::CancelRead() {
  absl::MutexLock lock{&read_mutex_};
  if (read_status_) {
    auto callback = std::move(read_status_->callback);
    read_status_.reset();
    callback(absl::CancelledError("cancelled by user"));
    return true;
  } else {
    return false;
  }
}

absl::Status Socket::Write(Buffer const &buffer, WriteCallback callback) {
  absl::MutexLock lock{&write_mutex_};
  if (write_status_) {
    return absl::FailedPreconditionError("another write is already in progress on the same socket");
  }
  size_t offset = 0;
  while (true) {
    size_t const remaining = buffer.size() - offset;
    ssize_t const result = write(*fd_, buffer.as_byte_array() + offset, remaining);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return absl::ErrnoToStatus(errno, "write() failed");
      } else {
        write_status_.emplace(buffer, remaining, std::move(callback));
        return absl::OkStatus();
      }
    } else {
      offset += result;
      if (!(offset < buffer.size())) {
        callback(absl::OkStatus());
        return absl::OkStatus();
      }
    }
  }
}

bool Socket::CancelWrite() {
  absl::MutexLock lock{&write_mutex_};
  if (write_status_) {
    auto callback = std::move(write_status_->callback);
    write_status_.reset();
    callback(absl::CancelledError("cancelled by user"));
    return true;
  } else {
    return false;
  }
}

void Socket::OnInput() {
  absl::MutexLock lock{&read_mutex_};
  while (read_status_) {
    auto &buffer = read_status_->buffer;
    size_t const offset = buffer.size();
    size_t const remaining = buffer.capacity() - offset;
    ssize_t const result = recv(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        auto callback = std::move(read_status_->callback);
        read_status_.reset();
        callback(absl::ErrnoToStatus(errno, "recv() failed"));
      } else {
        return;
      }
    } else {
      buffer.Advance(result);
      if (buffer.is_full()) {
        read_status_->callback(std::move(buffer));
        read_status_.reset();
      }
    }
  }
}

void Socket::OnOutput() {
  absl::MutexLock lock{&write_mutex_};
  while (write_status_) {
    auto const &buffer = write_status_->buffer;
    size_t const offset = buffer.size() - write_status_->remaining;
    ssize_t const result = write(*fd_, buffer.as_byte_array() + offset, write_status_->remaining);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        FinalizeWrite(absl::ErrnoToStatus(errno, "write() failed"));
      } else {
        return;
      }
    } else {
      write_status_->remaining -= result;
      if (!(offset + result < buffer.size())) {
        FinalizeWrite(absl::OkStatus());
      }
    }
  }
}

void Socket::FinalizeWrite(absl::Status const status) {
  auto callback = std::move(write_status_->callback);
  write_status_.reset();
  callback(absl::OkStatus());
}

SelectServer *SelectServer::GetInstance() {
  static SelectServer *const kInstance = new SelectServer();
  return kInstance;
}

void SelectServer::StartOrDie() {
  absl::WriterMutexLock lock{&mutex_};
  if (epoll_fd_ < 0) {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    CHECK_GE(epoll_fd_, 0) << "epoll_create1() failed, errno=" << errno;
  }
  auto const num_workers = absl::GetFlag(FLAGS_tsdb2_select_server_num_workers);
  CHECK_GT(num_workers, 0) << "SelectServer needs at least 1 worker, but " << num_workers
                           << " were specified in --tsdb2_select_server_num_workers";
  workers_.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    workers_.emplace_back(absl::bind_front(&SelectServer::WorkerLoop, this));
  }
}

int SelectServer::epoll_fd() const {
  absl::ReaderMutexLock lock{&mutex_};
  return epoll_fd_;
}

void SelectServer::WorkerLoop() {
  auto const epfd = epoll_fd();
  struct epoll_event events[kMaxEvents];
  while (true) {
    int const num_events = epoll_wait(epfd, events, kMaxEvents, 0);
    if (num_events < 0) {
      CHECK_EQ(errno, EINTR) << "epoll_wait() failed, errno=" << errno;
      continue;
    }
    for (int i = 0; i < num_events; ++i) {
      auto const socket = static_cast<BaseSocket *>(events[i].data.ptr);
      auto const revents = events[i].events;
      if ((revents & EPOLLERR) || (revents & EPOLLHUP)) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd(), nullptr);
        delete socket;
      } else if (revents & EPOLLIN) {
        socket->OnInput();
      } else if (revents & EPOLLOUT) {
        socket->OnOutput();
      }
    }
  }
}

}  // namespace net
}  // namespace tsdb2
