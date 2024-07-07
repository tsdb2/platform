#include "common/re/dfa.h"

#include <cstddef>
#include <string_view>

#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

bool DFA::Runner::Step(char const ch) {
  auto const& edges = dfa_->states_[current_state_];
  auto it = edges.find(0);
  if (it == edges.end()) {
    it = edges.find(ch);
    if (it == edges.end()) {
      return false;
    }
  }
  current_state_ = it->second;
  return true;
}

bool DFA::Runner::Step(std::string_view const chars) {
  for (char const ch : chars) {
    if (!Step(ch)) {
      return false;
    }
  }
  return true;
}

bool DFA::Runner::Finish() {
  while (current_state_ != dfa_->final_state_) {
    auto const& edges = dfa_->states_[current_state_];
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    current_state_ = it->second;
  }
  return true;
}

bool DFA::IsDeterministic() const { return true; }

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
