#include "common/re/automaton.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/types/span.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

bool AbstractAutomaton::AbstractStepper::Step(std::string_view const chars) {
  for (char const ch : chars) {
    if (!Step(ch)) {
      return false;
    }
  }
  return true;
}

bool AbstractAutomaton::PartialTest(std::string_view const input) const {
  auto const min_length = GetMinMatchLength();
  if (input.size() < min_length) {
    return false;
  }
  if (PartialTest(input, 0)) {
    return true;
  }
  if (AssertsBeginOfInput()) {
    return false;
  }
  for (size_t offset = 1; offset <= input.size() - min_length; ++offset) {
    if (PartialTest(input, offset)) {
      return true;
    }
  }
  return false;
}

std::optional<AbstractAutomaton::CaptureSet> AbstractAutomaton::PartialMatch(
    std::string_view const input) const {
  auto const min_length = GetMinMatchLength();
  if (input.size() < min_length) {
    return std::nullopt;
  }
  auto results = PartialMatch(input, 0);
  if (results) {
    return results;
  }
  if (AssertsBeginOfInput()) {
    return std::nullopt;
  }
  for (size_t offset = 1; offset <= input.size() - min_length; ++offset) {
    results = PartialMatch(input, offset);
    if (results) {
      return results;
    }
  }
  return std::nullopt;
}

bool AbstractAutomaton::PartialMatchArgs(std::string_view const input,
                                         absl::Span<std::string_view* const> const args) const {
  auto const min_length = GetMinMatchLength();
  if (input.size() < min_length) {
    return false;
  }
  if (PartialMatchArgs(input, 0, args)) {
    return true;
  }
  if (AssertsBeginOfInput()) {
    return false;
  }
  for (size_t offset = 1; offset <= input.size() - min_length; ++offset) {
    if (PartialMatchArgs(input, offset, args)) {
      return true;
    }
  }
  return false;
}

std::optional<AbstractAutomaton::RangeSet> AbstractAutomaton::PartialMatchRanges(
    std::string_view const input) const {
  auto const min_length = GetMinMatchLength();
  if (input.size() < min_length) {
    return std::nullopt;
  }
  auto results = PartialMatchRanges(input, 0);
  if (results) {
    return results;
  }
  if (AssertsBeginOfInput()) {
    return std::nullopt;
  }
  for (size_t offset = 1; offset <= input.size() - min_length; ++offset) {
    results = PartialMatchRanges(input, offset);
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
      range = std::make_pair(offset, offset);
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

AbstractAutomaton::RangeSet AbstractAutomaton::RangeSetCaptureManager::ToRanges() const {
  RangeSet ranges;
  ranges.reserve(ranges_.size());
  for (size_t i = 0; i < ranges_.size(); ++i) {
    auto const count = ranges_[i].size();
    if (count < 2) {
      ranges.emplace_back(-1, -1);
    } else {
      auto const [start, end] = ranges_[i][count - 2];
      ranges.emplace_back(start, end - start);
    }
  }
  return ranges;
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

void AbstractAutomaton::SingleRangeCaptureManager::CloseGroup(intptr_t const offset,
                                                              int const capture_group) {
  auto const it = capture_groups_->LookUp(capture_group);
  if (it != capture_groups_->root()) {
    auto const index = *it;
    if (index < ranges_.size()) {
      auto& [closed_string, begin, end] = ranges_[index];
      if (begin < 0) {
        closed_string = "";
      } else {
        closed_string = source_.substr(begin, end - begin);
      }
      begin = -1;
      end = -1;
    }
  }
}

void AbstractAutomaton::SingleRangeCaptureManager::Capture(intptr_t const offset,
                                                           int const innermost_capture_group) {
  for (auto it = capture_groups_->LookUp(innermost_capture_group); it != capture_groups_->root();
       ++it) {
    auto const index = *it;
    if (index < ranges_.size()) {
      auto& [unused, begin, end] = ranges_[index];
      if (begin < 0) {
        begin = offset;
      }
      end = offset + 1;
    }
  }
}

void AbstractAutomaton::SingleRangeCaptureManager::Dump() const {
  for (size_t i = 0; i < ranges_.size(); ++i) {
    *args_[i] = ranges_[i].closed_string;
  }
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

bool AbstractAutomaton::Assert(Assertions const assertions, char const ch1, char const ch2) {
  if ((assertions & Assertions::kBegin) != Assertions::kNone) {
    if (ch1 != 0) {
      return false;
    }
  }
  if ((assertions & Assertions::kEnd) != Assertions::kNone) {
    if (ch2 != 0) {
      return false;
    }
  }
  if ((assertions & Assertions::kWordBoundary) != Assertions::kNone) {
    if (!at_word_boundary(ch1, ch2)) {
      return false;
    }
  }
  if ((assertions & Assertions::kNotWordBoundary) != Assertions::kNone) {
    if (at_word_boundary(ch1, ch2)) {
      return false;
    }
  }
  return true;
}

bool AbstractAutomaton::is_word_character(char const ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
         (ch == '_');
}

bool AbstractAutomaton::is_word_character(std::string_view const text, size_t const offset) {
  if (offset < text.size()) {
    return is_word_character(text[offset]);
  } else {
    return false;
  }
}

bool AbstractAutomaton::at_word_boundary(char const ch1, char const ch2) {
  return is_word_character(ch1) != is_word_character(ch2);
}

bool AbstractAutomaton::at_word_boundary(std::string_view const text, size_t const offset) {
  if (offset > 0) {
    return is_word_character(text, offset - 1) != is_word_character(text, offset);
  } else {
    return is_word_character(text[0]);
  }
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
