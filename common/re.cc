#include "common/re.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "common/re/automaton.h"
#include "common/re/parser.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

namespace {

std::string ClipString(std::string_view const text) {
  static size_t constexpr kMaxLength = 50;
  if (text.size() > kMaxLength) {
    return absl::StrCat(text.substr(0, kMaxLength), "...");
  } else {
    return std::string(text);
  }
}

}  // namespace

bool RE::Test(std::string_view const input, std::string_view const pattern) {
  auto const status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    return status_or_re->Test(input);
  } else {
    return false;
  }
}

absl::StatusOr<std::vector<std::string>> RE::Match(std::string_view const input,
                                                   std::string_view const pattern) {
  auto status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    auto maybe_results = status_or_re->Match(input);
    if (maybe_results) {
      return std::move(maybe_results).value();
    } else {
      return absl::NotFoundError(absl::StrCat("string \"", absl::CEscape(ClipString(input)),
                                              "\" doesn't match \"", absl::CEscape(pattern), "\""));
    }
  } else {
    return std::move(status_or_re).status();
  }
}

absl::StatusOr<std::vector<std::string>> RE::PartialMatch(std::string_view const input,
                                                          std::string_view const pattern) {
  auto status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    auto maybe_results = status_or_re->PartialMatch(input);
    if (maybe_results) {
      return std::move(maybe_results).value();
    } else {
      return absl::NotFoundError(absl::StrCat("no substring matching \"", absl::CEscape(pattern),
                                              "\" found in \"", absl::CEscape(input), "\""));
    }
  } else {
    return std::move(status_or_re).status();
  }
}

absl::StatusOr<std::vector<std::string>> RE::ConsumePrefix(std::string_view *const input,
                                                           std::string_view const pattern) {
  auto status_or_re = RE::Create(absl::StrCat("(", pattern, ")"));
  if (!status_or_re.ok()) {
    return std::move(status_or_re).status();
  }
  std::string_view const original_input = *input;
  auto maybe_matches = status_or_re->MatchPrefix(*input);
  if (!maybe_matches) {
    return absl::NotFoundError(absl::StrCat("no prefix matching \"", absl::CEscape(pattern),
                                            "\" found in \"",
                                            absl::CEscape(ClipString(original_input)), "\""));
  }
  auto &matches = maybe_matches.value();
  std::string const prefix = std::move(matches[0]);
  matches.erase(matches.begin());
  input->remove_prefix(prefix.size());
  return std::move(matches);
}

absl::StatusOr<RE> RE::Create(std::string_view const pattern) {
  ASSIGN_VAR_OR_RETURN(reffed_ptr<regexp_internal::AutomatonInterface>, automaton,
                       regexp_internal::Parse(pattern));
  return RE(std::move(automaton));
}

}  // namespace common
}  // namespace tsdb2
