#include <memory>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/utilities.h"
#include "http/default_server.h"
#include "http/handlers.h"
#include "http/http.h"
#include "io/buffer.h"
#include "server/module.h"

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

struct StatuszModule {
  static std::string_view constexpr name = "statusz";  // NOLINT

  absl::Status Initialize() {  // NOLINT
    RETURN_IF_ERROR(tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
        "/statusz", std::make_unique<StatuszHandler>()));
    RETURN_IF_ERROR(tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
        "/", std::make_unique<StatuszHandler>()));
    return absl::OkStatus();
  }
};

tsdb2::init::Module<StatuszModule,
                    tsdb2::init::ReverseDependency<tsdb2::http::DefaultServerModule>> const
    statusz_module;

}  // namespace
