#include "common/re/automaton.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::optional<AbstractAutomaton::CaptureSet> AbstractAutomaton::PartialMatch(
    std::string_view const input) const {
  auto results = PartialMatchInternal(input, 0);
  if (results) {
    return results;
  }
  if (AssertsBegin()) {
    return std::nullopt;
  }
  for (size_t offset = 1; offset < input.size(); ++offset) {
    results = PartialMatchInternal(input, offset);
    if (results) {
      return results;
    }
  }
  return std::nullopt;
}

void AbstractAutomaton::RangeSet::CloseGroup(int const innermost_capture_group) {
  auto const it = capture_groups_->LookUp(innermost_capture_group);
  if (it != capture_groups_->root()) {
    ranges_[*it].emplace_back();
  }
}

void AbstractAutomaton::RangeSet::Capture(char const ch, int const innermost_capture_group) {
  for (auto it = capture_groups_->LookUp(innermost_capture_group); it != capture_groups_->root();
       ++it) {
    ranges_[*it].back() += ch;
  }
}

AbstractAutomaton::CaptureSet AbstractAutomaton::RangeSet::ToCaptureSet() const& {
  CaptureSet result{ranges_.size(), CaptureEntry()};
  for (size_t i = 0; i < ranges_.size(); ++i) {
    for (size_t j = 0; j < ranges_[i].size() - 1; ++j) {
      result[i].emplace_back(ranges_[i][j]);
    }
  }
  return result;
}

AbstractAutomaton::CaptureSet AbstractAutomaton::RangeSet::ToCaptureSet() && {
  CaptureSet result{ranges_.size(), CaptureEntry()};
  for (size_t i = 0; i < ranges_.size(); ++i) {
    for (size_t j = 0; j < ranges_[i].size() - 1; ++j) {
      result[i].emplace_back(std::move(ranges_[i][j]));
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
