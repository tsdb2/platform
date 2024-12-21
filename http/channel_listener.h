#ifndef __TSDB2_HTTP_CHANNEL_LISTENER_H__
#define __TSDB2_HTTP_CHANNEL_LISTENER_H__

#include <utility>

#include "absl/status/statusor.h"
#include "common/reffed_ptr.h"
#include "http/channel.h"
#include "net/base_sockets.h"
#include "net/epoll_server.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"

namespace tsdb2 {
namespace http {

// A specialized listener socket that constructs HTTP `Channel` objects upon acceptance. The
// `SocketType` template argument can be either `tsdb2::net::Socket` or `tsdb2::net::SSLSocket`.
template <typename SocketType>
class ChannelListener;

// Specializes `ChannelListener` for raw sockets.
template <>
class ChannelListener<tsdb2::net::Socket>
    : public tsdb2::net::ListenerSocket<Channel<tsdb2::net::Socket>> {
 public:
  using Base = tsdb2::net::ListenerSocket<Channel<tsdb2::net::Socket>>;
  using typename Base::AcceptCallback;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ChannelListener>> Create(Args&&... args) {
    return tsdb2::net::EpollServer::GetInstance()->CreateSocket<ChannelListener>(
        std::forward<Args>(args)...);
  }

 protected:
  absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>> CreateSocket(
      tsdb2::net::FD fd) override {
    return Channel<tsdb2::net::Socket>::Create(std::move(fd), manager_);
  }

 private:
  friend class tsdb2::net::EpollServer;
  friend class tsdb2::net::ListenerSocket<Channel<tsdb2::net::Socket>>;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ChannelListener>> CreateInternal(
      tsdb2::net::EpollServer* const parent, Args&&... args) {
    return Base::CreateClass<ChannelListener>(parent, std::forward<Args>(args)...);
  }

  template <typename... Args>
  explicit ChannelListener(tsdb2::net::EpollServer* const parent, ChannelManager* const manager,
                           Args&&... args)
      : Base(parent, std::forward<Args>(args)...), manager_(manager) {}

  ChannelManager* const manager_;
};

// Specializes `ChannelListener` for SSL sockets.
template <>
class ChannelListener<tsdb2::net::SSLSocket>
    : public tsdb2::net::SSLListenerSocket<Channel<tsdb2::net::SSLSocket>> {
 public:
  using Base = tsdb2::net::SSLListenerSocket<Channel<tsdb2::net::SSLSocket>>;
  using typename Base::AcceptCallback;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ChannelListener>> Create(Args&&... args) {
    return tsdb2::net::EpollServer::GetInstance()->CreateSocket<ChannelListener>(
        std::forward<Args>(args)...);
  }

 protected:
  absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>> CreateSocket(
      tsdb2::net::FD fd,
      tsdb2::net::SSLSocket::ConnectCallback<Channel<tsdb2::net::SSLSocket>> callback) override {
    return Channel<tsdb2::net::SSLSocket>::Create(std::move(fd), std::move(callback), manager_);
  }

 private:
  friend class tsdb2::net::EpollServer;
  friend class tsdb2::net::SSLListenerSocket<Channel<tsdb2::net::SSLSocket>>;

  template <typename... Args>
  static absl::StatusOr<tsdb2::common::reffed_ptr<ChannelListener>> CreateInternal(
      tsdb2::net::EpollServer* const parent, Args&&... args) {
    return Base::CreateClass<ChannelListener>(parent, std::forward<Args>(args)...);
  }

  template <typename... Args>
  explicit ChannelListener(tsdb2::net::EpollServer* const parent, ChannelManager* const manager,
                           Args&&... args)
      : Base(parent, std::forward<Args>(args)...), manager_(manager) {}

  ChannelManager* const manager_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_CHANNEL_LISTENER_H__
