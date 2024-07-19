#include "common/re/dfa.h"

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

std::unique_ptr<AutomatonInterface::RunnerInterface> DFA::Runner::Clone() const {
  return std::make_unique<Runner>(*this);
}

bool DFA::Runner::Step(char const ch) {
  auto const& edges = dfa_->states_[current_state_].edges;
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
    auto const& edges = dfa_->states_[current_state_].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    current_state_ = it->second;
  }
  return true;
}

bool DFA::IsDeterministic() const { return true; }

std::unique_ptr<AutomatonInterface::RunnerInterface> DFA::CreateRunner() const {
  return std::make_unique<Runner>(this);
}

bool DFA::Test(std::string_view input) const {
  size_t state = initial_state_;
  while (!input.empty()) {
    auto const& edges = states_[state].edges;
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
    auto const& edges = states_[state].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    state = it->second;
  }
  return true;
}

std::optional<std::vector<std::string>> DFA::Match(std::string_view const input) const {
  return Matcher(*this, input).Match();
}

std::optional<std::vector<std::string>> DFA::Matcher::Match() {
  absl::flat_hash_set<size_t> epsilon_path;
  auto maybe_results = MatchInternal(&epsilon_path, dfa_.initial_state_, 0);
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

DFA::Matcher::MatchResults DFA::Matcher::Cached(size_t const current_state_num, size_t const offset,
                                                MatchResults value) {
  auto const [it, unused_inserted] =
      cache_.try_emplace(std::make_pair(current_state_num, offset), std::move(value));
  return it->second;
}

DFA::Matcher::MatchResults DFA::Matcher::MatchInternal(
    absl::flat_hash_set<size_t>* const epsilon_path, size_t const current_state_num,
    size_t const offset) {
  if (auto const it = cache_.find(std::make_pair(current_state_num, offset)); it != cache_.end()) {
    return it->second;
  }
  auto const& current_state = dfa_.states_[current_state_num];
  if (offset >= input_.size() && current_state_num == dfa_.final_state_) {
    flat_map<size_t, std::string> results;
    for (auto it = dfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
         it != dfa_.capture_groups_.root(); ++it) {
      results.try_emplace(*it);
    }
    return Cached(current_state_num, offset, std::move(results));
  }
  if (auto const it = current_state.edges.find(0); it != current_state.edges.end()) {
    auto const transition = it->second;
    auto const [unused_it, inserted] = epsilon_path->emplace(transition);
    if (inserted) {
      auto results = MatchInternal(epsilon_path, transition, offset);
      epsilon_path->erase(transition);
      if (results) {
        for (auto it = dfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
             it != dfa_.capture_groups_.root(); ++it) {
          results->try_emplace(*it);
        }
        return Cached(current_state_num, offset, std::move(results));
      }
    }
  }
  if (offset >= input_.size()) {
    return Cached(current_state_num, offset, std::nullopt);
  }
  auto const ch = input_[offset];
  if (auto const it = current_state.edges.find(ch); it != current_state.edges.end()) {
    absl::flat_hash_set<size_t> new_epsilon_path;
    auto results = MatchInternal(&new_epsilon_path, it->second, offset + 1);
    if (results) {
      for (auto it = dfa_.capture_groups_.LookUp(current_state.innermost_capture_group);
           it != dfa_.capture_groups_.root(); ++it) {
        (*results)[*it] += ch;
      }
      return Cached(current_state_num, offset, std::move(results));
    }
  }
  return Cached(current_state_num, offset, std::nullopt);
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
