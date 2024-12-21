#include "http/default_server.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "common/utilities.h"
#include "http/handlers.h"
#include "http/http.h"
#include "http/server.h"
#include "net/base_sockets.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

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

DefaultServerBuilder *DefaultServerBuilder::Get() {
  static gsl::owner<DefaultServerBuilder *> const kInstance = new DefaultServerBuilder();
  return kInstance;  // NOLINT(cppcoreguidelines-owning-memory)
}

std::unique_ptr<Server> DefaultServerBuilder::Build() {
  tsdb2::net::SocketOptions options{
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
                     absl::GetFlag(FLAGS_use_ssl), options, ExtractHandlerSet());
  CHECK_OK(status_or_server) << "Failed to create default HTTPS server: "
                             << status_or_server.status();
  auto &server = status_or_server.value();
  auto const [address, port] = server->local_binding();
  LOG(INFO) << "Listening on " << address << ":" << port;
  return std::move(server);
}

absl::Status DefaultServerBuilder::RegisterHandler(std::string_view const path,
                                                   std::unique_ptr<Handler> handler) {
  absl::MutexLock lock{&mutex_};
  auto const [unused_it, inserted] = handlers_.try_emplace(path, std::move(handler));
  if (inserted) {
    return absl::OkStatus();
  } else {
    return absl::AlreadyExistsError(
        absl::StrCat("An HTTP handler for \"", absl::CEscape(path),
                     "\" is already registered on the default server."));
  }
}

DefaultServerBuilder::HandlerSet DefaultServerBuilder::ExtractHandlerSet() {
  HandlerSet result;
  absl::MutexLock lock{&mutex_};
  result = std::move(handlers_);
  handlers_ = HandlerSet();
  return result;
}

tsdb2::common::NoDestructor<DefaultServerModule> DefaultServerModule::instance_;

DefaultServerModule::DefaultServerModule() : BaseModule("default_server_builder") {
  tsdb2::init::RegisterModule(this, HttpModule::Get());
}

}  // namespace http
}  // namespace tsdb2
