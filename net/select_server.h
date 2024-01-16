#ifndef __TSDB2_NET_SELECT_SERVER_H__
#define __TSDB2_NET_SELECT_SERVER_H__

#include <errno.h>
#include <sys/epoll.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/buffer.h"
#include "net/fd.h"

namespace tsdb2 {
namespace net {

using Buffer = ::tsdb2::common::Buffer;

class SelectServer;

class BaseSocket {
 public:
  explicit BaseSocket(FD fd) : fd_{std::move(fd)} {}

  virtual ~BaseSocket() = default;

  int fd() const { return *fd_; }

 protected:
  friend class SelectServer;

  virtual void OnInput() = 0;
  virtual void OnOutput() = 0;

  FD const fd_;

 private:
  BaseSocket(BaseSocket const&) = delete;
  BaseSocket& operator=(BaseSocket const&) = delete;
  BaseSocket(BaseSocket&&) = delete;
  BaseSocket& operator=(BaseSocket&&) = delete;
};

class ListenerSocket : public BaseSocket {
 public:
  using Callback = absl::AnyInvocable<void(absl::StatusOr<FD> status_or_socket)>;

  static bool constexpr kIsListener = true;

  // Returns the local address this socket has been bound to. An empty string indicates it was bound
  // to INADDR6_ANY.
  std::string_view address() const { return address_; }

  // Returns the local TCP/IP port this socket is listening on.
  uint16_t port() const { return port_; }

  absl::Status Accept(Callback callback) ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class SelectServer;

  static absl::StatusOr<ListenerSocket*> Create(std::string_view address, uint16_t port);

  explicit ListenerSocket(std::string_view const address, uint16_t const port, FD fd)
      : BaseSocket(std::move(fd)), address_(address), port_(port) {}

  std::string const address_;
  uint16_t const port_;

  absl::Mutex mutable mutex_;
  std::optional<Callback> callback_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
};

class Socket : public BaseSocket {
 public:
  using ReadCallback = absl::AnyInvocable<void(absl::StatusOr<Buffer> status_or_buffer)>;
  using WriteCallback = absl::AnyInvocable<void(absl::Status status)>;

  static bool constexpr kIsListener = false;

  absl::Status Read(size_t length, ReadCallback callback) ABSL_LOCKS_EXCLUDED(read_mutex_);

  bool CancelRead() ABSL_LOCKS_EXCLUDED(read_mutex_);

  absl::Status Write(Buffer const& buffer, WriteCallback callback)
      ABSL_LOCKS_EXCLUDED(write_mutex_);

  bool CancelWrite() ABSL_LOCKS_EXCLUDED(write_mutex_);

 protected:
  void OnInput() override ABSL_LOCKS_EXCLUDED(read_mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(write_mutex_);

 private:
  struct ReadStatus {
    explicit ReadStatus(Buffer buffer, ReadCallback callback)
        : buffer(std::move(buffer)), callback(std::move(callback)) {}

    Buffer buffer;
    ReadCallback callback;
  };

  struct WriteStatus {
    explicit WriteStatus(Buffer const& buffer, size_t const remaining, WriteCallback callback)
        : buffer(buffer), remaining(remaining), callback(std::move(callback)) {}

    Buffer const& buffer;
    size_t remaining;
    WriteCallback callback;
  };

  friend class SelectServer;

  static absl::StatusOr<Socket*> Create(FD fd) { return new Socket(std::move(fd)); }

  explicit Socket(FD fd) : BaseSocket(std::move(fd)) {}

  void FinalizeWrite(absl::Status status) ABSL_EXCLUSIVE_LOCKS_REQUIRED(write_mutex_);

  absl::Mutex mutable read_mutex_;
  std::optional<ReadStatus> read_status_ ABSL_GUARDED_BY(read_mutex_) = std::nullopt;

  absl::Mutex mutable write_mutex_;
  std::optional<WriteStatus> write_status_ ABSL_GUARDED_BY(write_mutex_) = std::nullopt;
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
  void StartOrDie() ABSL_LOCKS_EXCLUDED(mutex_);

  // Create a socket and make the `SelectServer` listen to I/O events related to it.
  //
  // REQUIRES: `StartOrDie()` must have been completed.
  template <typename SocketType, typename... Args>
  absl::StatusOr<SocketType*> CreateSocket(Args&&... args) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
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

  int epoll_fd() const ABSL_LOCKS_EXCLUDED(mutex_);

  void WorkerLoop();

  absl::Mutex mutable mutex_;
  int epoll_fd_ ABSL_GUARDED_BY(mutex_) = -1;
  std::vector<std::thread> workers_;
};

template <typename SocketType, typename... Args>
absl::StatusOr<SocketType*> SelectServer::CreateSocket(Args&&... args) {
  static_assert(std::is_base_of_v<BaseSocket, SocketType>,
                "SocketType must be a subclass of BaseSocket");
  absl::ReaderMutexLock lock{&mutex_};
  if (epoll_fd_ < 0) {
    return absl::FailedPreconditionError("SelectServer hasn't started yet");
  }
  absl::StatusOr<SocketType*> const status_or_socket =
      SocketType::Create(std::forward<Args>(args)...);
  if (!status_or_socket.ok()) {
    return status_or_socket;
  }
  auto const socket = *status_or_socket;
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
  if constexpr (!SocketType::kIsListener) {
    event.events |= EPOLLOUT;
  }
  event.data.ptr = socket;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket->fd(), &event) < 0) {
    return absl::ErrnoToStatus(errno, "epoll_ctl(EPOLL_ADD) failed");
  }
  return socket;
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SELECT_SERVER_H__
