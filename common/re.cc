#include "common/re.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/no_destructor.h"
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

absl::StatusOr<std::string> RE::StrReplaceFirst(std::string_view input, std::string_view pattern,
                                                std::string_view replacement) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(absl::StrCat("(", pattern, ")")));
  return re.StrReplaceFirst(input, 0, replacement);
}

absl::StatusOr<std::string> RE::StrReplaceAll(std::string_view input, std::string_view pattern,
                                              std::string_view replacement) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(absl::StrCat("(", pattern, ")")));
  return re.StrReplaceAll(input, 0, replacement);
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

absl::StatusOr<std::string> RE::StrReplaceFirst(std::string_view const input,
                                                size_t const capture_index,
                                                std::string_view const replacement) const {
  auto const maybe_ranges = automaton_->PartialMatchRanges(input);
  if (!maybe_ranges.has_value()) {
    return std::string(input);
  }
  auto const &ranges = maybe_ranges.value();
  if (capture_index >= ranges.size()) {
    return absl::OutOfRangeError(absl::StrCat("invalid capture index ", capture_index,
                                              ", there are only ", ranges.size(),
                                              " capture groups"));
  }
  auto const &[offset, length] = ranges[capture_index];
  if (offset < 0) {
    return absl::FailedPreconditionError(
        absl::StrCat("capture group ", capture_index, " didn't get triggered"));
  }
  DEFINE_CONST_OR_RETURN(substituted_replacement, SubstituteRefs(input, replacement, ranges));
  return absl::StrCat(input.substr(0, offset), substituted_replacement,
                      input.substr(offset + length));
}

absl::StatusOr<std::string> RE::StrReplaceAll(std::string_view input, size_t const capture_index,
                                              std::string_view const replacement) const {
  absl::Cord result;
  std::optional<regexp_internal::AbstractAutomaton::RangeSet> maybe_ranges;
  while (maybe_ranges = automaton_->PartialMatchRanges(input), maybe_ranges.has_value()) {
    auto const &ranges = maybe_ranges.value();
    if (capture_index >= ranges.size()) {
      return absl::OutOfRangeError(absl::StrCat("invalid capture index ", capture_index,
                                                ", there are only ", ranges.size(),
                                                " capture groups"));
    }
    auto const &[offset, length] = ranges[capture_index];
    if (offset < 0) {
      return absl::FailedPreconditionError(
          absl::StrCat("capture group ", capture_index, " didn't get triggered"));
    }
    DEFINE_CONST_OR_RETURN(substituted_replacement, SubstituteRefs(input, replacement, ranges));
    result.Append(input.substr(0, offset));
    result.Append(substituted_replacement);
    input.remove_prefix(offset + length);
  }
  result.Append(input);
  return std::string(std::move(result).Flatten());
}

absl::StatusOr<std::string> RE::SubstituteRefs(
    std::string_view const input, std::string_view const replacement,
    absl::Span<std::pair<intptr_t, intptr_t> const> const ranges) {
  static NoDestructor<RE> const kNumberPattern{CreateOrDie("([0-9]+)")};
  absl::Cord result;
  size_t i = 0;
  for (size_t j = 0; j < replacement.size() - 1; ++j) {
    if (replacement[j] == '\\') {
      if (i < j) {
        result.Append(replacement.substr(i, j - i));
      }
      if (replacement[++j] == '\\') {
        result.Append("\\");
        i = j + 1;
        continue;
      }
      std::string_view number;
      if (!kNumberPattern->MatchPrefixArgs(replacement.substr(j), &number)) {
        return absl::InvalidArgumentError("invalid ref");
      }
      size_t index;
      auto const [ptr, ec] = std::from_chars(number.data(), number.data() + number.size(), index);
      if (ec != std::errc{}) {
        return absl::ErrnoToStatus(tsdb2::util::to_underlying(ec), "StrReplace");
      }
      if (index >= ranges.size()) {
        return absl::OutOfRangeError(absl::StrCat("invalid ref \\", index, ", there are only ",
                                                  ranges.size(), " capturing groups"));
      }
      j += number.size() - 1;
      auto const [offset, length] = ranges[index];
      if (offset < 0) {
        return absl::FailedPreconditionError(
            absl::StrCat("capture group ", index, " didn't get triggered"));
      }
      result.Append(input.substr(offset, length));
      i = j + 1;
    }
  }
  result.Append(replacement.substr(i));
  return std::string(std::move(result).Flatten());
}

}  // namespace common
}  // namespace tsdb2
