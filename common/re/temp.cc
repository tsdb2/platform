#include "common/re/temp.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "common/re/automaton.h"
#include "common/re/dfa.h"
#include "common/re/nfa.h"
#include "common/reffed_ptr.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

namespace {

Transitions RemapTransitions(Transitions const& transitions,
                             absl::flat_hash_map<size_t, size_t> const& state_map) {
  Transitions remapped;
  remapped.reserve(transitions.size());
  for (auto const& transition : transitions) {
    remapped.emplace(state_map.at(transition));
  }
  return remapped;
}

}  // namespace

bool TempNFA::force_nfa_for_testing = false;

bool TempNFA::IsDeterministic() const {
  if (force_nfa_for_testing) {
    return false;
  }
  for (auto const& [state_num, state] : states_) {
    auto const it = state.edges.find(0);
    size_t const num_epsilon = it != state.edges.end() ? it->second.size() : 0;
    if (num_epsilon > 1) {
      return false;
    }
    bool const has_epsilon = num_epsilon > 0;
    for (auto const& [ch, transitions] : state.edges) {
      if (transitions.size() > 1 || (!transitions.empty() && has_epsilon)) {
        return false;
      }
    }
  }
  return true;
}

bool TempNFA::RenameState(size_t const old_name, size_t const new_name) {
  if (old_name == new_name) {
    return true;
  }
  auto const old_it = states_.find(old_name);
  if (old_it != states_.end()) {
    auto const new_it = states_.find(new_name);
    if (new_it != states_.end()) {
      auto const& old_state = old_it->second;
      auto const& new_state = new_it->second;
      if (old_state.innermost_capture_group != new_state.innermost_capture_group) {
        return false;
      }
    }
    auto node = states_.extract(old_it);
    MergeState(new_name, std::move(node.mapped()));
  }
  for (auto& [state_num, state] : states_) {
    for (auto& [ch, transitions] : state.edges) {
      if (transitions.erase(old_name)) {
        transitions.emplace(new_name);
      }
    }
  }
  if (initial_state_ == old_name) {
    initial_state_ = new_name;
  }
  if (final_state_ == old_name) {
    final_state_ = new_name;
  }
  return true;
}

void TempNFA::RenameAllStates(size_t* const next_state) {
  absl::flat_hash_map<size_t, size_t> state_map;
  for (auto& [state_num, state] : states_) {
    state_map.try_emplace(state_num, (*next_state)++);
  }
  state_map.try_emplace(initial_state_, (*next_state)++);
  state_map.try_emplace(final_state_, (*next_state)++);
  absl::btree_map<size_t, State> new_states;
  for (auto& [state_num, state] : states_) {
    for (auto& [ch, transitions] : state.edges) {
      transitions = RemapTransitions(transitions, state_map);
    }
    new_states.try_emplace(state_map[state_num], std::move(state));
  }
  states_ = std::move(new_states);
  initial_state_ = state_map[initial_state_];
  final_state_ = state_map[final_state_];
}

void TempNFA::AddEdge(char const label, size_t const from, size_t const to) {
  states_.at(from).edges[label].emplace(to);
}

void TempNFA::Chain(TempNFA other) {
  for (auto& [state_num, state] : other.states_) {
    MergeState(state_num, std::move(state));
  }
  MaybeAddEpsilonEdge(final_state_, other.initial_state_);
  final_state_ = other.final_state_;
}

void TempNFA::Merge(TempNFA&& other, int const capture_group, size_t const initial_state,
                    size_t const final_state) {
  for (auto& [state_num, state] : other.states_) {
    MergeState(state_num, std::move(state));
  }
  states_.try_emplace(initial_state, capture_group,
                      State::Edges{{0, {initial_state_, other.initial_state_}}});
  states_.try_emplace(final_state, capture_group, State::Edges{});
  MaybeAddEpsilonEdge(final_state_, final_state);
  MaybeAddEpsilonEdge(other.final_state_, final_state);
  initial_state_ = initial_state;
  final_state_ = final_state;
}

reffed_ptr<AutomatonInterface> TempNFA::Finalize(CaptureGroups capture_groups) && {
  CollapseEpsilonMoves();
  if (IsDeterministic()) {
    return std::move(*this).ToDFA(std::move(capture_groups));
  } else {
    return std::move(*this).ToNFA(std::move(capture_groups));
  }
}

void TempNFA::MergeState(size_t const state_num, State&& new_state) {
  auto const [it, inserted] = states_.try_emplace(state_num, std::move(new_state));
  if (!inserted) {
    auto& old_state = it->second;
    for (auto const& [ch, new_edges] : new_state.edges) {
      auto& old_edges = old_state.edges[ch];
      old_edges.insert(new_edges.begin(), new_edges.end());
    }
  }
}

namespace {

// True iff the given state has exactly one outbound edge towards a single destination state, and
// that edge is epsilon-labeled. In that case `CollapseNextEpsilonMove` will collpase it into the
// destination state.
bool HasOnlyOneEpsilonMove(State const& state) {
  auto const it = state.edges.find(0);
  if (it == state.edges.end() || it->second.size() != 1) {
    return false;
  }
  for (auto const& [ch, transitions] : state.edges) {
    if (ch != 0 && !transitions.empty()) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool TempNFA::CollapseNextEpsilonMove() {
  for (auto& [state_num, state] : states_) {
    if (HasOnlyOneEpsilonMove(state)) {
      auto const it = state.edges.find(0);
      size_t const destination = *(it->second.begin());
      if (state_num == destination || state_num != final_state_) {
        auto node = state.edges.extract(it);
        if (RenameState(destination, state_num)) {
          return true;
        } else {
          state.edges.insert(std::move(node));
        }
      }
    }
  }
  return false;
}

void TempNFA::CollapseEpsilonMoves() {
  while (CollapseNextEpsilonMove()) {
    // nothing to do.
  }
}

reffed_ptr<DFA> TempNFA::ToDFA(CaptureGroups capture_groups) && {
  absl::flat_hash_map<size_t, size_t> state_map;
  DFA::States dfa_states;
  dfa_states.reserve(states_.size());
  size_t next_state = 0;
  for (auto const& [state_num, state] : states_) {
    state_map.try_emplace(state_num, next_state++);
    DFA::State dfa_state{state.innermost_capture_group, {}};
    for (auto const& [ch, transitions] : state.edges) {
      dfa_state.edges.try_emplace(ch, *transitions.begin());
    }
    dfa_states.emplace_back(std::move(dfa_state));
  }
  state_map.try_emplace(initial_state_, next_state++);
  state_map.try_emplace(final_state_, next_state++);
  for (auto& state : dfa_states) {
    for (auto& [ch, transition] : state.edges) {
      transition = state_map[transition];
    }
  }
  return MakeReffed<DFA>(std::move(dfa_states), state_map[initial_state_], state_map[final_state_],
                         std::move(capture_groups));
}

reffed_ptr<NFA> TempNFA::ToNFA(CaptureGroups capture_groups) && {
  absl::flat_hash_map<size_t, size_t> state_map;
  NFA::States nfa_states;
  nfa_states.reserve(states_.size());
  size_t next_state = 0;
  for (auto& [state_num, state] : states_) {
    state_map.try_emplace(state_num, next_state++);
    nfa_states.emplace_back(std::move(state));
  }
  state_map.try_emplace(initial_state_, next_state++);
  state_map.try_emplace(final_state_, next_state++);
  for (auto& state : nfa_states) {
    for (auto& [ch, transitions] : state.edges) {
      transitions = RemapTransitions(transitions, state_map);
    }
  }
  return MakeReffed<NFA>(std::move(nfa_states), state_map[initial_state_], state_map[final_state_],
                         std::move(capture_groups));
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
