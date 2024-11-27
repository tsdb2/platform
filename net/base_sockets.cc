#include "net/base_sockets.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace net {

absl::StatusOr<FD> CreateInetListener(std::string_view const address, uint16_t const port) {
  struct sockaddr_in6 sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = ::htons(port);
  if (address.empty()) {
    sa.sin6_addr = IN6ADDR_ANY_INIT;
  } else {
    std::string const address_string{address};
    if (::inet_pton(AF_INET6, address_string.c_str(), &sa.sin6_addr) < 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid address: \"", absl::CEscape(address), "\""));
    }
  }
  int const result = ::socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "socket(AF_INET6, SOCK_STREAM) failed");
  }
  FD fd{result};
  int opt = 0;
  if (::setsockopt(*fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
    return absl::ErrnoToStatus(errno, "setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, 0) failed");
  }
  if (::bind(*fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    return absl::ErrnoToStatus(errno, "bind() failed");
  }
  if (::listen(*fd, SOMAXCONN) < 0) {
    return absl::ErrnoToStatus(errno, "listen() failed");
  }
  return std::move(fd);
}

absl::Status ConfigureInetSocket(FD const& fd, SocketOptions const& options) {
  if (options.keep_alive) {
    int64_t optval = 1;
    if (::setsockopt(*fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, "setsockopt(SOL_SOCKET, SO_KEEPALIVE, 1) failed");
    }
    optval = absl::ToInt64Seconds(options.keep_alive_params.idle);
    if (::setsockopt(*fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, ",
                                                     options.keep_alive_params.idle, ") failed"));
    }
    optval = absl::ToInt64Seconds(options.keep_alive_params.interval);
    if (::setsockopt(*fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno,
                                 absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, ",
                                              options.keep_alive_params.interval, ") failed"));
    }
    optval = options.keep_alive_params.count;
    if (::setsockopt(*fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(errno, absl::StrCat("setsockopt(IPPROTO_TCP, TCP_KEEPCNT, ",
                                                     options.keep_alive_params.count, ") failed"));
    }
  }
  if (options.ip_tos) {
    auto const optval = *options.ip_tos;
    if (::setsockopt(*fd, IPPROTO_IP, IP_TOS, &optval, sizeof(optval)) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("setsockopt(IPPROTO_IP, IP_TOS, ", optval, ") failed"));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> BaseSocket::is_keep_alive() const {
  {
    absl::MutexLock lock{&mutex_};
    if (!fd_) {
      return absl::FailedPreconditionError("this socket has been shut down");
    }
    int optval = 0;
    socklen_t optsize = sizeof(optval);
    if (!::getsockopt(*fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, &optsize)) {
      return !!optval;
    }
  }
  return absl::ErrnoToStatus(errno, "getsockopt(SOL_SOCKET, SO_KEEPALIVE) failed");
}

absl::StatusOr<KeepAliveParams> BaseSocket::keep_alive_params() const {
  absl::MutexLock lock{&mutex_};
  if (!fd_) {
    return absl::FailedPreconditionError("this socket has been shut down");
  }
  int64_t optval = 0;
  ASSIGN_OR_RETURN(optval, GetIntSockOpt(SOL_SOCKET, "SOL_SOCKET", SO_KEEPALIVE, "SO_KEEPALIVE"));
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

absl::StatusOr<uint8_t> BaseSocket::ip_tos() const {
  {
    absl::MutexLock lock{&mutex_};
    if (!fd_) {
      return absl::FailedPreconditionError("this socket has been shut down");
    }
    uint8_t optval = 0;
    socklen_t optsize = sizeof(optval);
    if (!::getsockopt(*fd_, IPPROTO_IP, IP_TOS, &optval, &optsize)) {
      return optval;
    }
  }
  return absl::ErrnoToStatus(errno, "getsockopt(IPPROTO_IP, IP_TOS) failed");
}

bool BaseSocket::Close() { return CloseInternal(absl::CancelledError("socket shutdown")); }

void BaseSocket::OnLastUnref() {
  Close();
  EpollTarget::OnLastUnref();
}

absl::Status BaseSocket::SkipInternal(size_t const length, SkipCallback callback,
                                      std::optional<absl::Duration> const timeout) {
  static size_t constexpr kChunkSize = 4096;
  return ReadInternal(
      std::min(kChunkSize, length),
      [this, length, callback = std::move(callback),
       timeout](absl::StatusOr<Buffer> status_or_buffer) mutable {
        if (!status_or_buffer.ok()) {
          callback(std::move(status_or_buffer).status());
          return;
        }
        auto const skipped = status_or_buffer->size();
        // free the received chunk before reading the next
        status_or_buffer = Buffer();
        if (skipped < length) {
          SkipInternal(length - skipped, std::move(callback), timeout).IgnoreError();
        } else {
          callback(absl::OkStatus());
        }
      },
      timeout);
}

BaseSocket::ReadCallback BaseSocket::MakeReadSuccessCallback(ReadSuccessCallback callback) {
  return [callback = std::move(callback)](absl::StatusOr<Buffer> status_or_buffer) mutable {
    if (status_or_buffer.ok()) {
      callback(std::move(status_or_buffer).value());
    }
  };
}

BaseSocket::SkipCallback BaseSocket::MakeSkipSuccessCallback(SkipSuccessCallback callback) {
  return [callback = std::move(callback)](absl::Status status) mutable {
    if (status.ok()) {
      callback();
    }
  };
}

BaseSocket::WriteCallback BaseSocket::MakeWriteSuccessCallback(WriteSuccessCallback callback) {
  return [callback = std::move(callback)](absl::Status const status) mutable {
    if (status.ok()) {
      callback();
    }
  };
}

absl::StatusOr<int64_t> BaseSocket::GetIntSockOpt(int const level,
                                                  std::string_view const level_name,
                                                  int const option,
                                                  std::string_view const option_name) const {
  int64_t optval = 0;
  socklen_t optsize = sizeof(optval);
  if (::getsockopt(*fd_, level, option, &optval, &optsize) < 0) {
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

void BaseListenerSocket::OnOutput() {
  // Nothing to do here.
}

tsdb2::common::NoDestructor<SocketModule> SocketModule::instance_;

absl::Status SocketModule::Initialize() {
  if (::signal(SIGPIPE, SIG_IGN) != SIG_ERR) {
    return absl::OkStatus();
  } else {
    return absl::ErrnoToStatus(errno, "signal(SIGPIPE, SIG_IGN)");
  }
}

}  // namespace net
}  // namespace tsdb2
