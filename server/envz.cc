#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/env.h"
#include "common/utilities.h"
#include "http/default_server.h"
#include "http/handlers.h"
#include "http/http.h"
#include "io/buffer.h"
#include "server/module.h"

namespace {

class EnvzHandler final : public tsdb2::http::Handler {
 public:
  void operator()(tsdb2::http::StreamInterface *stream,
                  tsdb2::http::Request const &request) override;
};

std::vector<std::pair<std::string, std::string>> GetSortedEntries() {
  auto entries = tsdb2::common::Environ();
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(entries.size());
  for (auto &[key, value] : entries) {
    result.emplace_back(key, std::move(value));
  }
  std::sort(result.begin(), result.end());
  return result;
}

void EnvzHandler::operator()(tsdb2::http::StreamInterface *const stream,
                             tsdb2::http::Request const &request) {
  if (request.method != tsdb2::http::Method::kGet) {
    return stream->SendFieldsOrLog(
        {{":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k405))}},
        /*end_stream=*/true);
  }

  auto const entries = GetSortedEntries();
  std::vector<std::string> rows;
  rows.reserve(entries.size());
  for (auto const &[key, value] : entries) {
    rows.emplace_back(absl::StrCat(key, "=", value, "\n"));
  }
  std::string const content = absl::StrJoin(rows, "");

  stream->SendResponseOrLog(
      {
          {":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k200))},
          {"content-type", "text/plain"},
          {"content-disposition", "inline"},
          {"content-length", absl::StrCat(content.size())},
      },
      tsdb2::io::Buffer(content.data(), content.size()));
}

struct EnvzModule {
  static std::string_view constexpr name = "envz";  // NOLINT

  absl::Status Initialize() {  // NOLINT
    return tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
        "/envz", std::make_unique<EnvzHandler>());
  }
};

tsdb2::init::Module<
    EnvzModule, tsdb2::init::ReverseDependency<tsdb2::http::DefaultServerModule>> const envz_module;

}  // namespace
