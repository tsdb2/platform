// This unit provides a low-level API for IPC with asynchronous I/O. It supports TCP/IP sockets and
// Unix domain sockets. The former can be encrypted or unencrypted (but it's strongly recommended to
// encrypt them), while the latter are always unencrypted.
//
// The various socket classes are supported by a `SelectServer` singleton that uses epoll in
// edge-triggered mode to achieve the highest performance and parallelism. `SelectServer` runs a
// number of background threads that can be configured in the --select_server_num_workers command
// line flag.

#ifndef __TSDB2_NET_SOCKETS_H__
#define __TSDB2_NET_SOCKETS_H__

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/buffer.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "net/fd.h"

namespace tsdb2 {
namespace net {

using Buffer = ::tsdb2::common::Buffer;

absl::Duration constexpr kDefaultKeepAliveIdle = absl::Seconds(45);
absl::Duration constexpr kDefaultKeepAliveInterval = absl::Seconds(6);
int constexpr kDefaultKeepAliveCount = 5;

size_t constexpr kMaxUnixDomainSocketPathLength =
    sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;

struct InetSocketTag {};
static inline InetSocketTag constexpr kInetSocketTag;

struct UnixDomainSocketTag {};
static inline UnixDomainSocketTag constexpr kUnixDomainSocketTag;

// Options for configuring TCP/IP sockets.
struct SocketOptions {
  // Enables SO_KEEPALIVE. Uses the `keep_alive_params` below.
  bool keep_alive = false;

  struct KeepAliveParams {
    // Sets the TCP_KEEPIDLE time.
    absl::Duration idle = kDefaultKeepAliveIdle;

    // Sets the TCP_KEEPINTVL time.
    absl::Duration interval = kDefaultKeepAliveInterval;

    // Sets the TCP_KEEPCNT value.
    int count = kDefaultKeepAliveCount;
  };

  // Define the behavior of the keepalive packets, if enabled.
  KeepAliveParams keep_alive_params;

  // Sets the IP_TOS. Some of the standard available values are: `IPTOS_LOWDELAY`,
  // `IPTOS_THROUGHPUT`, `IPTOS_RELIABILITY`, and `IPTOS_MINCOST`.
  uint8_t ip_tos;
};

class SelectServer;

// Base class for all socket types.
class BaseSocket : public ::tsdb2::common::RefCounted {
 public:
  explicit BaseSocket(SelectServer* const parent, FD fd)
      : parent_(parent), initial_fd_(*fd), hash_(absl::HashOf(*fd)), fd_(std::move(fd)) {}

  virtual ~BaseSocket() = default;

  int initial_fd() const { return initial_fd_; }
  size_t hash() const { return hash_; }

 protected:
  friend class SelectServer;

  static absl::Status ConfigureInetSocket(FD const& fd, SocketOptions const& options);

  void RemoveFromEpoll();

  void OnLastUnref() override ABSL_LOCKS_EXCLUDED(mutex_);

  virtual void OnError() = 0;
  virtual void OnInput() = 0;
  virtual void OnOutput() = 0;

  SelectServer* const parent_;

 private:
  int const initial_fd_;
  size_t const hash_;

 protected:
  absl::Mutex mutable mutex_;
  FD fd_ ABSL_GUARDED_BY(mutex_);

 private:
  BaseSocket(BaseSocket const&) = delete;
  BaseSocket& operator=(BaseSocket const&) = delete;
  BaseSocket(BaseSocket&&) = delete;
  BaseSocket& operator=(BaseSocket&&) = delete;
};

// Generic unencrypted socket. This class is used for both client-side and server-side connections.
//
// Server-side sockets are implicitly constructed by `ListenerSocket` when accepting a connection
// and returned in the provided `AcceptCallback`. For example:
//
//   reffed_ptr<Socket> socket;
//   auto const listener = SelectServer::GetInstance()->CreateSocket<ListenerSocket>(
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
//   auto const socket = SelectServer::GetInstance()->CreateSocket<Socket>(
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
// a time and only one write operation at a time are supported. See the `Read` and `Write` methods
// for more information.
class Socket : public BaseSocket {
 public:
  using ConnectCallback = absl::AnyInvocable<void(absl::Status status)>;
  using ReadCallback = absl::AnyInvocable<void(absl::StatusOr<Buffer> status_or_buffer)>;
  using WriteCallback = absl::AnyInvocable<void(absl::Status status)>;

  static bool constexpr kIsListener = false;

  // Starts an asynchronous read operation. `callback` will be invoked upon completion. If the
  // operation is successful `callback` will receive a `Buffer` object of the specified `length`,
  // otherwise it will receive an error status.
  //
  // REQUIRES: `callback` must not be empty. The caller must always be notified of the end of a read
  // operation, otherwise it wouldn't know when it's okay to start the next.
  //
  // Only one read operation at a time is supported. If `Read` is called while another read is in
  // progress it will return immediately with an error status. If you need to split a read in two,
  // the next `Read` call must be issued in the `callback`.
  //
  // It usually doesn't make sense to issue multiple concurrent reads on the same socket, but if you
  // absolutely must then a read queue must be managed by the caller.
  absl::Status Read(size_t length, ReadCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  // Cancels the read operation currently in progress, if any. Returns true iff a read operation was
  // in progress and cancelled. The callback function of the cancelled operation will be invoked
  // with an error status.
  bool CancelRead() ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts an asynchronous write operation. `Socket` takes ownership of the provided `Buffer` and
  // takes care of deleting it as soon as it's no longer needed. Upon completion, `callback` will be
  // invoked with a status.
  //
  // REQUIRES: `callback` must not be empty. The caller must always be notified of the end of a
  // write operation, otherwise it wouldn't know when it's okay to start the next.
  //
  // Only one write operation at a time is supported. If `Write` is called while another write is in
  // progress it will return immediately with an error status. If you need to split a write in two,
  // the next `Write` call must be issued in the `callback`.
  //
  // It usually doesn't make sense to issue multiple concurrent writes on the same socket, but if
  // you absolutely must then a write queue must be managed by the caller.
  absl::Status Write(Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  // Cancels the write operation currently in progress, if any. Returns true iff a write operation
  // was in progress and cancelled. The callback function of the cancelled operation will be invoked
  // with an error status.
  bool CancelWrite() ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void OnError() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class SelectServer;

  struct ConnectedTag {};
  static inline ConnectedTag constexpr kConnectedTag;

  struct ConnectingTag {};
  static inline ConnectingTag constexpr kConnectingTag;

  struct ConnectStatus {
    explicit ConnectStatus(ConnectCallback connect_callback)
        : callback(std::move(connect_callback)) {}

    ConnectStatus(ConnectStatus const&) = delete;
    ConnectStatus& operator=(ConnectStatus const&) = delete;

    ConnectStatus(ConnectStatus&&) noexcept = default;
    ConnectStatus& operator=(ConnectStatus&&) noexcept = default;

    ConnectCallback callback;
  };

  struct ReadStatus {
    explicit ReadStatus(Buffer buffer, ReadCallback read_callback)
        : buffer(std::move(buffer)), callback(std::move(read_callback)) {}

    ReadStatus(ReadStatus const&) = delete;
    ReadStatus& operator=(ReadStatus const&) = delete;

    ReadStatus(ReadStatus&&) noexcept = default;
    ReadStatus& operator=(ReadStatus&&) noexcept = default;

    Buffer buffer;
    ReadCallback callback;
  };

  struct WriteStatus {
    explicit WriteStatus(Buffer buffer, size_t const remaining, WriteCallback callback)
        : buffer(std::move(buffer)), remaining(remaining), callback(std::move(callback)) {}

    WriteStatus(WriteStatus const&) = delete;
    WriteStatus& operator=(WriteStatus const&) = delete;

    WriteStatus(WriteStatus&&) noexcept = default;
    WriteStatus& operator=(WriteStatus&&) noexcept = default;

    Buffer buffer;
    size_t remaining;
    WriteCallback callback;
  };

  explicit Socket(SelectServer* const parent, ConnectedTag const&, FD fd)
      : BaseSocket(parent, std::move(fd)) {}

  explicit Socket(SelectServer* const parent, ConnectingTag const&, FD fd, ConnectCallback callback)
      : BaseSocket(parent, std::move(fd)), connect_status_(std::move(callback)) {}

  // Constructs a `Socket` from the specified file descriptor. Used by `ListenerSocket` to construct
  // sockets for accepted connections.
  static absl::StatusOr<std::unique_ptr<Socket>> Create(SelectServer* const parent, FD fd) {
    return std::unique_ptr<Socket>(new Socket(parent, kConnectedTag, std::move(fd)));
  }

  // Constructs a stream `Socket` connected to the specified host and port. This function uses
  // `getaddrinfo` to perform address resolution, so the `address` can be a numeric IPv4 address
  // (e.g. 192.168.0.1), a numeric IPv6 address (e.g. "::1"), or even a DNS name (e.g.
  // "www.example.com" or "localhost").
  static absl::StatusOr<std::unique_ptr<Socket>> Create(SelectServer* parent, InetSocketTag const&,
                                                        std::string const& address, uint16_t port,
                                                        SocketOptions const& options,
                                                        ConnectCallback callback);

  // Constructs a stream `Socket` connected to the specified Unix domain socket path.
  static absl::StatusOr<std::unique_ptr<Socket>> Create(SelectServer* parent,
                                                        UnixDomainSocketTag const&,
                                                        std::string_view socket_name,
                                                        ConnectCallback callback);

  void MaybeFinalizeConnect() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void AbortRead(absl::Status status) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void FinalizeWrite(absl::Status status) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::optional<ConnectStatus> connect_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  std::optional<ReadStatus> read_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  std::optional<WriteStatus> write_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
};

// A listener socket for unencrypted connections.
//
// To construct a listener for Unix Domain sockets:
//
//   auto const listener = SelectServer::GetInstance()->CreateSocket<ListenerSocket>(
//       kUnixDomainSocketTag, socket_name,
//       [&](absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       });
//
// To construct a listener for unencrypted dual-stack TCP/IP connections:
//
//   auto const listener = SelectServer::GetInstance()->CreateSocket<ListenerSocket>(
//       kInetSocketTag, address, port,
//       [&](absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       });
//
// WARNING: unencrypted TCP/IP connections are not recommended. Prefer using `SSLListenerSocket`
// instead.
//
// NOTE: the accept callback may be called many times concurrently. Ensure proper thread-safety of
// anything that's in its closure as well as any other data your callback may access.
template <typename Socket>
class ListenerSocket : public BaseSocket {
 public:
  static_assert(std::is_base_of_v<::tsdb2::net::Socket, Socket>,
                "The socket type created by ListenerSocket must be a subclass of Socket");

  using AcceptCallback = absl::AnyInvocable<void(
      absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket)>;

  static bool constexpr kIsListener = true;

  // Returns the local address this socket has been bound to. An empty string indicates it was bound
  // to INADDR6_ANY.
  std::string_view address() const { return address_; }

  // Returns the local TCP/IP port this socket is listening on.
  uint16_t port() const { return port_; }

 protected:
  void OnError() override;
  void OnInput() override;
  void OnOutput() override;

 private:
  friend class SelectServer;

  explicit ListenerSocket(SelectServer* const parent, InetSocketTag const&,
                          std::string_view const address, uint16_t const port, FD fd,
                          SocketOptions options, AcceptCallback callback)
      : BaseSocket(parent, std::move(fd)),
        address_(address),
        port_(port),
        options_(std::move(options)),
        callback_(std::move(callback)) {}

  explicit ListenerSocket(SelectServer* const parent, UnixDomainSocketTag const&,
                          std::string_view const socket_name, FD fd, AcceptCallback callback)
      : BaseSocket(parent, std::move(fd)),
        address_(socket_name),
        port_(0),
        options_(std::nullopt),
        callback_(std::move(callback)) {}

  static absl::StatusOr<std::unique_ptr<ListenerSocket<Socket>>> Create(
      SelectServer* parent, InetSocketTag const& tag, std::string_view address, uint16_t port,
      SocketOptions options, AcceptCallback callback);

  static absl::StatusOr<std::unique_ptr<ListenerSocket<Socket>>> Create(
      SelectServer* parent, UnixDomainSocketTag const& tag, std::string_view socket_name,
      AcceptCallback callback);

  absl::StatusOr<std::vector<FD>> AcceptAll() ABSL_LOCKS_EXCLUDED(mutex_);

  std::string const address_;
  uint16_t const port_;
  std::optional<SocketOptions> const options_;

  AcceptCallback callback_;
};

// `SelectServer` is a singleton that manages a pool of worker threads listening to I/O events on
// all the sockets in the process. All sockets must be created through this class.
//
// The number of worker threads is configured in the --select_server_num_workers command line flag.
//
// Despite being called "select" server, the implementation uses epoll in edge-triggered mode in
// order to achieve the highest performance and parallelism.
class SelectServer {
 public:
  // Returns the singleton instance of `SelectServer`.
  static SelectServer* GetInstance();

  // Start the `SelectServer`'s worker threads.
  void StartOrDie();

  // Creates a socket and makes the `SelectServer` listen to I/O events related to it.
  //
  // REQUIRES: `StartOrDie()` must have been completed.
  template <typename SocketType, typename... Args>
  absl::StatusOr<::tsdb2::common::reffed_ptr<SocketType>> CreateSocket(Args&&... args)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class BaseSocket;

  // Custom hash & eq functors to index sockets in hash data structures by file descriptor number.
  struct HashEq {
    struct Hash {
      using is_transparent = void;
      size_t operator()(std::unique_ptr<BaseSocket> const& socket) const { return socket->hash(); }
      size_t operator()(BaseSocket const* const socket) const { return socket->hash(); }
      size_t operator()(int const fd) const { return absl::HashOf(fd); }
      size_t operator()(FD const& fd) const { return absl::HashOf(*fd); }
    };

    struct Eq {
      using is_transparent = void;

      static int ToFD(std::unique_ptr<BaseSocket> const& socket) { return socket->initial_fd(); }
      static int ToFD(BaseSocket const* const socket) { return socket->initial_fd(); }
      static int ToFD(int const fd) { return fd; }
      static int ToFD(FD const& fd) { return *fd; }

      template <typename LHS, typename RHS>
      bool operator()(LHS&& lhs, RHS&& rhs) const {
        return ToFD(std::forward<LHS>(lhs)) == ToFD(std::forward<RHS>(rhs));
      }
    };
  };

  using SocketSet = absl::flat_hash_set<std::unique_ptr<BaseSocket>, HashEq::Hash, HashEq::Eq>;

  explicit SelectServer() = default;

  // Called by `GetInstance` only the first time, to construct the singleton `SelectServer`
  // instance.
  static SelectServer* CreateInstance();

  // `SelectServer` must be a singleton because unblocking its workers from `epoll_wait` and
  // stopping them is too complicated (it would require firing a signal, using thread-local storage
  // so that the signal handler knows what `SelectServer` instance it's stopping, write a state
  // machine to manage all the possible states, etc.)
  //
  // Not being able to destroy a `SelectServer` is not a big deal because a server can't be very
  // useful if it's completely deaf / can't accept connections, so we simply forbid destruction so
  // that the workers can keep running indefinitely and we avoid all the aforementioned
  // complexities.
  ~SelectServer() = delete;

  ::tsdb2::common::reffed_ptr<BaseSocket> LookupSocket(int fd) ABSL_LOCKS_EXCLUDED(mutex_);

  void DisableSocket(BaseSocket const& socket);

  std::unique_ptr<BaseSocket> RemoveSocket(BaseSocket const& socket);

  void WorkerLoop();

  absl::Mutex mutable mutex_;
  SocketSet sockets_ ABSL_GUARDED_BY(mutex_);

  int volatile epoll_fd_ = -1;
  std::vector<std::thread> workers_;
};

template <typename Socket>
void ListenerSocket<Socket>::OnError() {
  absl::MutexLock lock{&mutex_};
  RemoveFromEpoll();
  fd_.Close();
  callback_(absl::AbortedError("socket shutdown"));
}

template <typename Socket>
void ListenerSocket<Socket>::OnInput() {
  auto status_or_fds = AcceptAll();
  if (status_or_fds.ok()) {
    for (auto& fd : status_or_fds.value()) {
      if (options_) {
        auto configure_status = ConfigureInetSocket(fd, *options_);
        if (!configure_status.ok()) {
          callback_(std::move(configure_status));
          continue;
        }
      }
      callback_(parent_->CreateSocket<Socket>(std::move(fd)));
    }
  } else {
    callback_(status_or_fds.status());
  }
}

template <typename Socket>
void ListenerSocket<Socket>::OnOutput() {
  // Nothing to do here.
}

template <typename Socket>
absl::StatusOr<std::unique_ptr<ListenerSocket<Socket>>> ListenerSocket<Socket>::Create(
    SelectServer* const parent, InetSocketTag const& tag, std::string_view const address,
    uint16_t const port, SocketOptions options, AcceptCallback callback) {
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
  return std::unique_ptr<ListenerSocket<Socket>>(new ListenerSocket<Socket>(
      parent, tag, address, port, std::move(fd), std::move(options), std::move(callback)));
}

template <typename Socket>
absl::StatusOr<std::unique_ptr<ListenerSocket<Socket>>> ListenerSocket<Socket>::Create(
    SelectServer* const parent, UnixDomainSocketTag const& tag, std::string_view const socket_name,
    AcceptCallback callback) {
  if (!callback) {
    return absl::InvalidArgumentError("the accept callback must not be empty");
  }
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
  if (bind(*fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    return absl::ErrnoToStatus(errno, "bind() failed");
  }
  if (listen(*fd, SOMAXCONN) < 0) {
    return absl::ErrnoToStatus(errno, "listen() failed");
  }
  return std::unique_ptr<ListenerSocket<Socket>>(
      new ListenerSocket<Socket>(parent, tag, socket_name, std::move(fd), std::move(callback)));
}

template <typename Socket>
absl::StatusOr<std::vector<FD>> ListenerSocket<Socket>::AcceptAll() {
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

template <typename SocketType, typename... Args>
absl::StatusOr<::tsdb2::common::reffed_ptr<SocketType>> SelectServer::CreateSocket(Args&&... args) {
  static_assert(std::is_base_of_v<BaseSocket, SocketType>,
                "SocketType must be a subclass of BaseSocket");
  if (epoll_fd_ < 0) {
    return absl::FailedPreconditionError("SelectServer hasn't started yet");
  }
  auto status_or_socket = SocketType::Create(this, std::forward<Args>(args)...);
  if (!status_or_socket.ok()) {
    return status_or_socket.status();
  }
  std::unique_ptr<SocketType> socket = std::move(status_or_socket).value();
  ::tsdb2::common::reffed_ptr<SocketType> ptr{socket.get()};
  int const fd = socket->initial_fd();
  {
    absl::MutexLock lock{&mutex_};
    auto const [unused_it, inserted] = sockets_.emplace(std::move(socket));
    CHECK_EQ(inserted, true) << "internal error: duplicated file descriptor in socket map!";
  }
  struct epoll_event event;
  std::memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
  if constexpr (!SocketType::kIsListener) {
    event.events |= EPOLLOUT;
  }
  event.data.fd = fd;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    return absl::ErrnoToStatus(errno, "epoll_ctl(EPOLL_ADD) failed");
  }
  return ptr;
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SOCKETS_H__
