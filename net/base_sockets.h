#ifndef __TSDB2_NET_BASE_SOCKETS_H__
#define __TSDB2_NET_BASE_SOCKETS_H__

#include <sys/socket.h>
#include <sys/un.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "io/buffer.h"  // IWYU pragma: export
#include "io/fd.h"      // IWYU pragma: export
#include "net/epoll_server.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace net {

using ::tsdb2::io::Buffer;  // IWYU pragma: export
using ::tsdb2::io::FD;      // IWYU pragma: export

inline std::string_view constexpr kLocalHost = "::1";

inline absl::Duration constexpr kDefaultKeepAliveIdle = absl::Seconds(45);
inline absl::Duration constexpr kDefaultKeepAliveInterval = absl::Seconds(6);
inline int constexpr kDefaultKeepAliveCount = 5;

inline size_t constexpr kMaxUnixDomainSocketPathLength =
    sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path) - 1;

struct InetSocketTag {};
static inline InetSocketTag constexpr kInetSocketTag;

struct UnixDomainSocketTag {};
static inline UnixDomainSocketTag constexpr kUnixDomainSocketTag;

// Parameters to configure TCP keep-alives. Set these inside `SocketOptions`.
struct KeepAliveParams {
  // Sets the TCP_KEEPIDLE time.
  absl::Duration idle = kDefaultKeepAliveIdle;

  // Sets the TCP_KEEPINTVL time.
  absl::Duration interval = kDefaultKeepAliveInterval;

  // Sets the TCP_KEEPCNT value.
  int count = kDefaultKeepAliveCount;
};

// Options for configuring TCP/IP sockets.
struct SocketOptions {
  // Enables SO_KEEPALIVE. Uses the `keep_alive_params` below.
  bool keep_alive = false;

  // Define the behavior of the keepalive packets, if enabled.
  KeepAliveParams keep_alive_params;

  // Optionally sets the IP type of service. See RFC 791 for possible values.
  std::optional<uint8_t> ip_tos;
};

// This low-level function is used by both `ListenerSocket` and `SSLListenerSocket` to create a
// non-UDS listener socket accepting connections at the specified local address and port.
absl::StatusOr<FD> CreateInetListener(std::string_view address, uint16_t port);

// Low-level function used by listener sockets to configure the accepted sockets. This function
// performs several `setsockopt` calls on the provided file descriptor to apply the settings
// specified in `options`.
absl::Status ConfigureInetSocket(FD const& fd, SocketOptions const& options);

// Abstract base class for all streaming sockets. Inherited by `Socket` and `SSLSocket`.
class BaseSocket : public EpollTarget {
 public:
  using ReadCallback = absl::AnyInvocable<void(absl::StatusOr<Buffer>)>;
  using ReadSuccessCallback = absl::AnyInvocable<void(Buffer)>;
  using SkipCallback = absl::AnyInvocable<void(absl::Status)>;
  using SkipSuccessCallback = absl::AnyInvocable<void()>;
  using WriteCallback = absl::AnyInvocable<void(absl::Status)>;
  using WriteSuccessCallback = absl::AnyInvocable<void()>;

  static inline bool constexpr kIsListener = false;

  // Indicates whether the socket is open and performing I/O (based on whether the underlying file
  // descriptor is open and registered in the epoll server). The socket results closed either after
  // an explicit `Close` call or after implicit closure following an unrecoverable I/O error.
  using EpollTarget::is_open;

  // Destructors of subclasses should call `Close`, which must be idempotent and thread-safe.
  ~BaseSocket() override = default;

  // Returns a boolean indicating whether TCP keep-alives are enabled for this socket.
  absl::StatusOr<bool> is_keep_alive() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the `KeepAliveParams` configured for this socket. An error status is returned if
  // keep-alives are not enabled for this socket.
  absl::StatusOr<KeepAliveParams> keep_alive_params() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the IP TOS for this socket.
  absl::StatusOr<uint8_t> ip_tos() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts an asynchronous read operation. `callback` will be invoked upon completion. If the
  // operation is successful `callback` will receive a `Buffer` object of the specified `length`,
  // otherwise it will receive an error status.
  //
  // NOTE: if the operation fails it means the socket is no longer usable, so `Read` automatically
  // closes it. Any further read or write attempts will fail immediately with an error status. If
  // your read callback receives an error status it can safely assume the socket is close, no longer
  // registered in the epoll server, and can be destroyed.
  //
  // REQUIRES: `length` must be greater than zero.
  //
  // REQUIRES: `callback` must not be empty. The caller must always be notified of the end of a read
  // operation, otherwise it wouldn't know when it's okay to start the next.
  //
  // Only one read operation at a time is supported. If `Read` is called while another read is in
  // progress it will return immediately with an error status. If you need to split a read in two,
  // the next `Read` call must be issued in the `callback`.
  //
  // It usually doesn't make sense to issue multiple reads on the same socket concurrently, but if
  // you absolutely must then a read queue must be managed by the caller.
  absl::Status Read(size_t const length, ReadCallback callback) {
    return ReadInternal(length, std::move(callback), std::nullopt);
  }

  // Like `Read`, but fails (closing the socket) if no data is received for more than the timeout
  // duration.
  //
  // REQUIRES: if provided, `timeout` must be greater than zero.
  //
  // The timeout is reset every time some data is received, so it should be okay to set a low value
  // even if we're expecting large amounts of data.
  absl::Status ReadWithTimeout(size_t const length, ReadCallback callback,
                               absl::Duration const timeout) {
    return ReadInternal(length, std::move(callback), timeout);
  }

  // Starts an asynchronous read operation to skip `length` bytes of data efficiently, without
  // keeping any of it in memory. `callback` will be invoked upon completion and will receive only
  // an `absl::Status` without any buffer.
  //
  // NOTE: if the operation fails it means the socket is no longer usable, so `Skip` automatically
  // closes it. Any further read or write attempts will fail immediately with an error status. If
  // your skip callback receives an error status it can safely assume the socket is close, no longer
  // registered in the epoll server, and can be destroyed.
  //
  // REQUIRES: `length` must be greater than zero.
  //
  // REQUIRES: `callback` must not be empty. The caller must always be notified of the end of a read
  // operation, otherwise it wouldn't know when it's okay to start the next.
  //
  // Only one read operation at a time is supported, and skip operations count as reads. If `Read`
  // or `Skip` is called while another read is in progress it will return immediately with an error
  // status. If you need to split a read in two, the next `Read` or `Skip` call must be issued in
  // the `callback`.
  //
  // It usually doesn't make sense to issue multiple reads on the same socket concurrently, but if
  // you absolutely must then a read queue must be managed by the caller.
  absl::Status Skip(size_t const length, SkipCallback callback) {
    return SkipInternal(length, std::move(callback), std::nullopt);
  }

  // Like `Skip`, but fails (closing the socket) if no data is received for more than the timeout
  // duration.
  //
  // REQUIRES: if provided, `timeout` must be greater than zero.
  //
  // NOTE: the timeout is reset every time some data is received, so it does NOT represent the
  // overall time limit the peer must send all the data within. It only represents how long the peer
  // is allowed to "sleep" for.
  absl::Status SkipWithTimeout(size_t const length, SkipCallback callback,
                               absl::Duration const timeout) {
    return SkipInternal(length, std::move(callback), timeout);
  }

  // Starts an asynchronous write operation. The socket takes ownership of the provided `Buffer` and
  // takes care of deleting it as soon as it's no longer needed. Upon completion, `callback` will be
  // invoked with a status.
  //
  // NOTE: if the operation fails it means the socket is no longer usable, so `Write` automatically
  // closes it. Any further read or write attempts will fail immediately with an error status. If
  // your write callback receives an error status it can safely assume the socket is close, no
  // longer registered in the epoll server, and can be destroyed.
  //
  // REQUIRES: `buffer.size()` must be greater than zero.
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
  absl::Status Write(Buffer buffer, WriteCallback callback) {
    return WriteInternal(std::move(buffer), std::move(callback), std::nullopt);
  }

  // Like `Write`, but fails (closing the socket) if no data is transmitted for more than the
  // timeout duration.
  //
  // REQUIRES: if provided, `timeout` must be greater than zero.
  //
  // The timeout is reset every time some data is transferred, so it should be okay to set a low
  // value even if we're transferring large amounts of data.
  absl::Status WriteWithTimeout(Buffer buffer, WriteCallback callback,
                                absl::Duration const timeout) {
    return WriteInternal(std::move(buffer), std::move(callback), timeout);
  }

  // Shuts down the socket gracefully and removes it from the epoll server. All pending callbacks
  // are cancelled with an error status.
  //
  // WARNING: all user-provided callbacks run outside of any locks on the socket's internal mutex,
  // otherwise they wouldn't be able to issue new I/O calls; because of this it's possible that one
  // or more callbacks are still running when `Close` returns. It's up to the caller to determine
  // when all callbacks really have finished. It is safe to assume that a thread running an I/O
  // callback doesn't refer to the socket any more after the callback returns.
  //
  // Any attempt to issue new reads or writes after a call to `Close` will result in an error
  // status; no closures will be stored.
  //
  // As with all the socket framework this method is thread-safe, so it can be called multiple times
  // concurrently and only one call will perform the actual closure. The returned boolean indicates
  // whether closure was performed; false is returned when the socket was closed by another `Close`
  // call.
  bool Close();

 protected:
  explicit BaseSocket(EpollServer* const parent, FD fd) : EpollTarget(parent, std::move(fd)) {}

  void OnLastUnref() override;

  virtual absl::Status ReadInternal(size_t length, ReadCallback callback,
                                    std::optional<absl::Duration> timeout) = 0;

  absl::Status SkipInternal(size_t length, SkipCallback callback,
                            std::optional<absl::Duration> timeout);

  virtual absl::Status WriteInternal(Buffer buffer, WriteCallback callback,
                                     std::optional<absl::Duration> timeout) = 0;

  virtual bool CloseInternal(absl::Status status) = 0;

 private:
  static ReadCallback MakeReadSuccessCallback(ReadSuccessCallback callback);
  static SkipCallback MakeSkipSuccessCallback(SkipSuccessCallback callback);
  static WriteCallback MakeWriteSuccessCallback(WriteSuccessCallback callback);

  BaseSocket(BaseSocket const&) = delete;
  BaseSocket& operator=(BaseSocket const&) = delete;
  BaseSocket(BaseSocket&&) = delete;
  BaseSocket& operator=(BaseSocket&&) = delete;

  absl::StatusOr<int64_t> GetIntSockOpt(int level, std::string_view level_name, int option,
                                        std::string_view option_name) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
};

// Abstract base class for all listener sockets. Inherited by `ListenerSocket` and
// `SSLListenerSocket`.
class BaseListenerSocket : public EpollTarget {
 public:
  static inline bool constexpr kIsListener = true;

  // Indicates whether the socket is open and accepting connections (based on whether the underlying
  // file descriptor is open and registered in the epoll server). The socket is automatically closed
  // when an unrecoverable I/O error occurs.
  using EpollTarget::is_open;

  ~BaseListenerSocket() override = default;

  // Returns the local address this socket has been bound to. An empty string indicates it was bound
  // to INADDR6_ANY.
  std::string_view address() const { return address_; }

  // Returns the local TCP/IP port this socket is listening on.
  uint16_t port() const { return port_; }

 protected:
  explicit BaseListenerSocket(EpollServer* const parent, std::string_view const address,
                              uint16_t const port, FD fd)
      : EpollTarget(parent, std::move(fd)), address_(address), port_(port) {}

 private:
  BaseListenerSocket(BaseListenerSocket const&) = delete;
  BaseListenerSocket& operator=(BaseListenerSocket const&) = delete;
  BaseListenerSocket(BaseListenerSocket&&) = delete;
  BaseListenerSocket& operator=(BaseListenerSocket&&) = delete;

  void OnOutput() override;

  std::string const address_;
  uint16_t const port_;
};

class SocketModule : public tsdb2::init::BaseModule {
 public:
  static SocketModule* Get() { return instance_.Get(); };
  absl::Status Initialize() override;

 private:
  friend class tsdb2::common::NoDestructor<SocketModule>;
  static tsdb2::common::NoDestructor<SocketModule> instance_;

  explicit SocketModule();
};

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_BASE_SOCKETS_H__
