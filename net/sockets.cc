#include "net/sockets.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

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
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/buffer.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"
#include "io/fd.h"

ABSL_FLAG(uint16_t, select_server_num_workers, 10, "Number of I/O worker threads.");

namespace tsdb2 {
namespace net {

namespace {

using ::tsdb2::common::reffed_ptr;

size_t constexpr kMaxEvents = 1024;

}  // namespace

absl::Status BaseSocket::ConfigureInetSocket(FD const& fd, SocketOptions const& options) {
  if (options.keep_alive) {
    int64_t optval = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, "setsockopt(SOL_SOCKET, SO_KEEPALIVE, 1) failed");
    }
    optval = absl::ToInt64Seconds(options.keep_alive_params.idle);
    if (setsockopt(*fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, ",
                                                     options.keep_alive_params.idle, ") failed"));
    }
    optval = absl::ToInt64Seconds(options.keep_alive_params.interval);
    if (setsockopt(*fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno,
                                 absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, ",
                                              options.keep_alive_params.interval, ") failed"));
    }
    optval = options.keep_alive_params.count;
    if (setsockopt(*fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPCNT, ",
                                                     options.keep_alive_params.count, ") failed"));
    }
  }
  if (options.ip_tos) {
    auto const optval = *options.ip_tos;
    if (setsockopt(*fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("setsockopt(IPPROTO_IP, IP_TOS, ", optval, ") failed"));
    }
  }
  return absl::OkStatus();
}

void BaseSocket::CloseFD(bool const attempt_shutdown) {
  if (fd_) {
    parent_->KillSocket(*fd_);
    if (attempt_shutdown) {
      shutdown(*fd_, SHUT_RDWR);
    }
    fd_.Close();
  }
}

void BaseSocket::OnLastUnref() {
  std::unique_ptr<BaseSocket> socket;
  absl::MutexLock lock{&mutex_};
  CloseFD(/*attempt_shutdown=*/true);
  socket = parent_->DestroySocket(*this);
}

absl::StatusOr<bool> Socket::is_keep_alive() const {
  {
    absl::MutexLock lock{&mutex_};
    if (!fd_) {
      return absl::FailedPreconditionError("this socket has been shut down");
    }
    int optval = 0;
    socklen_t optsize = sizeof(optval);
    if (!getsockopt(*fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, &optsize)) {
      return !!optval;
    }
  }
  return absl::ErrnoToStatus(errno, "getsockopt(SOL_SOCKET, SO_KEEPALIVE) failed");
}

absl::StatusOr<KeepAliveParams> Socket::keep_alive_params() const {
  absl::MutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  int64_t optval = 0;
  socklen_t optsize = sizeof(optval);
  if (getsockopt(*fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, &optsize) < 0) {
    return absl::ErrnoToStatus(errno, "getsockopt(SOL_SOCKET, SO_KEEPALIVE) failed");
  }
  if (!optval) {
    return absl::FailedPreconditionError("TCP keep-alives are disabled for this socket");
  }
  KeepAliveParams kap;
  ASSIGN_OR_RETURN(optval, GetIntSockOpt(IPPROTO_TCP, "IPPROTO_TCP", TCP_KEEPIDLE, "TCP_KEEPIDLE"));
  kap.idle = absl::Seconds(optval);
  ASSIGN_OR_RETURN(optval,
                   GetIntSockOpt(IPPROTO_TCP, "IPPROTO_TCP", TCP_KEEPINTVL, "TCP_KEEPINTVL"));
  kap.interval = absl::Seconds(optval);
  ASSIGN_OR_RETURN(optval, GetIntSockOpt(IPPROTO_TCP, "IPPROTO_TCP", TCP_KEEPCNT, "TCP_KEEPCNT"));
  kap.count = optval;
  return kap;
}

absl::StatusOr<uint8_t> Socket::ip_tos() const {
  {
    absl::MutexLock lock{&mutex_};
    if (!fd_) {
      return absl::FailedPreconditionError("this socket has been shut down");
    }
    uint8_t optval = 0;
    socklen_t optsize = sizeof(optval);
    if (!getsockopt(*fd_, IPPROTO_IP, IP_TOS, &optval, &optsize)) {
      return optval;
    }
  }
  return absl::ErrnoToStatus(errno, "getsockopt(IPPROTO_IP, IP_TOS) failed");
}

absl::Status Socket::Read(size_t const length, ReadCallback callback) {
  if (!length) {
    return absl::InvalidArgumentError("the number of bytes to read must be at least 1");
  }
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
    CHECK_GT(length, offset);
    ssize_t const result =
        recv(*fd_, buffer.as_byte_array() + offset, length - offset, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return CloseLocked(absl::ErrnoToStatus(errno, "recv() failed"));
      } else {
        read_status_.emplace(std::move(buffer), std::move(callback));
        return absl::OkStatus();
      }
    } else if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        return MaybeCloseLocked(callback(std::move(buffer)));
      }
    } else {
      CloseLocked(absl::AbortedError("socket shutdown")).IgnoreError();
    }
  }
}

bool Socket::CancelRead() {
  absl::MutexLock lock{&mutex_};
  if (read_status_) {
    AbortReadAndClose(absl::CancelledError("cancelled by the user"));
    return true;
  } else {
    return false;
  }
}

absl::Status Socket::Write(Buffer buffer, WriteCallback callback) {
  if (buffer.empty()) {
    return absl::InvalidArgumentError("the buffer to write must not be empty");
  }
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
    CHECK_GT(remaining, 0);
    ssize_t const result = send(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return CloseLocked(absl::ErrnoToStatus(errno, "send() failed"));
      } else {
        write_status_.emplace(std::move(buffer), remaining, std::move(callback));
        return absl::OkStatus();
      }
    } else if (result > 0) {
      offset += result;
      if (!(offset < buffer.size())) {
        return MaybeCloseLocked(callback(absl::OkStatus()));
      }
    } else {
      CloseLocked(absl::AbortedError("socket shutdown")).IgnoreError();
    }
  }
}

bool Socket::CancelWrite() {
  absl::MutexLock lock{&mutex_};
  if (write_status_) {
    FinalizeWriteOrClose(absl::CancelledError("cancelled by the user"));
    return true;
  } else {
    return false;
  }
}

void Socket::OnError() {
  absl::MutexLock lock{&mutex_};
  CloseLocked(absl::AbortedError("socket shutdown")).IgnoreError();
}

void Socket::OnInput() {
  absl::MutexLock lock{&mutex_};
  MaybeFinalizeConnect();
  while (fd_ && read_status_) {
    auto& buffer = read_status_->buffer;
    size_t const offset = buffer.size();
    size_t const remaining = buffer.capacity() - offset;
    CHECK_GT(remaining, 0);
    ssize_t const result = recv(*fd_, buffer.as_byte_array() + offset, remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        AbortReadAndClose(absl::ErrnoToStatus(errno, "recv() failed"));
      } else {
        return;
      }
    } else if (result > 0) {
      buffer.Advance(result);
      if (buffer.is_full()) {
        MaybeCloseLocked(read_status_->callback(std::move(buffer))).IgnoreError();
        read_status_.reset();
      }
    } else {
      CloseLocked(absl::AbortedError("socket shutdown")).IgnoreError();
    }
  }
}

void Socket::OnOutput() {
  absl::MutexLock lock{&mutex_};
  MaybeFinalizeConnect();
  while (fd_ && write_status_) {
    auto const& buffer = write_status_->buffer;
    CHECK_GT(write_status_->remaining, 0);
    CHECK_LT(write_status_->remaining, buffer.size());
    size_t const offset = buffer.size() - write_status_->remaining;
    ssize_t const result =
        send(*fd_, buffer.as_byte_array() + offset, write_status_->remaining, MSG_DONTWAIT);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        FinalizeWriteOrClose(absl::ErrnoToStatus(errno, "send() failed"));
      } else {
        return;
      }
    } else if (result > 0) {
      write_status_->remaining -= result;
      if (!(offset + result < buffer.size())) {
        FinalizeWrite(absl::OkStatus()).IgnoreError();
      }
    } else {
      CloseLocked(absl::AbortedError("socket shutdown")).IgnoreError();
    }
  }
}

absl::StatusOr<std::unique_ptr<Socket>> Socket::Create(
    SelectServer* const parent, InetSocketTag const&, std::string const& address,
    uint16_t const port, SocketOptions const& options, ConnectCallback callback) {
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
  auto status = ConfigureInetSocket(fd, options);
  if (!status.ok()) {
    freeaddrinfo(rai);
    return status;
  }
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

absl::StatusOr<std::unique_ptr<Socket>> Socket::Create(SelectServer* const parent,
                                                       UnixDomainSocketTag const&,
                                                       std::string_view const socket_name,
                                                       ConnectCallback callback) {
  if (socket_name.size() > kMaxUnixDomainSocketPathLength) {
    return absl::InvalidArgumentError(absl::StrCat("path `", absl::CEscape(socket_name),
                                                   "` exceeds the maximum length of ",
                                                   kMaxUnixDomainSocketPathLength));
  }
  int const result = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "socket(AF_UNIX, SOCK_STREAM) failed");
  }
  FD fd{result};
  struct sockaddr_un sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  std::strncpy(sa.sun_path, socket_name.data(), kMaxUnixDomainSocketPathLength);
  if (connect(*fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
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

absl::StatusOr<int64_t> Socket::GetIntSockOpt(int const level, std::string_view const level_name,
                                              int const option,
                                              std::string_view const option_name) const {
  int64_t optval = 0;
  socklen_t optsize = sizeof(optval);
  if (getsockopt(*fd_, level, option, &optval, &optsize) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("getsockopt(", level_name, ", ", option_name, ") failed"));
  }
  switch (optsize) {
    case 1:
      return static_cast<int64_t>(*reinterpret_cast<int8_t*>(&optval));
    case 2:
      return static_cast<int64_t>(*reinterpret_cast<int16_t*>(&optval));
    case 4:
      return static_cast<int64_t>(*reinterpret_cast<int32_t*>(&optval));
    case 8:
      return static_cast<int64_t>(*reinterpret_cast<int64_t*>(&optval));
    default:
      return absl::UnknownError("getsockopt() returned an unknown value size");
  }
}

absl::Status Socket::CloseLocked(absl::Status status) {
  CloseFD(/*attempt_shutdown=*/false);
  if (connect_status_) {
    connect_status_->callback(status);
    connect_status_.reset();
  }
  if (write_status_) {
    FinalizeWrite(status).IgnoreError();
  }
  if (read_status_) {
    AbortRead(status);
  }
  return status;
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
  callback(std::move(status)).IgnoreError();
}

void Socket::AbortReadAndClose(absl::Status status) {
  AbortRead(status);
  CloseLocked(std::move(status)).IgnoreError();
}

absl::Status Socket::FinalizeWrite(absl::Status status) {
  auto callback = std::move(write_status_->callback);
  write_status_.reset();
  return callback(status);
}

void Socket::FinalizeWriteOrClose(absl::Status status) {
  MaybeCloseLocked(FinalizeWrite(std::move(status))).IgnoreError();
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

void SelectServer::KillSocket(int const fd) {
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  {
    absl::MutexLock lock{&mutex_};
    auto node = sockets_.extract(fd);
    if (node) {
      dead_sockets_.emplace(node.value().release());
      return;
    }
  }
  LOG(ERROR) << "file descriptor " << fd << " not found among live sockets!";
}

std::unique_ptr<BaseSocket> SelectServer::DestroySocket(BaseSocket const& socket) {
  absl::MutexLock lock{&mutex_};
  auto const it1 = dead_sockets_.find(&socket);
  if (it1 != dead_sockets_.end() && !(*it1)->is_referenced()) {
    return std::unique_ptr<BaseSocket>(dead_sockets_.extract(it1).value().release());
  }
  auto const it2 = sockets_.find(&socket);
  if (it2 != sockets_.end() && !(*it2)->is_referenced()) {
    return std::unique_ptr<BaseSocket>(sockets_.extract(it2).value().release());
  }
  return nullptr;
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
