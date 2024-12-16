#ifndef __TSDB2_NET_SSL_SOCKETS_H__
#define __TSDB2_NET_SSL_SOCKETS_H__

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "common/simple_condition.h"
#include "common/utilities.h"
#include "net/base_sockets.h"
#include "net/epoll_server.h"
#include "net/ssl.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

ABSL_DECLARE_FLAG(absl::Duration, ssl_handshake_timeout);

namespace tsdb2 {
namespace net {

// Generic SSL/TLS socket. This class is used for both client-side and server-side connections.
//
// This class is thread-safe.
//
// Server-side sockets are implicitly constructed by `SSLListenerSocket` when accepting a connection
// and returned in the provided `AcceptCallback`. For example:
//
//   reffed_ptr<Socket> socket;
//   auto const listener = SSLListenerSocket<SSLSocket>::Create(
//       kInetSocketTag, address, port, SocketOptions(),
//       [&](absl::StatusOr<reffed_ptr<SSLSocket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           socket = std::move(status_or_socket).value();
//         }
//       });
//
// While client-side sockets can be constructed as follows:
//
//   auto const socket = SSLSocket::Create(
//       kInetSocketTag, "www.example.com", 80, SocketOptions(),
//       [](absl::Status const connect_status) {
//         if (!connect_status.ok()) {
//           // Connection to the provided address/port failed.
//         } else {
//           // We can start reading and writing.
//         }
//       });
//
// The I/O model of `SSLSocket` is fully asynchronous, but keep in mind that only one read operation
// at a time and only one write operation at a time are supported. It's okay to issue a read and a
// write concurrently. See the `Read` and `Write` methods for more information.
class SSLSocket : public BaseSocket {
 public:
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool> = true>
  using ConnectCallback =
      absl::AnyInvocable<void(tsdb2::common::reffed_ptr<SocketClass>, absl::Status)>;

  template <typename SocketClass = SSLSocket, typename... Args,
            std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> Create(Args&&... args);

  template <typename FirstSocket, typename SecondSocket, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_base_of<SSLSocket, FirstSocket>,
                                                std::is_base_of<SSLSocket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreateHeterogeneousPairForTesting(Args&&... args);

  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<SSLSocket>, tsdb2::common::reffed_ptr<SSLSocket>>>
  CreatePairForTesting() {
    return CreateHeterogeneousPairForTesting<SSLSocket, SSLSocket>();
  }

  ~SSLSocket() override ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  using InternalConnectCallback = absl::AnyInvocable<void(SSLSocket*, absl::Status)>;

  struct AcceptTag {};
  static inline AcceptTag constexpr kAcceptTag;

  struct ConnectTag {};
  static inline ConnectTag constexpr kConnectTag;

  // Constructs a `Socket` from the specified file descriptor. Used by `ListenerSocket` to construct
  // sockets for accepted connections. The `ConnectCallback` will be notified when the SSL handshake
  // is complete.
  template <typename SocketClass, typename... ExtraArgs,
            std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> CreateClass(
      EpollServer* parent, FD fd, ConnectCallback<SocketClass> callback, ExtraArgs&&... extra_args);

  // Constructs an `SSLSocket` connected to the specified host and port. This function uses
  // `getaddrinfo` to perform address resolution, so the `address` can be a numeric IPv4 address
  // (e.g. 192.168.0.1), a numeric IPv6 address (e.g. "::1"), or even a DNS name (e.g.
  // "www.example.com" or "localhost"). The `ConnectCallback` will be notified when the whole SSL
  // handshake is complete, not just the TCP SYN-ACK sequence.
  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> CreateClass(
      EpollServer* parent, std::string_view address, uint16_t port, SocketOptions const& options,
      ConnectCallback<SocketClass> callback);

  // TEST ONLY: creates a pair of connected SSL sockets using the `socketpair` syscall.
  template <typename FirstSocket, typename SecondSocket, typename... ExtraArgs,
            std::enable_if_t<std::conjunction_v<std::is_base_of<SSLSocket, FirstSocket>,
                                                std::is_base_of<SSLSocket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreateClassPair(EpollServer* parent, ConnectCallback<FirstSocket> first_callback,
                  ConnectCallback<SecondSocket> second_callback, ExtraArgs&&... extra_args);

  explicit SSLSocket(EpollServer* parent, AcceptTag, FD fd, internal::SSL ssl,
                     absl::Duration handshake_timeout, InternalConnectCallback callback);

  explicit SSLSocket(EpollServer* parent, ConnectTag, FD fd, internal::SSL ssl,
                     absl::Duration handshake_timeout, InternalConnectCallback callback);

 private:
  struct HashSocket {
    using is_transparent = void;

    size_t operator()(tsdb2::common::reffed_ptr<SSLSocket> const& socket) const {
      return absl::HashOf(socket);
    }

    size_t operator()(SSLSocket* const socket) const { return absl::HashOf(socket); }
  };

  struct CompareSockets {
    using is_transparent = void;

    static SSLSocket* ToRawPtr(tsdb2::common::reffed_ptr<SSLSocket> const& socket) {
      return socket.get();
    }

    static SSLSocket* ToRawPtr(SSLSocket* const socket) { return socket; }

    template <typename LHS, typename RHS>
    bool operator()(LHS&& lhs, RHS&& rhs) const {
      return ToRawPtr(std::forward<LHS>(lhs)) == ToRawPtr(std::forward<RHS>(rhs));
    }
  };

  using SocketSet =
      absl::flat_hash_set<tsdb2::common::reffed_ptr<SSLSocket>, HashSocket, CompareSockets>;

  using TimeoutSet = absl::flat_hash_set<tsdb2::common::Scheduler::Handle>;

  friend class EpollServer;

  template <typename SocketClass, std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool>>
  friend class SSLListenerSocket;

  struct ConnectState final {
    enum class Mode { kAccepting, kConnecting };

    explicit ConnectState(Mode const mode, InternalConnectCallback callback,
                          absl::Duration const timeout)
        : callback(std::move(callback)), mode(mode), timeout(timeout) {}

    ~ConnectState() = default;

    ConnectState(ConnectState const&) = delete;
    ConnectState& operator=(ConnectState const&) = delete;

    ConnectState(ConnectState&&) noexcept = default;
    ConnectState& operator=(ConnectState&&) noexcept = default;

    InternalConnectCallback callback;
    Mode mode;
    absl::Duration timeout;
    tsdb2::common::Scheduler::Handle timeout_handle = tsdb2::common::Scheduler::kInvalidHandle;
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

  static absl::Mutex socket_mutex_;

  // Keeps accepted sockets alive while they're still handshaking.
  static SocketSet handshaking_sockets_ ABSL_GUARDED_BY(socket_mutex_);

  // Adds a socket to the handshaking set.
  static void EmplaceHandshakingSocket(tsdb2::common::reffed_ptr<SSLSocket> socket)
      ABSL_LOCKS_EXCLUDED(socket_mutex_);

  // Removes a socket from the handshaking set.
  static tsdb2::common::reffed_ptr<SSLSocket> ExtractHandshakingSocket(SSLSocket* socket)
      ABSL_LOCKS_EXCLUDED(socket_mutex_);

  template <typename SocketClass,
            std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool> = true>
  static InternalConnectCallback MakeConnectCallbackAdapter(ConnectCallback<SocketClass> callback) {
    return [callback = std::move(callback)](SSLSocket* const socket, absl::Status status) mutable {
      callback(tsdb2::common::WrapReffed(static_cast<SocketClass*>(socket)), std::move(status));
    };
  }

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SSLSocket>> CreateInternal(
      EpollServer* const parent, Args&&... args) {
    return CreateClass<SSLSocket>(parent, std::forward<Args>(args)...);
  }

  template <typename FirstSocket, typename SecondSocket, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_base_of<SSLSocket, FirstSocket>,
                                                std::is_base_of<SSLSocket, SecondSocket>>,
                             bool> = true>
  static absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreatePairInternal(EpollServer* const parent, Args&&... args) {
    return SSLSocket::template CreateClassPair<FirstSocket, SecondSocket>(
        parent, std::forward<Args>(args)...);
  }

  SSLSocket(SSLSocket const&) = delete;
  SSLSocket& operator=(SSLSocket const&) = delete;
  SSLSocket(SSLSocket&&) = delete;
  SSLSocket& operator=(SSLSocket&&) = delete;

  ConnectState ExpungeConnectState() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    ConnectState state = std::move(connect_state_).value();
    MaybeCancelTimeout(&state.timeout_handle);
    connect_state_ = std::nullopt;
    return state;
  }

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

  absl::StatusOr<bool> Handshake() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  absl::Status StartHandshake() ABSL_LOCKS_EXCLUDED(mutex_);
  absl::Status ContinueHandshake(absl::ReleasableMutexLock* lock)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void ScheduleRead(Buffer buffer, ReadCallback callback, std::optional<absl::Duration> timeout)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void ScheduleWrite(Buffer buffer, size_t remaining, WriteCallback callback,
                     std::optional<absl::Duration> timeout) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

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

  internal::SSL const ssl_ ABSL_GUARDED_BY(mutex_);
  MaybeConnectState connect_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  MaybeReadState read_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  MaybeWriteState write_state_ ABSL_GUARDED_BY(mutex_) = std::nullopt;

  TimeoutSet active_timeouts_ ABSL_GUARDED_BY(mutex_);
};

// A listener socket for SSL/TLS connections.
//
// This class is thread-safe.
//
// Example usage:
//
//   auto const listener = SSLListenerSocket<SSLSocket>::Create(
//       address, port, SocketOptions(),
//       +[](void *arg, absl::StatusOr<tsdb2::common::reffed_ptr<SSLSocket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       },
//       callback_arg);
//
// NOTE: the accept callback may be called many times concurrently (for different accepted sockets).
template <typename SocketType,
          std::enable_if_t<std::is_base_of_v<SSLSocket, SocketType>, bool> = true>
class SSLListenerSocket : public BaseListenerSocket {
 public:
  // NOTE: we can't use `absl::AnyInvocable` for this callback because that's not guaranteed to be
  // thread-safe, even if the wrapped function is thread-safe. Other uses of `absl::AnyInvocable`
  // are always guarded by a mutex, but in order to improve concurrency we'd rather not use a mutex
  // in a listener socket (which is often a global singleton).
  using AcceptCallback =
      void (*)(void* arg, absl::StatusOr<tsdb2::common::reffed_ptr<SocketType>> status_or_socket);

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SSLListenerSocket>> Create(Args&&... args) {
    return EpollServer::GetInstance()->CreateSocket<SSLListenerSocket>(std::forward<Args>(args)...);
  }

 protected:
  template <
      typename ListenerSocketClass, typename... ExtraArgs,
      std::enable_if_t<std::is_base_of_v<SSLListenerSocket, ListenerSocketClass>, bool> = true>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ListenerSocketClass>> CreateClass(
      EpollServer* const parent, std::string_view const address, uint16_t const port,
      SocketOptions options, AcceptCallback const callback, void* const callback_arg,
      ExtraArgs&&... extra_args) {
    if (!callback) {
      return absl::InvalidArgumentError("the accept callback must not be empty");
    }
    DEFINE_VAR_OR_RETURN(fd, CreateInetListener(address, port));
    return tsdb2::common::WrapReffed(
        new ListenerSocketClass(parent, std::forward<ExtraArgs>(extra_args)..., address, port,
                                std::move(fd), options, callback, callback_arg));
  }

  explicit SSLListenerSocket(EpollServer* const parent, std::string_view const address,
                             uint16_t const port, FD fd, SocketOptions options,
                             AcceptCallback const callback, void* const callback_arg)
      : BaseListenerSocket(parent, address, port, std::move(fd)),
        options_(options),
        callback_arg_(callback_arg),
        callback_(callback) {}

  // Constructs a socket from an accepted file descriptor.
  virtual absl::StatusOr<tsdb2::common::reffed_ptr<SocketType>> CreateSocket(
      FD fd, SSLSocket::ConnectCallback<SocketType> callback) {
    return SocketType::Create(std::move(fd), std::move(callback));
  }

 private:
  friend class EpollServer;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<SSLListenerSocket>> CreateInternal(
      EpollServer* const parent, Args&&... args) {
    return CreateClass<SSLListenerSocket>(parent, std::forward<Args>(args)...);
  }

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
          auto const saved_errno = errno;
          KillSocket();
          return absl::ErrnoToStatus(saved_errno, "accept4()");
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

  void OnInput() override {
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
      auto status_or_socket = CreateSocket(
          std::move(fd), [this](tsdb2::common::reffed_ptr<SocketType> socket, absl::Status status) {
            if (status.ok()) {
              callback_(callback_arg_, std::move(socket));
            } else {
              callback_(callback_arg_, std::move(status));
            }
          });
      if (!status_or_socket.ok()) {
        callback_(callback_arg_, std::move(status_or_socket).status());
      }
    }
  }

  std::optional<SocketOptions> const options_;

  void* const callback_arg_;
  AcceptCallback const callback_;
};

class SSLSocketModule : public tsdb2::init::BaseModule {
 public:
  static SSLSocketModule* Get() { return instance_.Get(); };

 private:
  friend class tsdb2::common::NoDestructor<SSLSocketModule>;
  static tsdb2::common::NoDestructor<SSLSocketModule> instance_;

  explicit SSLSocketModule() : BaseModule("ssl_sockets") {
    tsdb2::init::RegisterModule(this, SocketModule::Get(), internal::SSLModule::Get());
  }
};

template <typename SocketClass, typename... Args,
          std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> SSLSocket::Create(Args&&... args) {
  DEFINE_VAR_OR_RETURN(
      socket, EpollServer::GetInstance()->CreateSocket<SocketClass>(std::forward<Args>(args)...));
  RETURN_IF_ERROR(socket->StartHandshake());
  return std::move(socket);
}

template <typename FirstSocket, typename SecondSocket, typename... ExtraArgs,
          std::enable_if_t<std::conjunction_v<std::is_base_of<SSLSocket, FirstSocket>,
                                              std::is_base_of<SSLSocket, SecondSocket>>,
                           bool>>
absl::StatusOr<
    std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
SSLSocket::CreateHeterogeneousPairForTesting(ExtraArgs&&... extra_args) {
  absl::Mutex mutex;
  bool first_ready = false;
  bool second_ready = false;
  absl::Status final_status = absl::OkStatus();
  DEFINE_VAR_OR_RETURN(
      sockets,
      (EpollServer::GetInstance()
           ->CreateHeterogeneousSocketPair<SSLSocket, FirstSocket, SecondSocket>(
               [&](tsdb2::common::reffed_ptr<FirstSocket> const socket, absl::Status status) {
                 absl::MutexLock lock{&mutex};
                 final_status.Update(std::move(status));
                 first_ready = true;
               },
               [&](tsdb2::common::reffed_ptr<SecondSocket> const socket, absl::Status status) {
                 absl::MutexLock lock{&mutex};
                 final_status.Update(std::move(status));
                 second_ready = true;
               },
               std::forward<ExtraArgs>(extra_args)...)));
  RETURN_IF_ERROR(sockets.first->StartHandshake());
  RETURN_IF_ERROR(sockets.second->StartHandshake());
  absl::MutexLock lock{&mutex,
                       tsdb2::common::SimpleCondition([&] { return first_ready && second_ready; })};
  if (final_status.ok()) {
    return std::move(sockets);
  } else {
    return final_status;
  }
}

template <typename SocketClass, typename... ExtraArgs,
          std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> SSLSocket::CreateClass(
    EpollServer* const parent, FD fd, ConnectCallback<SocketClass> callback,
    ExtraArgs&&... extra_args) {
  if (!callback) {
    return absl::InvalidArgumentError("The connect callback must not be empty");
  }
  DEFINE_VAR_OR_RETURN(ssl, internal::SSLContext::GetServerContext().MakeSSL(fd));
  auto socket = tsdb2::common::WrapReffed(new SocketClass(
      parent, std::forward<ExtraArgs>(extra_args)..., kAcceptTag, std::move(fd), std::move(ssl),
      absl::GetFlag(FLAGS_ssl_handshake_timeout),
      [callback = std::move(callback)](SSLSocket* const socket, absl::Status status) mutable {
        callback(ExtractHandshakingSocket(socket).downcast<SocketClass>(), std::move(status));
      }));
  SSLSocket* const ssl_socket = socket.get();
  EmplaceHandshakingSocket(tsdb2::common::WrapReffed(ssl_socket));
  return std::move(socket);
}

template <typename SocketClass, std::enable_if_t<std::is_base_of_v<SSLSocket, SocketClass>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketClass>> SSLSocket::CreateClass(
    EpollServer* const parent, std::string_view const address, uint16_t const port,
    SocketOptions const& options, ConnectCallback<SocketClass> callback) {
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
  DEFINE_VAR_OR_RETURN(ssl, internal::SSLContext::GetClientContext().MakeSSL(fd));
  if (connect_result < 0 && errno != EINPROGRESS) {
    return absl::ErrnoToStatus(errno, "connect()");
  }
  auto socket = tsdb2::common::WrapReffed(new SocketClass(
      parent, kConnectTag, std::move(fd), std::move(ssl),
      absl::GetFlag(FLAGS_ssl_handshake_timeout),
      [callback = std::move(callback)](SSLSocket* const socket, absl::Status status) mutable {
        callback(ExtractHandshakingSocket(socket).downcast<SocketClass>(), std::move(status));
      }));
  SSLSocket* const ssl_socket = socket.get();
  EmplaceHandshakingSocket(tsdb2::common::WrapReffed(ssl_socket));
  return std::move(socket);
}

template <typename FirstSocket, typename SecondSocket, typename... ExtraArgs,
          std::enable_if_t<std::conjunction_v<std::is_base_of<SSLSocket, FirstSocket>,
                                              std::is_base_of<SSLSocket, SecondSocket>>,
                           bool>>
absl::StatusOr<
    std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
SSLSocket::CreateClassPair(EpollServer* const parent, ConnectCallback<FirstSocket> first_callback,
                           ConnectCallback<SecondSocket> second_callback,
                           ExtraArgs&&... extra_args) {
  int fds[2] = {0, 0};
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) < 0) {
    return absl::ErrnoToStatus(errno, "socketpair(AF_UNIX, SOCK_STREAM)");
  }
  FD fd1{fds[0]};
  FD fd2{fds[1]};
  DEFINE_VAR_OR_RETURN(ssl1, internal::SSLContext::GetServerContext().MakeSSL(fd1));
  DEFINE_VAR_OR_RETURN(ssl2, internal::SSLContext::GetClientContext().MakeSSL(fd2));
  auto const handshake_timeout = absl::GetFlag(FLAGS_ssl_handshake_timeout);
  return std::make_pair(tsdb2::common::WrapReffed(new FirstSocket(
                            parent, std::forward<ExtraArgs>(extra_args)..., kAcceptTag,
                            std::move(fd1), std::move(ssl1), handshake_timeout,
                            MakeConnectCallbackAdapter(std::move(first_callback)))),
                        tsdb2::common::WrapReffed(new SecondSocket(
                            parent, kConnectTag, std::move(fd2), std::move(ssl2), handshake_timeout,
                            MakeConnectCallbackAdapter(std::move(second_callback)))));
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SSL_SOCKETS_H__
