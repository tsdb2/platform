#include "common/re/automaton.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::optional<std::vector<std::string>> AutomatonInterface::PartialMatch(
    std::string_view const input) const {
  for (size_t offset = 0; offset < input.size(); ++offset) {
    auto results = MatchPrefix(input.substr(offset));
    if (results) {
      return results;
    }
  }
  return std::nullopt;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
