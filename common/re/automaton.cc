#include "common/re/automaton.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::optional<std::vector<std::string>> AbstractAutomaton::PartialMatch(
    std::string_view const input) const {
  for (size_t offset = 0; offset < input.size(); ++offset) {
    auto results = MatchPrefix(input.substr(offset));
    if (results) {
      return results;
    }
  }
  return std::nullopt;
}

bool AbstractAutomaton::Assert(Assertions const assertions, std::string_view const input,
                               size_t const offset) {
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
