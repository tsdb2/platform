#include "http/server.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/reffed_ptr.h"
#include "common/simple_condition.h"
#include "common/utilities.h"
#include "http/channel.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"

ABSL_FLAG(std::string, local_address, "", "The local network address this server will bind to.");
ABSL_FLAG(uint16_t, port, 443, "The local TCP/IP port this server will listen on.");

ABSL_FLAG(bool, use_ssl, true,
          "Whether to use SSL. If enabled, the server will look for the certificate file specified "
          "in the SSL_CERTIFICATE_PATH environment variable, the private key file specified in the "
          "SSL_PRIVATE_KEY_PATH environment variable, and a passphrase in the SSL_PASSPHRASE "
          "environment variable.");

ABSL_FLAG(bool, tcp_keep_alive, true, "Use TCP keep-alives.");

ABSL_FLAG(std::optional<absl::Duration>, tcp_keep_alive_idle, std::nullopt,
          "TCP keep-alive idle time.");

ABSL_FLAG(std::optional<absl::Duration>, tcp_keep_alive_interval, std::nullopt,
          "TCP keep-alive interval.");

ABSL_FLAG(std::optional<int>, tcp_keep_alive_count, std::nullopt, "Max. TCP keep-alive count.");

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
                                                       SocketOptions const& options) {
  auto server = absl::WrapUnique(new Server());
  RETURN_IF_ERROR(server->Listen(address, port, use_ssl, options));
  return server;
}

namespace {

gsl::owner<Server*> CreateDefaultServerOrDie() {
  SocketOptions options{
      .keep_alive = absl::GetFlag(FLAGS_tcp_keep_alive),
  };
  if (options.keep_alive) {
    auto const keep_alive_idle = absl::GetFlag(FLAGS_tcp_keep_alive_idle);
    if (keep_alive_idle) {
      options.keep_alive_params.idle = *keep_alive_idle;
    }
    auto const keep_alive_interval = absl::GetFlag(FLAGS_tcp_keep_alive_interval);
    if (keep_alive_interval) {
      options.keep_alive_params.interval = *keep_alive_interval;
    }
    auto const keep_alive_count = absl::GetFlag(FLAGS_tcp_keep_alive_count);
    if (keep_alive_count) {
      options.keep_alive_params.count = *keep_alive_count;
    }
  }
  auto status_or_server =
      Server::Create(absl::GetFlag(FLAGS_local_address), absl::GetFlag(FLAGS_port),
                     absl::GetFlag(FLAGS_use_ssl), options);
  CHECK_OK(status_or_server) << "Failed to create default HTTPS server: "
                             << status_or_server.status();
  auto* const server = status_or_server.value().release();
  auto const [address, port] = server->local_binding();
  LOG(INFO) << "Listening on " << address << ":" << port;
  return server;
}

}  // namespace

Server* Server::GetDefault() {
  static gsl::owner<Server*> const kInstance = CreateDefaultServerOrDie();
  return kInstance;  // NOLINT(cppcoreguidelines-owning-memory)
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
  LOG_IF(ERROR, !status.ok()) << "Failed to accept HTTP/2 connection: " << status;
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
    ASSIGN_OR_RETURN(listener_, SSLListenerSocket<Channel<SSLSocket>>::Create(
                                    address, port, options, &Server::AcceptSSLCallback, this));
  } else {
    ASSIGN_OR_RETURN(listener_,
                     ListenerSocket<Channel<Socket>>::Create(kInetSocketTag, address, port, options,
                                                             &Server::AcceptCallback, this));
  }
  return absl::OkStatus();
}

}  // namespace http
}  // namespace tsdb2
