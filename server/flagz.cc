#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/reflection.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/no_destructor.h"
#include "common/utilities.h"
#include "http/default_server.h"
#include "http/handlers.h"
#include "http/http.h"
#include "io/buffer.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

namespace {

class FlagzHandler final : public tsdb2::http::Handler {
 public:
  void operator()(tsdb2::http::StreamInterface *stream,
                  tsdb2::http::Request const &request) override;
};

std::vector<std::pair<std::string, std::string>> GetSortedEntries() {
  auto const flags = absl::GetAllFlags();
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(flags.size());
  for (auto const &[name, flag] : flags) {
    result.emplace_back(name, flag->CurrentValue());
  }
  std::sort(result.begin(), result.end());
  return result;
}

void FlagzHandler::operator()(tsdb2::http::StreamInterface *const stream,
                              tsdb2::http::Request const &request) {
  if (request.method != tsdb2::http::Method::kGet) {
    return stream->SendFields(
        {{":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k405))}},
        /*end_stream=*/true);
  }

  auto const entries = GetSortedEntries();
  std::vector<std::string> rows;
  rows.reserve(entries.size());
  for (auto const &[name, value] : entries) {
    rows.emplace_back(absl::StrCat("--", name, "=", value, "\n"));
  }
  std::string const content = absl::StrJoin(rows, "");

  stream->SendFields(
      {
          {":status", absl::StrCat(tsdb2::util::to_underlying(tsdb2::http::Status::k200))},
          {"content-type", "text/plain"},
          {"content-disposition", "inline"},
          {"content-length", absl::StrCat(content.size())},
      },
      /*end_stream=*/false);
  stream->SendData(tsdb2::io::Buffer(content.data(), content.size()), /*end_stream=*/true);
}

class FlagzModule : public tsdb2::init::BaseModule {
 public:
  static FlagzModule *Get() { return instance_.Get(); }

  absl::Status Initialize() override;

 private:
  friend class tsdb2::common::NoDestructor<FlagzModule>;
  static tsdb2::common::NoDestructor<FlagzModule> instance_;

  explicit FlagzModule() : BaseModule("flagz") {
    tsdb2::init::RegisterModule(this, tsdb2::http::DefaultServerModule::Get());
  }
};

tsdb2::common::NoDestructor<FlagzModule> FlagzModule::instance_;

absl::Status FlagzModule::Initialize() {
  return tsdb2::http::DefaultServerBuilder::Get()->RegisterHandler(
      "/flagz", std::make_unique<FlagzHandler>());
}

}  // namespace
