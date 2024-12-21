#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/no_destructor.h"
#include "common/utilities.h"
#include "http/default_server.h"
#include "http/handlers.h"
#include "http/http.h"
#include "io/buffer.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

namespace {

class StatuszHandler final : public tsdb2::http::Handler {
 public:
  void operator()(tsdb2::http::StreamInterface *stream,
                  tsdb2::http::Request const &request) override;
};

void StatuszHandler::operator()(tsdb2::http::StreamInterface *const stream,
                                tsdb2::http::Request const &request) {
  if (request.method != tsdb2::http::Method::kGet) {
    return stream->SendFieldsOrLog(
        {{":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k405))}},
        /*end_stream=*/true);
  }

  static std::string_view constexpr content = R"html(
<!doctype html>
<html lang="de">
<head>
  <style>
    body {
      font-family: sans-serif;
    }
  </style>
</head>
<body>
  <h1>Es Funktioniert!</h1>
</body>
</html>
  )html";

  stream->SendResponseOrLog(
      {
          {":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k200))},
          {"content-type", "text/html; charset=utf-8"},
          {"content-length", absl::StrCat(content.size())},
      },
      tsdb2::io::Buffer(content.data(), content.size()));
}

class StatuszModule : public tsdb2::init::BaseModule {
 public:
  static StatuszModule *Get() { return instance_.Get(); }

  absl::Status Initialize() override;

 private:
  friend class tsdb2::common::NoDestructor<StatuszModule>;
  static tsdb2::common::NoDestructor<StatuszModule> instance_;

  explicit StatuszModule() : BaseModule("statusz") {
    tsdb2::init::RegisterModule(this, tsdb2::http::DefaultServerModule::Get());
  }
};

tsdb2::common::NoDestructor<StatuszModule> StatuszModule::instance_;

absl::Status StatuszModule::Initialize() {
  RETURN_IF_ERROR(tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
      "/statusz", std::make_unique<StatuszHandler>()));
  RETURN_IF_ERROR(tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
      "/", std::make_unique<StatuszHandler>()));
  return absl::OkStatus();
}

}  // namespace
