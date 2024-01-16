#include "net/http_server.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "net/fd.h"
#include "net/select_server.h"

ABSL_FLAG(std::string, local_address, "", "The local network address this server will bind to.");
ABSL_FLAG(uint16_t, port, 8080, "The local TCP/IP port this server will listen on.");

namespace tsdb2 {
namespace net {

absl::StatusOr<std::unique_ptr<HttpServer>> HttpServer::Create(std::string_view const address,
                                                               uint16_t const port) {
  auto status_or_listener =
      SelectServer::GetInstance()->CreateSocket<ListenerSocket>(address, port);
  if (status_or_listener.ok()) {
    // cannot use std::make_unique because the `HttpServer` constructor is private
    return std::unique_ptr<HttpServer>(new HttpServer(std::move(status_or_listener).value()));
  } else {
    return status_or_listener.status();
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

void HttpServer::Accept() {
  auto const status = listener_->Accept(absl::bind_front(&HttpServer::AcceptCallback, this));
  LOG_IF(ERROR, !status.ok()) << status;
}

void HttpServer::AcceptCallback(absl::StatusOr<FD> fd) {
  // TODO
  Accept();
}

}  // namespace net
}  // namespace tsdb2
