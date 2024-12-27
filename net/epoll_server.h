#ifndef __TSDB2_NET_EPOLL_SERVER_H__
#define __TSDB2_NET_EPOLL_SERVER_H__

#include <errno.h>
#include <sys/epoll.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"
#include "io/fd.h"

namespace tsdb2 {
namespace net {

using ::tsdb2::io::FD;

class EpollServer;

// Abstract base class for all socket types, including listeners.
class EpollTarget : public tsdb2::common::RefCounted {
 public:
  ~EpollTarget() override = default;

  int initial_fd() const { return initial_fd_; }
  size_t hash() const { return hash_; }

  bool is_open() const ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  friend class EpollServer;

  explicit EpollTarget(EpollServer* const parent, FD fd)
      : parent_(parent), initial_fd_(*fd), hash_(absl::HashOf(*fd)), fd_(std::move(fd)) {}

  EpollServer* parent() const { return parent_; }

  // Closes the file descriptor and removes the target from the `EpollServer`.
  void KillSocket() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OnLastUnref() override ABSL_LOCKS_EXCLUDED(mutex_);

  virtual void OnError() = 0;
  virtual void OnInput() = 0;
  virtual void OnOutput() = 0;

 private:
  EpollTarget(EpollTarget const&) = delete;
  EpollTarget& operator=(EpollTarget const&) = delete;
  EpollTarget(EpollTarget&&) = delete;
  EpollTarget& operator=(EpollTarget&&) = delete;

  EpollServer* const parent_;
  int const initial_fd_;
  size_t const hash_;

 protected:
  absl::Mutex mutable mutex_;
  FD fd_ ABSL_GUARDED_BY(mutex_);
};

// `EpollServer` is a singleton that manages a pool of worker threads listening to I/O events on all
// the sockets in the process. All sockets must be created through this class.
//
// The number of worker threads is configured in the --num_io_workers command line flag.
//
// The implementation uses epoll in edge-triggered mode in order to achieve the highest performance
// and parallelism.
class EpollServer final {
 public:
  // Returns the singleton instance of `EpollServer`.
  static EpollServer* GetInstance();

  // Creates a socket and adds it to the `EpollServer`, registering the corresponding file
  // descriptor in the underlying epoll.
  //
  // `SocketType` must inherit `EpollTarget` directly or indirectly and must provide a static member
  // function with the following signature:
  //
  //   static absl::StatusOr<reffed_ptr<SocketType>> CreateInternal(EpollServer *parent, ...);
  //
  // The first argument of `CreateInternal` must be a pointer to the parent `EpollServer`. In
  // addition to that, `CreateInternal` can take any number of arguments of any type, and
  // `EpollServer::CreateSocket` will perfect-forward them.
  template <typename SocketType, typename... Args,
            std::enable_if_t<std::is_base_of_v<EpollTarget, SocketType>, bool> = true>
  absl::StatusOr<tsdb2::common::reffed_ptr<SocketType>> CreateSocket(Args&&... args);

  // Creates a pair of connected sockets using the `socketpair` syscall, adding them to the epoll
  // server.
  //
  // `SocketType` must inherit `EpollTarget` directly or indirectly and must provide a static member
  // function with the following signature:
  //
  //   static absl::StatusOr<reffed_ptr<SocketType>, reffed_ptr<SocketType>>
  //   CreatePairInternal(EpollServer *parent);
  //
  template <typename SocketType, typename... Args,
            std::enable_if_t<std::is_base_of_v<EpollTarget, SocketType>, bool> = true>
  absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<SocketType>, tsdb2::common::reffed_ptr<SocketType>>>
  CreateSocketPair(Args&&... args) {
    return CreateHeterogeneousSocketPair<SocketType, SocketType, SocketType>(
        std::forward<Args>(args)...);
  }

  // Creates a pair of connected sockets with heterogeneous types. Same as `CreateSocketPair` except
  // that the socket classes can be different. This is useful for testing scenarios e.g. when you
  // want to test an HTTP/2 channel against a raw socket to verify the frames it writes.
  template <typename BaseSocket, typename FirstSocket, typename SecondSocket, typename... Args,
            std::enable_if_t<std::conjunction_v<std::is_base_of<EpollTarget, BaseSocket>,
                                                std::is_base_of<BaseSocket, FirstSocket>,
                                                std::is_base_of<BaseSocket, SecondSocket>>,
                             bool> = true>
  absl::StatusOr<
      std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
  CreateHeterogeneousSocketPair(Args&&... args);

  // Removes the specified socket file descriptor form epoll. No more `OnError`, `OnInput`, and
  // `OnOutput` calls will be issued on the corresponding `EpollTarget`.
  //
  // `EpollTarget` calls this when the user wants to close the socket.
  void KillSocket(int fd) ABSL_LOCKS_EXCLUDED(mutex_);

  // Removes the socket from all internal data structures. Called by `EpollTarget` when the user
  // drops the last reference to it, rendering it useless. This is the garbage collection mechanism
  // for all socket classes. When this function returns the socket can be destroyed.
  //
  // NOTE: calling `KillSocket` before `DestroySocket` is not necessary. The latter works even if
  // the socket file descriptor is still alive, although it will no longer be able to perform I/O
  // due to not being in the epoll and at that point shutting down the connection is pretty much the
  // only thing the caller can do.
  //
  // NOTE: we return a (shared) pointer to the removed socket rather than destroying it ourselves
  // because this way `DestroySocket` doesn't perform destruction directly and is therefore suitable
  // for running inside a critical section. The caller can then destroy the socket after releasing
  // the mutex, and the destructor can carry out slow operations such as shutting down the network
  // connection without causing contention.
  std::shared_ptr<EpollTarget> DestroySocket(EpollTarget const& target) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Custom hash & eq functors to index sockets in hash data structures by file descriptor number.
  struct HashEq {
    struct Hash {
      using is_transparent = void;
      size_t operator()(std::shared_ptr<EpollTarget> const& socket) const { return socket->hash(); }
      size_t operator()(EpollTarget const* const socket) const { return socket->hash(); }
      size_t operator()(int const fd) const { return absl::HashOf(fd); }
      size_t operator()(FD const& fd) const { return absl::HashOf(*fd); }
    };

    struct Eq {
      using is_transparent = void;

      static int ToFD(std::shared_ptr<EpollTarget> const& socket) { return socket->initial_fd(); }
      static int ToFD(EpollTarget const* const socket) { return socket->initial_fd(); }
      static int ToFD(int const fd) { return fd; }
      static int ToFD(FD const& fd) { return *fd; }

      template <typename LHS, typename RHS>
      bool operator()(LHS&& lhs, RHS&& rhs) const {
        return ToFD(std::forward<LHS>(lhs)) == ToFD(std::forward<RHS>(rhs));
      }
    };
  };

  using TargetSet = absl::flat_hash_set<std::shared_ptr<EpollTarget>, HashEq::Hash, HashEq::Eq>;
  using DeadTargetSet = absl::flat_hash_set<std::shared_ptr<EpollTarget>>;

  static int CreateEpoll();
  std::vector<std::thread> StartWorkers();

  explicit EpollServer() : epoll_fd_(CreateEpoll()), workers_(StartWorkers()) {}

  EpollServer(EpollServer const&) = delete;
  EpollServer& operator=(EpollServer const&) = delete;
  EpollServer(EpollServer&&) = delete;
  EpollServer& operator=(EpollServer&&) = delete;

  // `EpollServer` must be a singleton because unblocking its workers from `epoll_wait` and stopping
  // them is too complicated (it would require firing a signal, using thread-local storage so that
  // the signal handler knows what `EpollServer` instance it's stopping, write a state machine to
  // manage all the possible states, etc.)
  //
  // Not being able to destroy an `EpollServer` is not a big deal because a server can't be very
  // useful if it's completely deaf, so we simply forbid destruction so that the workers can keep
  // running indefinitely and we avoid all the aforementioned complexities.
  ~EpollServer() = delete;

  // Adds a target to the `EpollServer`, registering its file descriptor in the underlying epoll.
  template <typename SocketType,
            std::enable_if_t<std::is_base_of_v<EpollTarget, SocketType>, bool> = true>
  absl::Status AddTarget(tsdb2::common::reffed_ptr<SocketType> const& socket)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<EpollTarget> LookupTarget(int fd) ABSL_LOCKS_EXCLUDED(mutex_);

  void WorkerLoop();

  absl::Mutex mutable mutex_;
  TargetSet targets_ ABSL_GUARDED_BY(mutex_);
  DeadTargetSet dead_targets_ ABSL_GUARDED_BY(mutex_);

  int const epoll_fd_;
  std::vector<std::thread> const workers_;
};

template <typename SocketType, typename... Args,
          std::enable_if_t<std::is_base_of_v<EpollTarget, SocketType>, bool>>
absl::StatusOr<tsdb2::common::reffed_ptr<SocketType>> EpollServer::CreateSocket(Args&&... args) {
  DEFINE_VAR_OR_RETURN(socket, SocketType::CreateInternal(this, std::forward<Args>(args)...));
  RETURN_IF_ERROR(AddTarget(socket));
  return std::move(socket);
}

template <typename BaseSocket, typename FirstSocket, typename SecondSocket, typename... Args,
          std::enable_if_t<std::conjunction_v<std::is_base_of<EpollTarget, BaseSocket>,
                                              std::is_base_of<BaseSocket, FirstSocket>,
                                              std::is_base_of<BaseSocket, SecondSocket>>,
                           bool>>
absl::StatusOr<
    std::pair<tsdb2::common::reffed_ptr<FirstSocket>, tsdb2::common::reffed_ptr<SecondSocket>>>
EpollServer::CreateHeterogeneousSocketPair(Args&&... args) {
  DEFINE_VAR_OR_RETURN(pair, (BaseSocket::template CreatePairInternal<FirstSocket, SecondSocket>(
                                 this, std::forward<Args>(args)...)));
  RETURN_IF_ERROR(AddTarget(pair.first));
  RETURN_IF_ERROR(AddTarget(pair.second));
  return std::move(pair);
}

template <typename SocketType, std::enable_if_t<std::is_base_of_v<EpollTarget, SocketType>, bool>>
absl::Status EpollServer::AddTarget(tsdb2::common::reffed_ptr<SocketType> const& socket) {
  int const fd = socket->initial_fd();
  {
    absl::MutexLock lock{&mutex_};
    EpollTarget* const target = socket.get();
    auto const [unused_it, inserted] = targets_.emplace(target);
    CHECK_EQ(inserted, true) << "internal error: duplicate file descriptor in epoll server!";
  }
  struct epoll_event event {};
  std::memset(&event, 0, sizeof(event));
  event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
  if constexpr (!SocketType::kIsListener) {
    event.events |= EPOLLOUT;
  }
  event.data.fd = fd;
  if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
    return absl::ErrnoToStatus(errno, "epoll_ctl(EPOLL_ADD)");
  }
  return absl::OkStatus();
}

struct EpollServerModule {
  static std::string_view constexpr name = "epoll";
  absl::Status Initialize();
};

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_EPOLL_SERVER_H__
