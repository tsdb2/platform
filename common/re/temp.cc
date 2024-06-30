#include "common/re/temp.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "common/re/automaton.h"
#include "common/re/dfa.h"
#include "common/re/nfa.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

bool TempNFA::force_nfa_for_testing = false;

bool TempNFA::IsDeterministic() const {
  if (force_nfa_for_testing) {
    return false;
  }
  for (auto const& [state, edges] : states_) {
    auto const it = edges.find(0);
    int const num_epsilon = it != edges.end() ? it->second.size() : 0;
    if (num_epsilon > 1) {
      return false;
    }
    bool const has_epsilon = num_epsilon > 0;
    for (auto const& [ch, transitions] : edges) {
      if (transitions.size() > 1 || (!transitions.empty() && has_epsilon)) {
        return false;
      }
    }
  }
  return true;
}

// TODO

std::unique_ptr<AutomatonInterface> TempNFA::Finalize() && {
  CollapseEpsilonMoves();
  if (IsDeterministic()) {
    return std::make_unique<DFA>(std::move(*this).ToDFA());
  } else {
    return std::make_unique<NFA>(std::move(*this).ToNFA());
  }
}

// TODO

bool TempNFA::HasOnlyOneEpsilonMove(size_t const state) const {
  auto const& edges = states_.find(state)->second;
  auto const it = edges.find(0);
  if (it == edges.end() || it->second.size() != 1) {
    return false;
  }
  for (auto const& [ch, transitions] : edges) {
    if (!transitions.empty()) {
      return false;
    }
  }
  return true;
}

// TODO

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
