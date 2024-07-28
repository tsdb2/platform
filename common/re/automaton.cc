#include "common/re/automaton.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::optional<AbstractAutomaton::CaptureSet> AbstractAutomaton::PartialMatch(
    std::string_view const input) const {
  auto results = PartialMatch(input, 0);
  if (results) {
    return results;
  }
  if (AssertsBegin()) {
    return std::nullopt;
  }
  for (size_t offset = 1; offset < input.size(); ++offset) {
    results = PartialMatch(input, offset);
    if (results) {
      return results;
    }
  }
  return std::nullopt;
}

void AbstractAutomaton::RangeSetCaptureManager::CloseGroup(intptr_t const offset,
                                                           int const capture_group) {
  auto const it = capture_groups_->LookUp(capture_group);
  if (it != capture_groups_->root()) {
    auto& ranges = ranges_[*it];
    auto& range = ranges.back();
    if (range.first < 0) {
      range = std::make_pair(offset, offset + 1);
    }
    ranges.emplace_back(-1, -1);
  }
}

void AbstractAutomaton::RangeSetCaptureManager::Capture(intptr_t const offset,
                                                        int const innermost_capture_group) {
  for (auto it = capture_groups_->LookUp(innermost_capture_group); it != capture_groups_->root();
       ++it) {
    auto& range = ranges_[*it].back();
    if (range.first < 0) {
      range.first = offset;
    }
    range.second = offset + 1;
  }
}

AbstractAutomaton::CaptureSet AbstractAutomaton::RangeSetCaptureManager::ToCaptureSet(
    std::string_view const source) const {
  CaptureSet result{ranges_.size(), CaptureEntry()};
  for (size_t i = 0; i < ranges_.size(); ++i) {
    for (size_t j = 0; j < ranges_[i].size() - 1; ++j) {
      auto const [start, end] = ranges_[i][j];
      result[i].emplace_back(source.substr(start, end - start));
    }
  }
  return result;
}

bool AbstractAutomaton::Assert(Assertions const assertions, std::string_view const input,
                               size_t const offset) {
  if ((assertions & Assertions::kBegin) != Assertions::kNone) {
    if (offset != 0) {
      return false;
    }
  }
  if ((assertions & Assertions::kEnd) != Assertions::kNone) {
    if (offset < input.size() - 1) {
      return false;
    }
  }
  if ((assertions & Assertions::kWordBoundary) != Assertions::kNone) {
    if (!at_word_boundary(input, offset)) {
      return false;
    }
  }
  if ((assertions & Assertions::kNotWordBoundary) != Assertions::kNone) {
    if (at_word_boundary(input, offset)) {
      return false;
    }
  }
  return true;
}

bool AbstractAutomaton::is_word_character(std::string_view const text, size_t const offset) {
  if (offset < text.size()) {
    char const ch = text[offset];
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
           (ch == '_');
  } else {
    return false;
  }
}

bool AbstractAutomaton::at_word_boundary(std::string_view const text, size_t const offset) {
  if (offset > 0) {
    return is_word_character(text, offset - 1) != is_word_character(text, offset);
  } else {
    return is_word_character(text, offset);
  }
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
