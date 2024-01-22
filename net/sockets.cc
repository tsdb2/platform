#include "net/sockets.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
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

ABSL_FLAG(uint16_t, select_server_num_workers, 10, "Number of I/O worker threads.");

namespace tsdb2 {
namespace net {

namespace {

size_t constexpr kMaxEvents = 1024;

using ::tsdb2::common::reffed_ptr;

}  // namespace

void BaseSocket::RemoveFromEpoll() { parent_->DisableSocket(*this); }

void BaseSocket::OnLastUnref() {
  std::shared_ptr<BaseSocket> socket;
  absl::MutexLock lock{&mutex_};
  if (fd_) {
    shutdown(*fd_, SHUT_RDWR);
  }
  socket = parent_->RemoveSocket(*this);
}

absl::Status Socket::Read(size_t const length, ReadCallback callback) {
  if (!callback) {
    return absl::InvalidArgumentError("the read callback must not be empty");
  }
  Buffer buffer{length};
  absl::MutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
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
  absl::MutexLock lock{&mutex_};
  if (read_status_) {
    AbortRead(absl::CancelledError("cancelled by the user"));
    return true;
  } else {
    return false;
  }
}

absl::Status Socket::Write(Buffer buffer, WriteCallback callback) {
  if (!callback) {
    return absl::InvalidArgumentError("the write callback must not be empty");
  }
  absl::MutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  if (write_status_) {
    return absl::FailedPreconditionError("another write is already in progress on the same socket");
  }
  size_t offset = 0;
  while (true) {
    size_t const remaining = buffer.size() - offset;
    ssize_t const result = send(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return absl::ErrnoToStatus(errno, "send() failed");
      } else {
        write_status_.emplace(std::move(buffer), remaining, std::move(callback));
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
  absl::MutexLock lock{&mutex_};
  if (write_status_) {
    FinalizeWrite(absl::CancelledError("cancelled by the user"));
    return true;
  } else {
    return false;
  }
}

void Socket::OnError() {
  auto const status = absl::AbortedError("socket shutdown");
  absl::MutexLock lock{&mutex_};
  fd_.Close();
  if (connect_status_) {
    connect_status_->callback(status);
    connect_status_.reset();
  }
  if (write_status_) {
    FinalizeWrite(status);
  }
  if (read_status_) {
    AbortRead(status);
  }
}

void Socket::OnInput() {
  absl::MutexLock lock{&mutex_};
  MaybeFinalizeConnect();
  while (fd_ && read_status_) {
    auto& buffer = read_status_->buffer;
    size_t const offset = buffer.size();
    size_t const remaining = buffer.capacity() - offset;
    ssize_t const result = recv(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        AbortRead(absl::ErrnoToStatus(errno, "recv() failed"));
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
  absl::MutexLock lock{&mutex_};
  MaybeFinalizeConnect();
  while (fd_ && write_status_) {
    auto const& buffer = write_status_->buffer;
    size_t const offset = buffer.size() - write_status_->remaining;
    ssize_t const result =
        send(*fd_, buffer.as_byte_array() + offset, write_status_->remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        FinalizeWrite(absl::ErrnoToStatus(errno, "send() failed"));
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

absl::StatusOr<std::unique_ptr<Socket>> Socket::Create(SelectServer* const parent,
                                                       InetSocketTag const&,
                                                       std::string const& address,
                                                       uint16_t const port,
                                                       ConnectCallback callback) {
  auto const port_string = absl::StrCat(port);
  struct addrinfo* rai;
  if (getaddrinfo(address.c_str(), port_string.c_str(), nullptr, &rai) < 0) {
    return absl::ErrnoToStatus(errno, absl::StrCat("getaddrinfo(\"", absl::CEscape(address), "\", ",
                                                   port_string, ") failed"));
  }
  struct addrinfo* ai = rai;
  while (ai && ai->ai_socktype != SOCK_STREAM) {
    ai = ai->ai_next;
  }
  if (!ai) {
    freeaddrinfo(rai);
    return absl::NotFoundError(absl::StrCat("getaddrinfo(\"", absl::CEscape(address), "\", ",
                                            port_string,
                                            ") didn't return any SOCK_STREAM addresses"));
  }
  int const socket_result =
      socket(ai->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
  if (socket_result < 0) {
    freeaddrinfo(rai);
    return absl::ErrnoToStatus(errno, "socket() failed");
  }
  FD fd{socket_result};
  int const connect_result = connect(*fd, ai->ai_addr, ai->ai_addrlen);
  freeaddrinfo(rai);
  if (connect_result < 0) {
    if (errno != EINPROGRESS) {
      return absl::ErrnoToStatus(errno, "connect() failed");
    } else {
      return std::unique_ptr<Socket>(
          new Socket(parent, kConnectingTag, std::move(fd), std::move(callback)));
    }
  } else {
    return std::unique_ptr<Socket>(new Socket(parent, kConnectedTag, std::move(fd)));
  }
}

void Socket::MaybeFinalizeConnect() {
  if (!connect_status_) {
    return;
  }
  if (!fd_) {
    connect_status_->callback(absl::AbortedError("socket shutdown"));
    return;
  }
  int error = 0;
  socklen_t size = sizeof(error);
  if (getsockopt(*fd_, SOL_SOCKET, SO_ERROR, &error, &size) < 0) {
    connect_status_->callback(
        absl::ErrnoToStatus(errno, "getsockopt(SOL_SOCKET, SO_ERROR) failed"));
  }
  if (error) {
    connect_status_->callback(absl::ErrnoToStatus(error, "connect() failed"));
  } else {
    connect_status_->callback(absl::OkStatus());
  }
  connect_status_.reset();
}

void Socket::AbortRead(absl::Status status) {
  auto callback = std::move(read_status_->callback);
  read_status_.reset();
  callback(status);
}

void Socket::FinalizeWrite(absl::Status status) {
  auto callback = std::move(write_status_->callback);
  write_status_.reset();
  callback(status);
}

void ListenerSocket::OnError() {
  absl::MutexLock lock{&mutex_};
  fd_.Close();
  callback_(absl::AbortedError("socket shutdown"));
}

void ListenerSocket::OnInput() {
  auto status_or_fds = AcceptAll();
  if (status_or_fds.ok()) {
    for (auto& fd : status_or_fds.value()) {
      callback_(parent_->CreateSocket<Socket>(std::move(fd)));
    }
  } else {
    callback_(status_or_fds.status());
  }
}

void ListenerSocket::OnOutput() {
  // Nothing to do here.
}

absl::StatusOr<std::unique_ptr<ListenerSocket>> ListenerSocket::Create(
    SelectServer* const parent, InetSocketTag const& tag, std::string_view const address,
    uint16_t const port, AcceptCallback callback) {
  if (!callback) {
    return absl::InvalidArgumentError("the accept callback must not be empty");
  }
  struct sockaddr_in6 sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(port);
  if (address.empty()) {
    sa.sin6_addr = IN6ADDR_ANY_INIT;
  } else {
    std::string const address_string{address};
    if (inet_pton(AF_INET6, address_string.c_str(), &sa.sin6_addr) < 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid address: \"", absl::CEscape(address), "\""));
    }
  }
  int const result = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "socket(AF_INET6, SOCK_STREAM) failed");
  }
  FD fd{result};
  int opt = 0;
  if (setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
    return absl::ErrnoToStatus(errno, "setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, 0) failed");
  }
  if (bind(*fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    return absl::ErrnoToStatus(errno, "bind() failed");
  }
  if (listen(*fd, SOMAXCONN) < 0) {
    return absl::ErrnoToStatus(errno, "listen() failed");
  }
  return std::unique_ptr<ListenerSocket>(
      new ListenerSocket(parent, tag, address, port, std::move(fd), std::move(callback)));
}

absl::StatusOr<std::vector<FD>> ListenerSocket::AcceptAll() {
  std::vector<FD> fds;
  absl::MutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  while (true) {
    int const result = accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        RemoveFromEpoll();
        fd_.Close();
        return absl::ErrnoToStatus(errno, "accept4() failed");
      } else {
        return fds;
      }
    } else {
      fds.emplace_back(result);
    }
  }
}

SelectServer* SelectServer::GetInstance() {
  static SelectServer* const kInstance = CreateInstance();
  return kInstance;
}

void SelectServer::StartOrDie() {
  if (epoll_fd_ < 0) {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    CHECK_GE(epoll_fd_, 0) << "epoll_create1() failed, errno=" << errno;
  }
  auto const num_workers = absl::GetFlag(FLAGS_select_server_num_workers);
  CHECK_GT(num_workers, 0) << "SelectServer needs at least 1 worker, but " << num_workers
                           << " were specified in --select_server_num_workers";
  workers_.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    workers_.emplace_back(absl::bind_front(&SelectServer::WorkerLoop, this));
  }
}

SelectServer* SelectServer::CreateInstance() {
  auto const instance = new SelectServer();
  instance->StartOrDie();
  return instance;
}

reffed_ptr<BaseSocket> SelectServer::LookupSocket(int const fd) {
  absl::MutexLock lock{&mutex_};
  auto const it = sockets_.find(fd);
  if (it != sockets_.end()) {
    return reffed_ptr<BaseSocket>(it->get());
  } else {
    return nullptr;
  }
}

void SelectServer::DisableSocket(BaseSocket const& socket) {
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket.initial_fd(), nullptr);
}

std::shared_ptr<BaseSocket> SelectServer::RemoveSocket(BaseSocket const& socket) {
  absl::MutexLock lock{&mutex_};
  return sockets_.extract(&socket).value();
}

void SelectServer::WorkerLoop() {
  struct epoll_event events[kMaxEvents];
  while (true) {
    int const num_events = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
    if (num_events < 0) {
      CHECK_EQ(errno, EINTR) << absl::ErrnoToStatus(errno, "epoll_wait() failed");
      continue;
    }
    for (int i = 0; i < num_events; ++i) {
      auto const socket = LookupSocket(events[i].data.fd);
      if (!socket) {
        continue;
      }
      if (events[i].events & (EPOLLERR | EPOLLHUP)) {
        DisableSocket(*socket);
        socket->OnError();
      } else {
        if (events[i].events & EPOLLIN) {
          socket->OnInput();
        }
        if (events[i].events & EPOLLOUT) {
          socket->OnOutput();
        }
      }
    }
  }
}

}  // namespace net
}  // namespace tsdb2
