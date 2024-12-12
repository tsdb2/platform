#include "net/epoll_server.h"

#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/utilities.h"

namespace {
#ifdef NDEBUG
uint16_t constexpr kDefaultNumIoWorkers = 10;
#else
uint16_t constexpr kDefaultNumIoWorkers = 1;
#endif
}  // namespace

ABSL_FLAG(uint16_t, num_io_workers, kDefaultNumIoWorkers, "Number of I/O worker threads.");

namespace tsdb2 {
namespace net {

namespace {

size_t constexpr kMaxEvents = 1024;

}  // namespace

bool EpollTarget::is_open() const {
  absl::MutexLock lock{&mutex_};
  return !fd_.empty();
}

void EpollTarget::KillSocket() {
  if (fd_) {
    parent_->KillSocket(*fd_);
    fd_.Close();
  }
}

void EpollTarget::OnLastUnref() {
  std::shared_ptr<EpollTarget> target;
  absl::MutexLock lock{&mutex_};
  KillSocket();
  target = parent_->DestroySocket(*this);
}

EpollServer* EpollServer::GetInstance() {
  static gsl::owner<EpollServer*> const kInstance = new EpollServer();
  return kInstance;  // NOLINT(cppcoreguidelines-owning-memory)
}

void EpollServer::KillSocket(int const fd) {
  ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  {
    absl::MutexLock lock{&mutex_};
    auto node = targets_.extract(fd);
    if (node) {
      dead_targets_.emplace(std::move(node.value()));
      return;
    }
  }
  LOG(ERROR) << "file descriptor " << fd << " not found among live sockets!";
}

std::shared_ptr<EpollTarget> EpollServer::DestroySocket(EpollTarget const& target) {
  absl::MutexLock lock{&mutex_};
  auto const it1 = dead_targets_.find(&target);
  if (it1 != dead_targets_.end() && !(*it1)->is_referenced()) {
    return std::move(dead_targets_.extract(it1).value());
  }
  auto const it2 = targets_.find(&target);
  if (it2 != targets_.end() && !(*it2)->is_referenced()) {
    return std::move(targets_.extract(it2).value());
  }
  return nullptr;
}

int EpollServer::CreateEpoll() {
  int const fd = ::epoll_create1(EPOLL_CLOEXEC);
  CHECK_GE(fd, 0) << "epoll_create1() failed, errno=" << errno;
  return fd;
}

std::vector<std::thread> EpollServer::StartWorkers() {
  auto const num_workers = absl::GetFlag(FLAGS_num_io_workers);
  CHECK_GT(num_workers, 0) << "EpollServer needs at least 1 worker, but " << num_workers
                           << " were specified in --num_io_workers";
  std::vector<std::thread> workers;
  workers.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    workers.emplace_back(absl::bind_front(&EpollServer::WorkerLoop, this));
  }
  return workers;
}

std::shared_ptr<EpollTarget> EpollServer::LookupTarget(int const fd) {
  absl::MutexLock lock{&mutex_};
  auto const it = targets_.find(fd);
  if (it != targets_.end()) {
    return *it;
  } else {
    return nullptr;
  }
}

void EpollServer::WorkerLoop() {
  struct epoll_event events[kMaxEvents];
  while (true) {
    int const num_events = ::epoll_wait(epoll_fd_, events, kMaxEvents, -1);
    if (num_events < 0) {
      CHECK_EQ(errno, EINTR) << absl::ErrnoToStatus(errno, "epoll_wait()");
      continue;
    }
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
    for (int i = 0; i < num_events; ++i) {
      auto const target = LookupTarget(events[i].data.fd);
      if (!target) {
        continue;
      }
      if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0) {
        target->OnError();
      } else {
        if ((events[i].events & EPOLLIN) != 0) {
          target->OnInput();
        }
        if ((events[i].events & EPOLLOUT) != 0) {
          target->OnOutput();
        }
      }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
  }
}

}  // namespace net
}  // namespace tsdb2
