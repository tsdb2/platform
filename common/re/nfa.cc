#include "common/re/nfa.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "common/flat_map.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::unique_ptr<AutomatonInterface::RunnerInterface> NFA::Runner::Clone() const {
  return std::make_unique<Runner>(*this);
}

bool NFA::Runner::Step(char const ch) {
  absl::flat_hash_set<size_t> next_states;
  next_states.reserve(nfa_->states_.size());
  for (auto const state : states_) {
    auto const& edges = nfa_->states_[state].edges;
    auto const it = edges.find(ch);
    if (it != edges.end()) {
      for (auto const transition : it->second) {
        next_states.emplace(transition);
      }
    }
  }
  states_ = std::move(next_states);
  if (states_.empty()) {
    return false;
  }
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

bool NFA::Runner::Finish() { return states_.contains(nfa_->final_state_); }

void NFA::Runner::EpsilonClosure() { nfa_->EpsilonClosure(&states_); }

bool NFA::IsDeterministic() const { return false; }

std::unique_ptr<AutomatonInterface::RunnerInterface> NFA::CreateRunner() const {
  return std::make_unique<Runner>(this);
}

bool NFA::Test(std::string_view input) const {
  absl::flat_hash_set<size_t> states{initial_state_};
  states.reserve(states_.size());
  EpsilonClosure(&states);
  absl::flat_hash_set<size_t> next_states;
  while (!input.empty()) {
    char const ch = input.front();
    input.remove_prefix(1);
    next_states.reserve(states_.size());
    for (auto const state : states) {
      auto const& edges = states_[state].edges;
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

std::optional<std::vector<std::string>> NFA::Match(std::string_view const input) const {
  return Matcher(*this, input).Match();
}

std::optional<std::vector<std::string>> NFA::Matcher::Match() {
  absl::flat_hash_set<size_t> epsilon_path;
  auto maybe_results = MatchInternal(&epsilon_path, nfa_.initial_state_, 0);
  if (!maybe_results) {
    return std::nullopt;
  }
  auto& results = maybe_results.value();
  if (results.empty()) {
    return std::vector<std::string>();
  }
  std::vector<std::string> captures(results.rbegin()->first + 1, std::string());
  for (auto& [capture_group, string] : results) {
    std::reverse(string.begin(), string.end());
    captures[capture_group] = std::move(string);
  }
  return captures;
}

NFA::Matcher::MatchResults NFA::Matcher::Cached(size_t const current_state_num, size_t const offset,
                                                MatchResults value) {
  auto const [it, unused_inserted] =
      cache_.try_emplace(std::make_pair(current_state_num, offset), std::move(value));
  return it->second;
}

NFA::Matcher::MatchResults NFA::Matcher::MatchInternal(
    absl::flat_hash_set<size_t>* const epsilon_path, size_t const current_state_num,
    size_t const offset) {
  if (auto const it = cache_.find(std::make_pair(current_state_num, offset)); it != cache_.end()) {
    return it->second;
  }
  auto const& current_state = nfa_.states_[current_state_num];
  if (offset >= input_.size() && current_state_num == nfa_.final_state_) {
    flat_map<size_t, std::string> results;
    for (auto it = nfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
         it != nfa_.capture_groups_.root(); ++it) {
      results.try_emplace(*it);
    }
    return Cached(current_state_num, offset, std::move(results));
  }
  if (auto const it = current_state.edges.find(0); it != current_state.edges.end()) {
    for (auto const transition : it->second) {
      auto const [unused_it, inserted] = epsilon_path->emplace(transition);
      if (inserted) {
        auto results = MatchInternal(epsilon_path, transition, offset);
        epsilon_path->erase(transition);
        if (results) {
          for (auto it = nfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
               it != nfa_.capture_groups_.root(); ++it) {
            results->try_emplace(*it);
          }
          return Cached(current_state_num, offset, std::move(results));
        }
      }
    }
  }
  if (offset >= input_.size()) {
    return Cached(current_state_num, offset, std::nullopt);
  }
  auto const ch = input_[offset];
  if (auto const it = current_state.edges.find(ch); it != current_state.edges.end()) {
    for (auto const transition : it->second) {
      absl::flat_hash_set<size_t> new_epsilon_path;
      auto results = MatchInternal(&new_epsilon_path, transition, offset + 1);
      if (results) {
        for (auto it = nfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
             it != nfa_.capture_groups_.root(); ++it) {
          (*results)[*it] += ch;
        }
        return Cached(current_state_num, offset, std::move(results));
      }
    }
  }
  return Cached(current_state_num, offset, std::nullopt);
}

void NFA::EpsilonClosure(absl::flat_hash_set<size_t>* const states) const {
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const state : *states) {
      auto const& edges = states_[state].edges;
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
