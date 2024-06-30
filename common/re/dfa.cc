#include "common/re/dfa.h"

#include <cstddef>
#include <memory>
#include <string_view>

#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::unique_ptr<AutomatonInterface> DFA::Clone() const { return std::make_unique<DFA>(*this); }

bool DFA::Run(std::string_view input) const {
  size_t state = initial_state_;
  while (!input.empty()) {
    auto const& edges = states_[state];
    auto it = edges.find(0);
    if (it == edges.end()) {
      char const ch = input[0];
      it = edges.find(ch);
      if (it == edges.end()) {
        return false;
      }
      input.remove_prefix(1);
    }
    state = it->second;
  }
  while (state != final_state_) {
    auto const& edges = states_[state];
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    state = it->second;
  }
  return true;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
