#include "net/http_server.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/reffed_ptr.h"
#include "net/sockets.h"

ABSL_FLAG(std::string, local_address, "", "The local network address this server will bind to.");
ABSL_FLAG(uint16_t, port, 8080, "The local TCP/IP port this server will listen on.");

ABSL_FLAG(bool, tcp_keep_alive, true, "Use TCP keep-alives.");

ABSL_FLAG(std::optional<absl::Duration>, tcp_keep_alive_idle, std::nullopt,
          "TCP keep-alive idle time.");

ABSL_FLAG(std::optional<absl::Duration>, tcp_keep_alive_interval, std::nullopt,
          "TCP keep-alive interval.");

ABSL_FLAG(std::optional<int>, tcp_keep_alive_count, std::nullopt, "Max. TCP keep-alive count.");

namespace tsdb2 {
namespace net {

namespace {

using ::tsdb2::common::reffed_ptr;

}  // namespace

absl::StatusOr<std::unique_ptr<HttpServer>> HttpServer::Create(std::string_view const address,
                                                               uint16_t const port) {
  std::unique_ptr<HttpServer> server{new HttpServer()};
  auto status = server->Listen(address, port);
  if (status.ok()) {
    return server;
  } else {
    return status;
  }
}

namespace {

HttpServer* CreateDefaultServerOrDie() {
  auto status_or_server =
      HttpServer::Create(absl::GetFlag(FLAGS_local_address), absl::GetFlag(FLAGS_port));
  CHECK_OK(status_or_server) << "Failed to create default HTTP server: "
                             << status_or_server.status();
  auto const server = status_or_server.value().release();
  LOG(INFO) << "Listening on " << server->local_address() << ":" << server->port();
  return server;
}

}  // namespace

HttpServer* HttpServer::GetDefault() {
  static HttpServer* const kInstance = CreateDefaultServerOrDie();
  return kInstance;
}

absl::Status HttpServer::Listen(std::string_view const address, uint16_t const port) {
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
  auto status_or_listener = SelectServer::GetInstance()->CreateSocket<ListenerSocket>(
      ListenerSocket::kInetSocketTag, address, port, options,
      absl::bind_front(&HttpServer::AcceptCallback, this));
  if (status_or_listener.ok()) {
    listener_ = std::move(status_or_listener).value();
    return absl::OkStatus();
  } else {
    return status_or_listener.status();
  }
}

void HttpServer::AcceptCallback(absl::StatusOr<reffed_ptr<Socket>> status_or_socket) {
  // TODO
}

}  // namespace net
}  // namespace tsdb2
