#include "common/re/dfa.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

size_t DFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AutomatonInterface::RunnerInterface> DFA::CreateRunner() const {
  return std::make_unique<Runner>(this);
}

bool DFA::Test(std::string_view input) const {
  size_t state_num = initial_state_;
  while (!input.empty()) {
    auto const& edges = states_[state_num].edges;
    auto it = edges.find(0);
    if (it == edges.end()) {
      char const ch = input.front();
      it = edges.find(ch);
      if (it == edges.end()) {
        return false;
      }
      input.remove_prefix(1);
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& edges = states_[state_num].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    state_num = it->second;
  }
  return true;
}

std::optional<std::vector<std::string>> DFA::Match(std::string_view input) const {
  std::vector<std::string> captures{capture_groups_.size(), std::string()};
  size_t state_num = initial_state_;
  while (!input.empty()) {
    auto const& state = states_[state_num];
    auto it = state.edges.find(0);
    if (it == state.edges.end()) {
      char const ch = input.front();
      it = state.edges.find(ch);
      if (it == state.edges.end()) {
        return std::nullopt;
      }
      for (auto it = capture_groups_.LookUp(state.innermost_capture_group);
           it != capture_groups_.root(); ++it) {
        captures[*it] += ch;
      }
      input.remove_prefix(1);
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& edges = states_[state_num].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return std::nullopt;
    }
    state_num = it->second;
  }
  return captures;
}

std::optional<std::vector<std::string>> DFA::MatchPrefix(std::string_view const input) const {
  return MatchPrefixInternal(input, initial_state_,
                             std::vector<std::string>{capture_groups_.size(), std::string()});
}

std::optional<std::vector<std::string>> DFA::MatchPrefixInternal(
    std::string_view input, size_t state_num, std::vector<std::string> captures) const {
  while (!input.empty()) {
    char const ch = input.front();
    auto const& state = states_[state_num];
    auto it = state.edges.find(ch);
    if (it != state.edges.end()) {
      for (auto it = capture_groups_.LookUp(state.innermost_capture_group);
           it != capture_groups_.root(); ++it) {
        captures[*it] += ch;
      }
      input.remove_prefix(1);
    } else {
      auto it = state.edges.find(0);
      if (it == state.edges.end()) {
        return captures;
      }
    }
    state_num = it->second;
    if (state_num == final_state_) {
      auto maybe_results = MatchPrefixInternal(input, state_num, captures);
      if (maybe_results) {
        return maybe_results;
      }
    }
  }
  while (state_num != final_state_) {
    auto const& edges = states_[state_num].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return std::nullopt;
    }
    state_num = it->second;
  }
  return captures;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
