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

bool TempNFA::force_nfa_for_testing = false;

bool TempNFA::IsDeterministic() const {
  if (force_nfa_for_testing) {
    return false;
  }
  for (auto const& [state, edges] : states_) {
    auto const it = edges.find(0);
    size_t const num_epsilon = it != edges.end() ? it->second.size() : 0;
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

void TempNFA::RenameState(size_t const old_name, size_t const new_name) {
  auto const it = states_.find(old_name);
  if (it != states_.end()) {
    auto node = states_.extract(it);
    MergeState(new_name, std::move(node.mapped()));
  }
  for (auto& [state, edges] : states_) {
    for (auto& [ch, edge] : edges) {
      for (auto& transition : edge) {
        if (transition == old_name) {
          transition = new_name;
        }
      }
    }
  }
  if (initial_state_ == old_name) {
    initial_state_ = new_name;
  }
  if (final_state_ == old_name) {
    final_state_ = new_name;
  }
}

void TempNFA::RenameAllStates(size_t* const next_state) {
  absl::flat_hash_map<size_t, size_t> state_map;
  for (auto& [state, edges] : states_) {
    state_map.try_emplace(state, (*next_state)++);
  }
  state_map.try_emplace(initial_state_, (*next_state)++);
  state_map.try_emplace(final_state_, (*next_state)++);
  absl::btree_map<size_t, State> new_states;
  for (auto& [state, edges] : states_) {
    for (auto& [ch, transitions] : edges) {
      for (auto& transition : transitions) {
        transition = state_map[transition];
      }
    }
    new_states.try_emplace(state_map[state], std::move(edges));
  }
  states_ = std::move(new_states);
  initial_state_ = state_map[initial_state_];
  final_state_ = state_map[final_state_];
}

void TempNFA::AddEdge(char const label, size_t const from, size_t const to) {
  states_[from][label].emplace_back(to);
}

void TempNFA::Chain(TempNFA other) {
  for (auto& [state, edges] : other.states_) {
    MergeState(state, std::move(edges));
  }
  AddEdge(0, final_state_, other.initial_state_);
  final_state_ = other.final_state_;
}

void TempNFA::Merge(TempNFA&& other, size_t const initial_state, size_t const final_state) {
  for (auto& [state, edges] : other.states_) {
    MergeState(state, std::move(edges));
  }
  states_.try_emplace(initial_state, State{{0, {initial_state_, other.initial_state_}}});
  states_.try_emplace(final_state, State{});
  AddEdge(0, final_state_, final_state);
  AddEdge(0, other.final_state_, final_state);
  initial_state_ = initial_state;
  final_state_ = final_state;
}

reffed_ptr<AutomatonInterface> TempNFA::Finalize() && {
  CollapseEpsilonMoves();
  if (IsDeterministic()) {
    return std::move(*this).ToDFA();
  } else {
    return std::move(*this).ToNFA();
  }
}

void TempNFA::MergeState(size_t const state_num, State&& new_state) {
  auto const [it, inserted] = states_.try_emplace(state_num, std::move(new_state));
  if (!inserted) {
    auto& old_state = it->second;
    for (auto const& [ch, new_edges] : new_state) {
      auto& old_edges = old_state[ch];
      old_edges.insert(old_edges.end(), new_edges.begin(), new_edges.end());
    }
  }
}

namespace {

// Checks whether the given state has exactly one outbound edge towards a single destination state,
// and that edge is epsilon-labeled. In that case `CollapseNextEpsilonMove` will collpase it into
// the destination state.
bool HasOnlyOneEpsilonMove(State const& state) {
  auto const it = state.find(0);
  if (it == state.end() || it->second.size() != 1) {
    return false;
  }
  for (auto const& [ch, transitions] : state) {
    if (ch != 0 && !transitions.empty()) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool TempNFA::CollapseNextEpsilonMove() {
  for (auto& [state, edges] : states_) {
    if (HasOnlyOneEpsilonMove(edges)) {
      size_t const destination = edges[0][0];
      if (state == destination || state != final_state_) {
        edges.erase(0);
        RenameState(destination, state);
        return true;
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

reffed_ptr<DFA> TempNFA::ToDFA() && {
  absl::flat_hash_map<size_t, size_t> state_map;
  DFA::States dfa_states;
  dfa_states.reserve(states_.size());
  size_t next_state = 0;
  for (auto const& [state, edges] : states_) {
    state_map.try_emplace(state, next_state++);
    DFA::State dfa_state;
    for (auto const& [ch, transitions] : edges) {
      dfa_state[ch] = transitions[0];
    }
    dfa_states.emplace_back(std::move(dfa_state));
  }
  state_map.try_emplace(initial_state_, next_state++);
  state_map.try_emplace(final_state_, next_state++);
  for (auto& state : dfa_states) {
    for (auto& [ch, transition] : state) {
      transition = state_map[transition];
    }
  }
  return MakeReffed<DFA>(std::move(dfa_states), state_map[initial_state_], state_map[final_state_]);
}

reffed_ptr<NFA> TempNFA::ToNFA() && {
  absl::flat_hash_map<size_t, size_t> state_map;
  NFA::States nfa_states;
  nfa_states.reserve(states_.size());
  size_t next_state = 0;
  for (auto& [state, edges] : states_) {
    state_map.try_emplace(state, next_state++);
    nfa_states.emplace_back(std::move(edges));
  }
  state_map.try_emplace(initial_state_, next_state++);
  state_map.try_emplace(final_state_, next_state++);
  for (auto& state : nfa_states) {
    for (auto& [ch, transitions] : state) {
      for (auto& transition : transitions) {
        transition = state_map[transition];
      }
    }
  }
  return MakeReffed<NFA>(std::move(nfa_states), state_map[initial_state_], state_map[final_state_]);
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
