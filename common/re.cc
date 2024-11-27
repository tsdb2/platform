#include "common/re.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
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

namespace internal {

std::string ClipString(std::string_view const text) {
  static size_t constexpr kMaxLength = 50;
  if (text.size() > kMaxLength) {
    return absl::StrCat(text.substr(0, kMaxLength), "...");
  } else {
    return std::string(text);
  }
}

}  // namespace internal

bool RE::Test(std::string_view const input, std::string_view const pattern) {
  auto const status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    return status_or_re->Test(input);
  } else {
    return false;
  }
}

bool RE::Contains(std::string_view const input, std::string_view const pattern) {
  auto const status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    return status_or_re->ContainedIn(input);
  } else {
    return false;
  }
}

absl::StatusOr<RE::CaptureSet> RE::Match(std::string_view const input,
                                         std::string_view const pattern) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(pattern));
  auto maybe_results = re.Match(input);
  if (maybe_results) {
    return std::move(maybe_results).value();
  } else {
    return absl::NotFoundError(absl::StrCat("string \"", absl::CEscape(internal::ClipString(input)),
                                            "\" doesn't match \"", absl::CEscape(pattern), "\""));
  }
}

absl::StatusOr<RE::CaptureSet> RE::PartialMatch(std::string_view const input,
                                                std::string_view const pattern) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(pattern));
  auto maybe_results = re.PartialMatch(input);
  if (maybe_results) {
    return std::move(maybe_results).value();
  } else {
    return absl::NotFoundError(absl::StrCat("no substring matching \"", absl::CEscape(pattern),
                                            "\" found in \"", absl::CEscape(input), "\""));
  }
}

absl::StatusOr<RE::CaptureSet> RE::ConsumePrefix(std::string_view *const input,
                                                 std::string_view const pattern) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(absl::StrCat("(", pattern, ")")));
  auto maybe_matches = re.MatchPrefix(*input);
  if (!maybe_matches) {
    return absl::NotFoundError(absl::StrCat("no prefix matching \"", absl::CEscape(pattern),
                                            "\" found in \"",
                                            absl::CEscape(internal::ClipString(*input)), "\""));
  }
  auto &matches = maybe_matches.value();
  size_t prefix_length = 0;
  if (!matches.empty()) {
    for (auto const &prefix : matches[0]) {
      prefix_length += prefix.size();
    }
    matches.erase(matches.begin());
  }
  input->remove_prefix(prefix_length);
  return std::move(matches);
}

absl::StatusOr<RE> RE::Create(std::string_view const pattern, Options const &options) {
  ASSIGN_VAR_OR_RETURN(reffed_ptr<regexp_internal::AbstractAutomaton>, automaton,
                       regexp_internal::Parse(pattern, options));
  return RE(std::move(automaton));
}

RE RE::CreateOrDie(std::string_view const pattern, Options const &options) {
  auto status_or_automaton = regexp_internal::Parse(pattern, options);
  CHECK_OK(status_or_automaton) << "Failed to compile regular expression \""
                                << absl::CEscape(pattern) << "\": " << status_or_automaton.status();
  return RE(std::move(status_or_automaton).value());
}

}  // namespace common
}  // namespace tsdb2
