#include "http/server.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/simple_condition.h"
#include "common/utilities.h"
#include "http/channel.h"
#include "http/channel_listener.h"
#include "http/handlers.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"

namespace tsdb2 {
namespace http {

namespace {

using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::net::BaseListenerSocket;
using ::tsdb2::net::BaseSocket;
using ::tsdb2::net::kInetSocketTag;
using ::tsdb2::net::ListenerSocket;
using ::tsdb2::net::Socket;
using ::tsdb2::net::SocketOptions;
using ::tsdb2::net::SSLListenerSocket;
using ::tsdb2::net::SSLSocket;

}  // namespace

absl::StatusOr<std::unique_ptr<Server>> Server::Create(std::string_view const address,
                                                       uint16_t const port, bool const use_ssl,
                                                       SocketOptions const& options,
                                                       HandlerSet handlers) {
  auto server = absl::WrapUnique(new Server(std::move(handlers)));
  RETURN_IF_ERROR(server->Listen(address, port, use_ssl, options));
  return server;
}

Server::Binding Server::local_binding() const {
  auto const socket = listener();
  return Binding{
      .address = std::string(socket->address()),
      .port = socket->port(),
  };
}

absl::Status Server::WaitForTermination() {
  absl::MutexLock lock{&mutex_, SimpleCondition([this]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
                         return termination_status_.has_value();
                       })};
  return termination_status_.value();
}

reffed_ptr<BaseListenerSocket> Server::listener() const {
  absl::MutexLock lock{&listener_mutex_};
  return listener_;
}

void Server::RemoveChannel(BaseChannel* const channel) {
  typename ChannelSet::node_type node;
  absl::MutexLock lock{&mutex_};
  node = channels_.extract(channel);
}

absl::StatusOr<Handler*> Server::GetHandler(std::string_view const path) {
  auto const it = handlers_.find(path);
  if (it != handlers_.end()) {
    return it->second.get();
  } else {
    return absl::NotFoundError(path);
  }
}

absl::Status Server::AcceptInternal(absl::StatusOr<reffed_ptr<BaseChannel>> status_or_socket) {
  if (!status_or_socket.ok()) {
    return std::move(status_or_socket).status();
  }
  auto const& channel = status_or_socket.value();
  {
    absl::MutexLock lock{&mutex_};
    channels_.emplace(channel);
  }
  channel->StartServer();
  return absl::OkStatus();
}

void Server::AcceptCallbackImpl(absl::StatusOr<reffed_ptr<Channel<Socket>>> status_or_socket) {
  auto const status = AcceptInternal(std::move(status_or_socket));
  LOG_IF(ERROR, !status.ok()) << "Failed to accept HTTP/2 connection over TLS: " << status;
}

void Server::AcceptCallback(void* const arg,
                            absl::StatusOr<reffed_ptr<Channel<Socket>>> status_or_socket) {
  reinterpret_cast<Server*>(arg)->AcceptCallbackImpl(std::move(status_or_socket));
}

void Server::AcceptSSLCallbackImpl(
    absl::StatusOr<reffed_ptr<Channel<SSLSocket>>> status_or_socket) {
  auto const status = AcceptInternal(std::move(status_or_socket));
  LOG_IF(ERROR, !status.ok()) << "Failed to accept HTTP/2 connection: " << status;
}

void Server::AcceptSSLCallback(void* const arg,
                               absl::StatusOr<reffed_ptr<Channel<SSLSocket>>> status_or_socket) {
  reinterpret_cast<Server*>(arg)->AcceptSSLCallbackImpl(std::move(status_or_socket));
}

absl::Status Server::Listen(std::string_view const address, uint16_t const port, bool const use_ssl,
                            SocketOptions const& options) {
  absl::MutexLock lock{&listener_mutex_};
  if (use_ssl) {
    ASSIGN_OR_RETURN(listener_, ChannelListener<SSLSocket>::Create(
                                    address, port, options, &Server::AcceptSSLCallback,
                                    /*callback_arg=*/this, /*channel_manager=*/this));
  } else {
    ASSIGN_OR_RETURN(listener_, ChannelListener<Socket>::Create(
                                    kInetSocketTag, address, port, options, &Server::AcceptCallback,
                                    /*callback_arg=*/this, /*channel_manager=*/this));
  }
  return absl::OkStatus();
}

}  // namespace http
}  // namespace tsdb2
