#include "net/select_server.h"

#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "net/fd.h"

namespace tsdb2 {
namespace net {

namespace {

size_t constexpr kMaxEvents = 1024;

}  // namespace

absl::Status ListenerSocket::Accept(Callback callback) {
  absl::MutexLock lock{&mutex_};
  if (callback_) {
    return absl::FailedPreconditionError("another Accept call is already in progress");
  }
  int const result = accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (result < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      return absl::ErrnoToStatus(errno, "accept4() failed");
    } else {
      callback_.emplace(std::move(callback));
      return absl::OkStatus();
    }
  } else {
    callback(FD(result));
    return absl::OkStatus();
  }
}

void ListenerSocket::OnInput() {
  absl::MutexLock lock{&mutex_};
  if (callback_) {
    int const result = accept4(*fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (result < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        (*callback_)(absl::ErrnoToStatus(errno, "accept4() failed"));
        callback_.reset();
      }
    } else {
      (*callback_)(FD(result));
      callback_.reset();
    }
  }
}

void ListenerSocket::OnOutput() {
  // Nothing to do here.
}

absl::Status SelectServer::Start() {
  absl::MutexLock lock{&mutex_};
  if (epoll_fd_ < 0) {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
      return absl::ErrnoToStatus(errno, "epoll_create1() failed");
    }
    workers_.reserve(options_.num_workers);
    for (int i = 0; i < options_.num_workers; ++i) {
      workers_.emplace_back(absl::bind_front(&SelectServer::WorkerLoop, this));
    }
  }
  return absl::OkStatus();
}

void SelectServer::Stop() {
  absl::MutexLock lock{&mutex_};
  if (epoll_fd_ < 0) {
    return;
  }
  CHECK_GE(close(epoll_fd_), 0) << "close(epoll_fd) failed, errno=" << std::strerror(errno);
  for (auto &worker : workers_) {
    worker.join();
  }
  workers_.clear();
}

int SelectServer::epoll_fd() const {
  absl::MutexLock lock{&mutex_};
  return epoll_fd_;
}

void SelectServer::WorkerLoop() {
  auto const epfd = epoll_fd();
  struct epoll_event events[kMaxEvents];
  while (true) {
    int const num_events = epoll_wait(epfd, events, kMaxEvents, 0);
    if (num_events < 0) {
      return;
    }
    for (int i = 0; i < num_events; ++i) {
      auto const socket = static_cast<BaseSocket *>(events[i].data.ptr);
      auto const revents = events[i].events;
      if ((revents & EPOLLERR) || (revents & EPOLLHUP)) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd(), nullptr);
        delete socket;
      } else if (revents & EPOLLIN) {
        socket->OnInput();
      } else if (revents & EPOLLOUT) {
        socket->OnOutput();
      }
    }
  }
}

}  // namespace net
}  // namespace tsdb2
