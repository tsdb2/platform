#ifndef __TSDB2_NET_SELECT_SERVER_H__
#define __TSDB2_NET_SELECT_SERVER_H__

#include <errno.h>
#include <sys/epoll.h>

#include <cstddef>
#include <cstdint>
#include <optional>
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

  absl::Status Accept(Callback callback) ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class SelectServer;

  explicit ListenerSocket(FD fd) : BaseSocket(std::move(fd)) {}

  absl::Mutex mutable mutex_;
  std::optional<Callback> callback_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
};

class Socket : public BaseSocket {
 public:
  using ReadCallback = absl::AnyInvocable<void(absl::StatusOr<Buffer> status_or_buffer)>;
  using WriteCallback = absl::AnyInvocable<void(absl::Status status)>;

  static bool constexpr kIsListener = false;

  absl::Status Read(size_t length, ReadCallback callback) ABSL_LOCKS_EXCLUDED(read_mutex_);

  absl::Status Write(Buffer const& buffer, WriteCallback callback)
      ABSL_LOCKS_EXCLUDED(write_mutex_);

 protected:
  void OnInput() override ABSL_LOCKS_EXCLUDED(read_mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(write_mutex_);

 private:
  struct ReadStatus {
    explicit ReadStatus(Buffer buffer, size_t const remaining, ReadCallback callback)
        : buffer(std::move(buffer)), remaining(remaining), callback(std::move(callback)) {}

    Buffer buffer;
    size_t remaining;
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

  explicit Socket(FD fd) : BaseSocket(std::move(fd)) {}

  absl::Mutex mutable read_mutex_;
  std::optional<ReadStatus> read_status_ ABSL_GUARDED_BY(read_mutex_) = std::nullopt;

  absl::Mutex mutable write_mutex_;
  std::optional<WriteStatus> write_status_ ABSL_GUARDED_BY(write_mutex_) = std::nullopt;
};

class SelectServer {
 public:
  struct Options {
    uint16_t num_workers = 1;
    bool start_now = false;
  };

  explicit SelectServer(Options options) : options_(std::move(options)) {
    CHECK_GT(options_.num_workers, 0) << "SelectServer needs to have at least 1 worker";
    if (options_.start_now) {
      StartOrDie();
    }
  }

  ~SelectServer() { Stop(); }

  absl::Status Start() ABSL_LOCKS_EXCLUDED(mutex_);

  void StartOrDie() { CHECK_OK(Start()) << "SelectServer failed to start"; }

  void Stop() ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename SocketType, typename... Args>
  absl::StatusOr<SocketType*> CreateSocket(Args&&... args) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  int epoll_fd() const ABSL_LOCKS_EXCLUDED(mutex_);

  void WorkerLoop();

  Options const options_;

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
  auto const socket = new SocketType(std::forward<Args>(args)...);
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
