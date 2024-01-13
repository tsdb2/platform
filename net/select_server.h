#ifndef __TSDB2_NET_SELECT_SERVER_H__
#define __TSDB2_NET_SELECT_SERVER_H__

#include <errno.h>
#include <sys/epoll.h>

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
#include "net/fd.h"

namespace tsdb2 {
namespace net {

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
  using Callback = absl::AnyInvocable<void(absl::StatusOr<FD>)>;

  explicit ListenerSocket(FD fd) : BaseSocket(std::move(fd)) {}

  absl::Status Accept(Callback callback) ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  void OnInput() override ABSL_LOCKS_EXCLUDED(mutex_);
  void OnOutput() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutable mutex_;
  std::optional<Callback> callback_ ABSL_GUARDED_BY(mutex_);
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
  auto const epfd = epoll_fd();
  if (epfd < 0) {
    return absl::FailedPreconditionError("SelectServer hasn't started yet");
  }
  auto const socket = new SocketType(std::forward<Args>(args)...);
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE;
  event.data.ptr = socket;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket->fd(), &event) < 0) {
    return absl::ErrnoToStatus(errno, "epoll_ctl(EPOLL_ADD) failed");
  }
  return socket;
}

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SELECT_SERVER_H__
