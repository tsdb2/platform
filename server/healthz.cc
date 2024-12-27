#include "server/healthz.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "http/default_server.h"
#include "http/handlers.h"
#include "http/http.h"
#include "io/buffer.h"
#include "server/module.h"

namespace tsdb2 {
namespace server {

tsdb2::common::Singleton<Healthz> Healthz::instance{+[] { return new Healthz(); }};

void Healthz::AddCheck(CheckFn check) {
  absl::MutexLock lock{&mutex_};
  checks_.emplace_back(std::move(check));
}

absl::Status Healthz::RunChecks() {
  absl::MutexLock lock{&mutex_};
  for (auto &check : checks_) {
    RETURN_IF_ERROR(check());
  }
  return absl::OkStatus();
}

namespace {

class HealthzHandler final : public tsdb2::http::Handler {
 public:
  void operator()(tsdb2::http::StreamInterface *const stream,
                  tsdb2::http::Request const &request) override {
    if (request.method != tsdb2::http::Method::kGet) {
      return stream->SendFieldsOrLog(
          {{":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k405))}},
          /*end_stream=*/true);
    }

    absl::Status const status = Healthz::instance->RunChecks();
    std::string reply = absl::StrCat(status.ToString(), "\n");

    stream->SendResponseOrLog(
        {
            {":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k200))},
            {"content-type", "text/plain"},
            {"content-disposition", "inline"},
            {"content-length", absl::StrCat(reply.size())},
        },
        tsdb2::io::Buffer(reply.data(), reply.size()));
  }
};

}  // namespace

absl::Status
HealthzModule::Initialize() {  // NOLINT(readability-convert-member-functions-to-static)
  return tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
      "/healthz", std::make_unique<HealthzHandler>());
}

static tsdb2::init::Module<
    HealthzModule, tsdb2::init::ReverseDependency<http::DefaultServerModule>> const healthz_module;

}  // namespace server
}  // namespace tsdb2
