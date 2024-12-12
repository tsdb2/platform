#ifndef __TSDB2_NET_SOCKETS_H__
#define __TSDB2_NET_SOCKETS_H__

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "common/utilities.h"
#include "net/base_sockets.h"
#include "net/epoll_server.h"

namespace tsdb2 {
namespace net {

// Generic unencrypted socket. This class is used for both client-side and server-side connections.
//
// This class is thread-safe.
//
// Server-side sockets are implicitly constructed by `ListenerSocket` when accepting a connection
// and returned in the provided `AcceptCallback`. For example:
//
//   reffed_ptr<Socket> socket;
//   auto const listener = ListenerSocket<Socket>::Create(
//       kInetSocketTag, address, port, [&](absl::StatusOr<reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           socket = std::move(status_or_socket).value();
//         }
//       });
//
// While client-side sockets can be constructed as follows:
//
//   auto const socket = Socket::Create(
//       kInetSocketTag, "www.example.com", 80, [](absl::Status connect_status) {
//         if (!connect_status.ok()) {
//           // Connection to the provided address/port failed.
//         } else {
//           // We can start reading and writing.
//         }
//       });
//
// WARNING: unencrypted TCP/IP connections are not recommended. Prefer using `SSLSocket` instead.
//
// The I/O model of `Socket` is fully asynchronous, but keep in mind that only one read operation at
// a time and only one write operation at a time are supported. It's okay to issue a read and a
// write concurrently. See the `Read` and `Write` methods for more information.
class Socket : public BaseSocket {
 public:
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  using ConnectCallback =
      absl::AnyInvocable<void(tsdb2::common::reffed_ptr<SocketClass>, absl::Status)>;

  template <typename SocketClass = Socket, typename... Args,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<Socket>> Create(Args&&... args) {
    return EpollServer::GetInstance()->CreateSocket<Socket>(std::forward<Args>(args)...);
  }

  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<Socket>, tsdb2::common::reffed_ptr<Socket>>>
  CreatePair() {
    return EpollServer::GetInstance()->CreateSocketPair<Socket>();
  }

  ~Socket() override ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  using InternalConnectCallback = absl::AnyInvocable<void(Socket*, absl::Status)>;

  struct ConnectedTag {};
  static inline ConnectedTag constexpr kConnectedTag;

  struct ConnectingTag {};
  static inline ConnectingTag constexpr kConnectingTag;

  // Constructs a `Socket` from the specified file descriptor. Used by `ListenerSocket` to construct
  // sockets for accepted connections.
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> CreateClass(
      EpollServer* const parent, FD fd) {
    return tsdb2::common::WrapReffed(new SocketClass(parent, kConnectedTag, std::move(fd)));
  }

  // Constructs a stream `Socket` connected to the specified host and port. This function uses
  // `getaddrinfo` to perform address resolution, so the `address` can be a numeric IPv4 address
  // (e.g. 192.168.0.1), a numeric IPv6 address (e.g. "::1"), or even a DNS name (e.g.
  // "www.example.com" or "localhost").
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> CreateClass(
      EpollServer* parent, InetSocketTag /*inet_socket_tag*/, std::string_view address,
      uint16_t port, SocketOptions const& options, ConnectCallback<SocketClass> callback);

  // Constructs a stream `Socket` connected to the specified Unix domain socket path. The path is
  // specified in the `socket_name` argument and must not be longer than
  // `kMaxUnixDomainSocketPathLength` characters.
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> CreateClass(
      EpollServer* parent, UnixDomainSocketTag /*unix_domain_socket_tag*/,
      std::string_view socket_name, ConnectCallback<SocketClass> callback);

  // Creates a pair of connected sockets using the `socketpair` syscall.
  template <typename FirstSocket, typename SecondSocket,
            std::enable_if_t<std::conjunction_v<std::is_base_of<Socket, FirstSocket>,
                                                std::is_base_of<Socket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreateClassPair(EpollServer* parent);

  explicit Socket(EpollServer* const parent, ConnectedTag /*connect_tag*/, FD fd)
      : BaseSocket(parent, std::move(fd)) {}

  explicit Socket(EpollServer* const parent, ConnectingTag /*connect_tag*/, FD fd,
                  InternalConnectCallback callback)
      : BaseSocket(parent, std::move(fd)), connect_state_(std::move(callback)) {}

 private:
  using TimeoutSet = absl::flat_hash_set<tsdb2::common::Scheduler::Handle>;

  friend class EpollServer;

  struct ConnectState final {
    explicit ConnectState(InternalConnectCallback callback) : callback(std::move(callback)) {}
    ~ConnectState() = default;

    ConnectState(ConnectState const&) = delete;
    ConnectState& operator=(ConnectState const&) = delete;

    ConnectState(ConnectState&&) noexcept = default;
    ConnectState& operator=(ConnectState&&) noexcept = default;

    InternalConnectCallback callback;
  };

  using MaybeConnectState = std::optional<ConnectState>;

  struct ReadState final {
    explicit ReadState(Buffer buffer, ReadCallback callback,
                       std::optional<absl::Duration> const timeout,
                       tsdb2::common::Scheduler::Handle const timeout_handle)
        : buffer(std::move(buffer)),
          callback(std::move(callback)),
          timeout(timeout),
          timeout_handle(timeout_handle) {}

    ~ReadState() = default;

    ReadState(ReadState const&) = delete;
    ReadState& operator=(ReadState const&) = delete;

    ReadState(ReadState&&) noexcept = default;
    ReadState& operator=(ReadState&&) noexcept = default;

    Buffer buffer;
    ReadCallback callback;
    std::optional<absl::Duration> timeout;
    tsdb2::common::Scheduler::Handle timeout_handle;
  };

  using MaybeReadState = std::optional<ReadState>;

  struct WriteState final {
    explicit WriteState(Buffer buffer, size_t const remaining, WriteCallback callback,
                        std::optional<absl::Duration> const timeout,
                        tsdb2::common::Scheduler::Handle const timeout_handle)
        : buffer(std::move(buffer)),
          remaining(remaining),
          callback(std::move(callback)),
          timeout(timeout),
          timeout_handle(timeout_handle) {}

    ~WriteState() = default;

    WriteState(WriteState const&) = delete;
    WriteState& operator=(WriteState const&) = delete;

    WriteState(WriteState&&) noexcept = default;
    WriteState& operator=(WriteState&&) noexcept = default;

    Buffer buffer;
    size_t remaining;
    WriteCallback callback;
    std::optional<absl::Duration> timeout;
    tsdb2::common::Scheduler::Handle timeout_handle;
  };

  using MaybeWriteState = std::optional<WriteState>;

  using PendingState = std::tuple<MaybeConnectState, MaybeReadState, MaybeWriteState>;

  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool> = true>
  static InternalConnectCallback MakeConnectCallbackAdapter(ConnectCallback<SocketClass> callback) {
    return [callback = std::move(callback)](Socket* const socket, absl::Status status) mutable {
      callback(tsdb2::common::WrapReffed(static_cast<SocketClass*>(socket)), std::move(status));
    };
  }

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<Socket>> CreateInternal(EpollServer* const parent,
                                                                          Args&&... args) {
    return Socket::template CreateClass<Socket>(parent, std::forward<Args>(args)...);
  }

  // Creates a pair of connected sockets using the `socketpair` syscall.
  template <typename FirstSocket, typename SecondSocket,
            std::enable_if_t<std::conjunction_v<std::is_base_of<Socket, FirstSocket>,
                                                std::is_base_of<Socket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreatePairInternal(EpollServer* const parent) {
    return Socket::template CreateClassPair<FirstSocket, SecondSocket>(parent);
  }

  Socket(Socket const&) = delete;
  Socket& operator=(Socket const&) = delete;
  Socket(Socket&&) = delete;
  Socket& operator=(Socket&&) = delete;

  ReadState ExpungeReadState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    ReadState state = std::move(read_state_).value();
    MaybeCancelTimeout(&state.timeout_handle);
    read_state_ = std::nullopt;
    return state;
  }

  WriteState ExpungeWriteState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    WriteState state = std::move(write_state_).value();
    MaybeCancelTimeout(&state.timeout_handle);
    write_state_ = std::nullopt;
    return state;
  }

  PendingState ExpungeAllPendingState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  absl::Status AbortCallbacks(PendingState state, absl::Status status) ABSL_LOCKS_EXCLUDED(mutex_);
  bool CloseInternal(absl::Status status) override ABSL_LOCKS_EXCLUDED(mutex_);

  void MaybeFinalizeConnect() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OnError() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

  tsdb2::common::Scheduler::Handle ScheduleTimeout(absl::Duration timeout,
                                                   std::string_view status_message)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool MaybeCancelTimeout(tsdb2::common::Scheduler::Handle* handle_ptr)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Timeout(std::string_view status_message) ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status ReadInternal(size_t length, ReadCallback callback,
                            std::optional<absl::Duration> timeout) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status WriteInternal(Buffer buffer, WriteCallback callback,
                             std::optional<absl::Duration> timeout) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  MaybeConnectState connect_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  MaybeReadState read_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  MaybeWriteState write_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;

  TimeoutSet active_timeouts_ ABSL_GUARDED_BY(mutex_);
};

// A listener socket for unencrypted connections.
//
// This class is thread-safe.
//
// To construct a listener for Unix Domain sockets:
//
//   auto const listener = ListenerSocket<Socket>::Create(
//       kUnixDomainSocketTag, socket_name, SocketOptions(),
//       +[](void *arg, absl::StatusOr<tsdb2::common::reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       },
//       callback_arg);
//
// To construct a listener for unencrypted dual-stack TCP/IP connections:
//
//   auto const listener = ListenerSocket<Socket>::Create(
//       kInetSocketTag, address, port, SocketOptions(),
//       +[](void *arg, absl::StatusOr<tsdb2::common::reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       },
//       callback_arg);
//
// WARNING: unencrypted TCP/IP connections are not recommended. Prefer using `SSLListenerSocket`
// instead.
//
// NOTE: the accept callback may be called many times concurrently (for different accepted sockets).
template <typename SocketType, std::enable_if_t<std::is_base_of_v<Socket, SocketType>, bool> = true>
class ListenerSocket : public BaseListenerSocket {
 public:
  // NOTE: we can't use `absl::AnyInvocable` for this callback because that's not guaranteed to be
  // thread-safe, even if the wrapped function is thread-safe. Other uses of `absl::AnyInvocable`
  // are always guarded by a mutex, but in order to improve concurrency we'd rather not use a mutex
  // in a listener socket (which is often a global singleton).
  using AcceptCallback =
      void (*)(void* arg, absl::StatusOr<tsdb2::common::reffed_ptr<SocketType>> status_or_socket);

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ListenerSocket>> Create(Args&&... args) {
    return EpollServer::GetInstance()->CreateSocket<ListenerSocket>(std::forward<Args>(args)...);
  }

 private:
  friend class EpollServer;

  static absl::StatusOr<tsdb2::common::reffed_ptr<ListenerSocket>> CreateInternal(
      EpollServer* const parent, InetSocketTag const tag, std::string_view const address,
      uint16_t const port, SocketOptions options, AcceptCallback const callback,
      void* const callback_arg) {
    if (!callback) {
      return absl::InvalidArgumentError("the accept callback must not be empty");
    }
    DEFINE_VAR_OR_RETURN(fd, CreateInetListener(address, port));
    return tsdb2::common::WrapReffed(new ListenerSocket(parent, tag, address, port, std::move(fd),
                                                        options, callback, callback_arg));
  }

  static absl::StatusOr<tsdb2::common::reffed_ptr<ListenerSocket>> CreateInternal(
      EpollServer* const parent, UnixDomainSocketTag const tag, std::string_view const socket_name,
      AcceptCallback const callback, void* const callback_arg) {
    if (!callback) {
      return absl::InvalidArgumentError("the accept callback must not be empty");
    }
    if (socket_name.size() > kMaxUnixDomainSocketPathLength) {
      return absl::InvalidArgumentError(absl::StrCat("path `", absl::CEscape(socket_name),
                                                     "` exceeds the maximum length of ",
                                                     kMaxUnixDomainSocketPathLength));
    }
    int const result = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (result < 0) {
      return absl::ErrnoToStatus(errno, "socket(AF_UNIX, SOCK_STREAM)");
    }
    FD fd{result};
    struct sockaddr_un sa {};
    sa.sun_family = AF_UNIX;
    std::strncpy(sa.sun_path, socket_name.data(), kMaxUnixDomainSocketPathLength);
    if (::bind(*fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
      return absl::ErrnoToStatus(errno, "bind()");
    }
    if (::listen(*fd, SOMAXCONN) < 0) {
      return absl::ErrnoToStatus(errno, "listen()");
    }
    return tsdb2::common::WrapReffed(
        new ListenerSocket(parent, tag, socket_name, std::move(fd), callback, callback_arg));
  }

  explicit ListenerSocket(EpollServer* const parent, InetSocketTag /*inet_socket_tag*/,
                          std::string_view const address, uint16_t const port, FD fd,
                          SocketOptions options, AcceptCallback const callback,
                          void* const callback_arg)
      : BaseListenerSocket(parent, address, port, std::move(fd)),
        options_(options),
        callback_arg_(callback_arg),
        callback_(callback) {}

  explicit ListenerSocket(EpollServer* const parent, UnixDomainSocketTag /*unix_domain_socket_tag*/,
                          std::string_view const socket_name, FD fd, AcceptCallback const callback,
                          void* const callback_arg)
      : BaseListenerSocket(parent, socket_name, 0, std::move(fd)),
        options_(std::nullopt),
        callback_arg_(callback_arg),
        callback_(callback) {}

  absl::StatusOr<std::vector<FD>> AcceptAll() {
    std::vector<FD> fds;
    absl::MutexLock lock{&mutex_};
    if (!fd_) {
      return absl::FailedPreconditionError("this socket has been shut down");
    }
    while (true) {
      int const result = ::accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
      if (result < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          KillSocket();
          return absl::ErrnoToStatus(errno, "accept4()");
        } else {
          return fds;
        }
      } else {
        fds.emplace_back(result);
      }
    }
  }

  void OnError() override ABSL_LOCKS_EXCLUDED(mutex_) {
    {
      absl::MutexLock lock{&mutex_};
      KillSocket();
    }
    callback_(callback_arg_, absl::AbortedError("socket shutdown"));
  }

  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_) {
    auto status_or_fds = AcceptAll();
    if (!status_or_fds.ok()) {
      return callback_(callback_arg_, status_or_fds.status());
    }
    for (auto& fd : status_or_fds.value()) {
      if (options_) {
        auto configure_status = ConfigureInetSocket(fd, *options_);
        if (!configure_status.ok()) {
          callback_(callback_arg_, std::move(configure_status));
          continue;
        }
      }
      callback_(callback_arg_, parent()->template CreateSocket<SocketType>(std::move(fd)));
    }
  }

  std::optional<SocketOptions> const options_;

  void* const callback_arg_;
  AcceptCallback const callback_;
};

template <typename SocketClass, std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> Socket::CreateClass(
    EpollServer* const parent, InetSocketTag /*inet_socket_tag*/, std::string_view const address,
    uint16_t const port, SocketOptions const& options, ConnectCallback<SocketClass> callback) {
  if (!callback) {
    return absl::InvalidArgumentError("The connect callback must not be empty");
  }
  std::string const address_string{address};
  auto const port_string = absl::StrCat(port);
  struct addrinfo* rai;
  if (::getaddrinfo(address_string.c_str(), port_string.c_str(), nullptr, &rai) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("getaddrinfo(\"", absl::CEscape(address), "\", ", port_string, ")"));
  }
  struct addrinfo* ai = rai;
  while (ai && ai->ai_socktype != SOCK_STREAM) {
    ai = ai->ai_next;
  }
  if (!ai) {
    ::freeaddrinfo(rai);
    return absl::NotFoundError(absl::StrCat("getaddrinfo(\"", absl::CEscape(address), "\", ",
                                            port_string,
                                            ") didn't return any SOCK_STREAM addresses"));
  }
  int const socket_result =
      ::socket(ai->ai_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
  if (socket_result < 0) {
    ::freeaddrinfo(rai);
    return absl::ErrnoToStatus(errno, "socket()");
  }
  FD fd{socket_result};
  auto status = ConfigureInetSocket(fd, options);
  if (!status.ok()) {
    ::freeaddrinfo(rai);
    return status;
  }
  int const connect_result = ::connect(*fd, ai->ai_addr, ai->ai_addrlen);
  ::freeaddrinfo(rai);
  if (connect_result < 0) {
    if (errno != EINPROGRESS) {
      return absl::ErrnoToStatus(errno, "connect()");
    } else {
      return tsdb2::common::WrapReffed(new SocketClass(
          parent, kConnectingTag, std::move(fd), MakeConnectCallbackAdapter(std::move(callback))));
    }
  } else {
    auto socket = tsdb2::common::WrapReffed(new SocketClass(parent, kConnectedTag, std::move(fd)));
    callback(socket, absl::OkStatus());
    return std::move(socket);
  }
}

template <typename SocketClass, std::enable_if_t<std::is_base_of_v<Socket, SocketClass>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> Socket::CreateClass(
    EpollServer* const parent, UnixDomainSocketTag /*unix_domain_socket_tag*/,
    std::string_view const socket_name, ConnectCallback<SocketClass> callback) {
  if (!callback) {
    return absl::InvalidArgumentError("The connect callback must not be empty");
  }
  if (socket_name.size() > kMaxUnixDomainSocketPathLength) {
    return absl::InvalidArgumentError(absl::StrCat("path `", absl::CEscape(socket_name),
                                                   "` exceeds the maximum length of ",
                                                   kMaxUnixDomainSocketPathLength));
  }
  int const result = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, "socket(AF_UNIX, SOCK_STREAM)");
  }
  FD fd{result};
  struct sockaddr_un sa {};
  sa.sun_family = AF_UNIX;
  std::strncpy(sa.sun_path, socket_name.data(), kMaxUnixDomainSocketPathLength);
  if (::connect(*fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) < 0) {
    if (errno != EINPROGRESS) {
      return absl::ErrnoToStatus(errno, "connect()");
    } else {
      return tsdb2::common::WrapReffed(new SocketClass(
          parent, kConnectingTag, std::move(fd), MakeConnectCallbackAdapter(std::move(callback))));
    }
  } else {
    auto socket = tsdb2::common::WrapReffed(new SocketClass(parent, kConnectedTag, std::move(fd)));
    callback(socket, absl::OkStatus());
    return std::move(socket);
  }
}

template <typename FirstSocket, typename SecondSocket,
          std::enable_if_t<std::conjunction_v<std::is_base_of<Socket, FirstSocket>,
                                              std::is_base_of<Socket, SecondSocket>>,
                           bool>>
absl::StatusOr<
    std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
Socket::CreateClassPair(EpollServer* const parent) {
  int fds[2] = {0, 0};
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) < 0) {
    return absl::ErrnoToStatus(errno, "socketpair(AF_UNIX, SOCK_STREAM)");
  }
  return std::make_pair(
      tsdb2::common::WrapReffed(new FirstSocket(parent, kConnectedTag, FD(fds[0]))),
      tsdb2::common::WrapReffed(new SecondSocket(parent, kConnectedTag, FD(fds[1]))));
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SOCKETS_H__
