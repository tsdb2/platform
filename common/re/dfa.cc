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
    auto const& states = states_[state];
    auto it = states.find(0);
    if (it == states.end()) {
      char const ch = input[0];
      it = states.find(ch);
      if (it == states.end()) {
        return false;
      }
      input.remove_prefix(1);
    }
    state = it->second;
  }
  while (state != final_state_) {
    auto const& states = states_[state];
    auto const it = states.find(0);
    if (it == states.end()) {
      return false;
    }
    state = it->second;
  }
  return true;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
