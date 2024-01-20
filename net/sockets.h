#ifndef __TSDB2_NET_SOCKETS_H__
#define __TSDB2_NET_SOCKETS_H__

#include <errno.h>
#include <sys/epoll.h>

#include <cstddef>
#include <cstdint>
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
#include "absl/synchronization/mutex.h"
#include "common/buffer.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "net/fd.h"

namespace tsdb2 {
namespace net {

using Buffer = ::tsdb2::common::Buffer;

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

  void RemoveFromParent() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OnLastUnref() override ABSL_LOCKS_EXCLUDED(mutex_);

  virtual void OnError() = 0;
  virtual void OnInput() = 0;
  virtual void OnOutput() = 0;

  SelectServer* const parent_;
  int const initial_fd_;
  size_t const hash_;

  absl::Mutex mutable mutex_;
  FD fd_ ABSL_GUARDED_BY(mutex_);

 private:
  BaseSocket(BaseSocket const&) = delete;
  BaseSocket& operator=(BaseSocket const&) = delete;
  BaseSocket(BaseSocket&&) = delete;
  BaseSocket& operator=(BaseSocket&&) = delete;
};

// Generic unencrypted socket.
class Socket : public BaseSocket {
 public:
  using ReadCallback = absl::AnyInvocable<void(absl::StatusOr<Buffer> status_or_buffer)>;
  using WriteCallback = absl::AnyInvocable<void(absl::Status status)>;

  static bool constexpr kIsListener = false;

  absl::Status Read(size_t length, ReadCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  bool CancelRead() ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status Write(Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  bool CancelWrite() ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void OnError() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class SelectServer;

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

  explicit Socket(SelectServer* const parent, FD fd) : BaseSocket(parent, std::move(fd)) {}

  static absl::StatusOr<std::unique_ptr<Socket>> Create(SelectServer* const parent, FD fd) {
    return std::unique_ptr<Socket>(new Socket(parent, std::move(fd)));
  }

  void AbortRead(absl::Status status) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void FinalizeWrite(absl::Status status) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::optional<ReadStatus> read_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  std::optional<WriteStatus> write_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
};

// A listener socket for unencrypted connections.
//
// To construct a listener for Unix Domain sockets:
//
//   auto const listener = SelectServer::GetInstance()->CreateSocket<ListenerSocket>(
//       ListenerSocket::kUnixDomainSocketTag, socket_name,
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
//       ListenerSocket::kInetSocketTag, address, port,
//       [&](absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket) {
//         if (!status_or_socket.ok()) {
//           // There was an error other than EAGAIN / EWOULDBLOCK.
//         } else {
//           // Start reading/writing data from/to the new socket.
//         }
//       });
//
// WARNING: unencrypted TCP/IP connections are not recommended. Use `SSLListenerSocket` instead.
//
// NOTE: the accept callback may be called many times concurrently. Ensure proper thread-safety of
// anything that's in its closure as well as any other data your callback may access.
class ListenerSocket : public BaseSocket {
 public:
  using AcceptCallback = absl::AnyInvocable<void(
      absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket)>;

  struct InetSocketTag {};
  static inline InetSocketTag constexpr kInetSocketTag;

  struct UnixDomainSocketTag {};
  static inline UnixDomainSocketTag constexpr kUnixDomainSocketTag;

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
                          AcceptCallback callback)
      : BaseSocket(parent, std::move(fd)),
        address_(address),
        port_(port),
        callback_(std::move(callback)) {}

  static absl::StatusOr<std::unique_ptr<ListenerSocket>> Create(SelectServer* parent,
                                                                InetSocketTag const& tag,
                                                                std::string_view address,
                                                                uint16_t port,
                                                                AcceptCallback callback);

  std::vector<FD> AcceptAll() ABSL_LOCKS_EXCLUDED(mutex_);

  std::string const address_;
  uint16_t const port_;

  AcceptCallback callback_;
};

// `SelectServer` is a singleton that manages a pool of worker threads listening to I/O events on
// all the sockets in the process. All sockets must be created through this class.
//
// The number of worker threads is configured in the --tsdb2_select_server_num_workers command line
// flag.
//
// Despite being called "select" server, the implementation uses epoll in edge-triggered mode in
// order to achieve the highest performance and parallelism.
class SelectServer {
 public:
  // Returns the singleton instance of `SelectServer`.
  static SelectServer* GetInstance();

  // Start the `SelectServer`'s worker threads.
  void StartOrDie() ABSL_LOCKS_EXCLUDED(epoll_mutex_);

  // Create a socket and make the `SelectServer` listen to I/O events related to it.
  //
  // REQUIRES: `StartOrDie()` must have been completed.
  template <typename SocketType, typename... Args>
  absl::StatusOr<::tsdb2::common::reffed_ptr<SocketType>> CreateSocket(Args&&... args)
      ABSL_LOCKS_EXCLUDED(epoll_mutex_);

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

  int epoll_fd() const {
    absl::MutexLock lock{&epoll_mutex_};
    return epoll_fd_;
  }

  ::tsdb2::common::reffed_ptr<BaseSocket> LookupSocket(int fd) ABSL_LOCKS_EXCLUDED(sockets_mutex_);

  void RemoveSocket(BaseSocket const& socket);

  void WorkerLoop();

  absl::Mutex mutable sockets_mutex_;
  SocketSet sockets_ ABSL_GUARDED_BY(sockets_mutex_);

  absl::Mutex mutable epoll_mutex_;
  int epoll_fd_ ABSL_GUARDED_BY(epoll_mutex_) = -1;
  std::vector<std::thread> workers_ ABSL_GUARDED_BY(epoll_mutex_);
};

template <typename SocketType, typename... Args>
absl::StatusOr<::tsdb2::common::reffed_ptr<SocketType>> SelectServer::CreateSocket(Args&&... args) {
  static_assert(std::is_base_of_v<BaseSocket, SocketType>,
                "SocketType must be a subclass of BaseSocket");
  absl::ReaderMutexLock epoll_lock{&epoll_mutex_};
  if (epoll_fd_ < 0) {
    return absl::FailedPreconditionError("SelectServer hasn't started yet");
  }
  auto status_or_socket = SocketType::Create(this, std::forward<Args>(args)...);
  if (!status_or_socket.ok()) {
    return status_or_socket.status();
  }
  std::unique_ptr<SocketType> socket = std::move(status_or_socket).value();
  SocketType* const ptr = socket.get();
  int const fd = socket->initial_fd();
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
  if constexpr (!SocketType::kIsListener) {
    event.events |= EPOLLOUT;
  }
  event.data.fd = fd;
  {
    absl::MutexLock sockets_lock{&sockets_mutex_};
    auto const [unused_it, inserted] = sockets_.emplace(std::move(socket));
    CHECK_EQ(inserted, true) << "internal error: duplicated file descriptor in socket map!";
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
      return absl::ErrnoToStatus(errno, "epoll_ctl(EPOLL_ADD) failed");
    }
  }
  return ::tsdb2::common::reffed_ptr<SocketType>(ptr);
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SOCKETS_H__
