#include "common/re/nfa.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::unique_ptr<AutomatonInterface::RunnerInterface> NFA::Runner::Clone() const {
  return std::make_unique<Runner>(*this);
}

bool NFA::Runner::Step(char const ch) {
  StateSet next_states;
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

size_t NFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AutomatonInterface::RunnerInterface> NFA::CreateRunner() const {
  return std::make_unique<Runner>(this);
}

bool NFA::Test(std::string_view input) const {
  StateSet states{initial_state_};
  EpsilonClosure(&states);
  StateSet next_states;
  while (!input.empty()) {
    char const ch = input.front();
    input.remove_prefix(1);
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
    next_states = StateSet();
  }
  return states.contains(final_state_);
}

std::optional<std::vector<std::string>> NFA::Match(std::string_view const input) const {
  return Matcher(*this, input).Match(/*prefix=*/false);
}

std::optional<std::vector<std::string>> NFA::MatchPrefix(std::string_view const input) const {
  return Matcher(*this, input).Match(/*prefix=*/true);
}

std::optional<std::vector<std::string>> NFA::Matcher::Match(bool const prefix) && {
  auto maybe_results = prefix ? MatchInternal</*prefix=*/true>(nfa_.initial_state_, 0)
                              : MatchInternal</*prefix=*/false>(nfa_.initial_state_, 0);
  if (!maybe_results) {
    return std::nullopt;
  }
  auto& results = maybe_results.value();
  std::vector<std::string> captures(nfa_.capture_groups_.size(), std::string());
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

void NFA::EpsilonClosure(StateSet* const states) const {
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
