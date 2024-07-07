#include "common/re/nfa.h"

#include <cstddef>
#include <string_view>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

bool NFA::Runner::Step(char const ch) {
  absl::flat_hash_set<size_t> next_states;
  next_states.reserve(nfa_->states_.size());
  for (auto const state : states_) {
    auto const& edges = nfa_->states_[state];
    auto const it = edges.find(ch);
    if (it != edges.end()) {
      for (auto const transition : it->second) {
        next_states.emplace(transition);
      }
    }
  }
  if (next_states.empty()) {
    return false;
  }
  states_ = std::move(next_states);
  EpsilonClosure();
  return true;
}

bool NFA::Runner::Step(std::string_view const chars) {
  for (char const ch : chars) {
    if (!Step(ch)) {
      return false;
    }
  }
  return true;
}

bool NFA::Runner::Finish() const { return states_.contains(nfa_->final_state_); }

void NFA::Runner::EpsilonClosure() { nfa_->EpsilonClosure(&states_); }

bool NFA::IsDeterministic() const { return false; }

bool NFA::Run(std::string_view input) const {
  absl::flat_hash_set<size_t> states{initial_state_};
  states.reserve(states_.size());
  EpsilonClosure(&states);
  absl::flat_hash_set<size_t> next_states;
  while (!input.empty()) {
    char const ch = input.front();
    input.remove_prefix(1);
    next_states.reserve(states_.size());
    for (auto const state : states) {
      auto const& edges = states_[state];
      auto const it = edges.find(ch);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          next_states.emplace(transition);
        }
      }
    }
    if (next_states.empty()) {
      return false;
    }
    EpsilonClosure(&next_states);
    states = std::move(next_states);
    next_states = absl::flat_hash_set<size_t>();
  }
  return states.contains(final_state_);
}

void NFA::EpsilonClosure(absl::flat_hash_set<size_t>* const states) const {
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const state : *states) {
      auto const& edges = states_[state];
      auto const it = edges.find(0);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          auto const [it, inserted] = states->emplace(transition);
          new_state_found |= inserted;
        }
        if (new_state_found) {
          break;
        }
      }
    }
  } while (new_state_found);
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
