#include "common/re/dfa.h"

#include <cstddef>
#include <cstdint>
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

std::unique_ptr<AbstractAutomaton::StepperInterface> DFA::Stepper::Clone() const {
  return std::make_unique<Stepper>(*this);
}

bool DFA::Stepper::Step(char const ch) {
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

bool DFA::Stepper::Step(std::string_view const chars) {
  for (char const ch : chars) {
    if (!Step(ch)) {
      return false;
    }
  }
  return true;
}

bool DFA::Stepper::Finish() {
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

std::pair<size_t, size_t> DFA::GetSize() const {
  return std::make_pair(states_.size(), total_edge_count_);
}

size_t DFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AbstractAutomaton::StepperInterface> DFA::MakeStepper() const {
  return std::make_unique<Stepper>(this);
}

bool DFA::Test(std::string_view const input) const {
  uint32_t state_num = initial_state_;
  size_t offset = 0;
  while (offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return false;
    }
    auto it = state.edges.find(0);
    if (it == state.edges.end()) {
      it = state.edges.find(input[offset]);
      if (it == state.edges.end()) {
        return false;
      }
      ++offset;
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return false;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return false;
    }
    state_num = it->second;
  }
  if (!Assert(states_[final_state_].assertions, input, offset)) {
    return false;
  }
  return true;
}

std::optional<std::vector<std::string>> DFA::Match(std::string_view const input) const {
  std::vector<std::string> captures{capture_groups_.size(), std::string()};
  uint32_t state_num = initial_state_;
  size_t offset = 0;
  while (offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return std::nullopt;
    }
    auto it = state.edges.find(0);
    if (it == state.edges.end()) {
      char const ch = input[offset];
      it = state.edges.find(ch);
      if (it == state.edges.end()) {
        return std::nullopt;
      }
      for (auto it = capture_groups_.LookUp(state.innermost_capture_group);
           it != capture_groups_.root(); ++it) {
        captures[*it] += ch;
      }
      ++offset;
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return std::nullopt;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return std::nullopt;
    }
    state_num = it->second;
  }
  if (!Assert(states_[final_state_].assertions, input, offset)) {
    return std::nullopt;
  }
  return captures;
}

bool DFA::AssertsBegin() const { return asserts_begin_; }

std::optional<std::vector<std::string>> DFA::PartialMatchInternal(std::string_view const input,
                                                                  size_t offset) const {
  std::optional<std::vector<std::string>> result = std::nullopt;
  uint32_t state_num = initial_state_;
  std::vector<std::string> captures{capture_groups_.size(), std::string()};
  while (offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return result;
    }
    if (state_num == final_state_) {
      result = captures;
    }
    char const ch = input[offset];
    auto it = state.edges.find(ch);
    if (it != state.edges.end()) {
      for (auto it = capture_groups_.LookUp(state.innermost_capture_group);
           it != capture_groups_.root(); ++it) {
        captures[*it] += ch;
      }
      ++offset;
    } else {
      it = state.edges.find(0);
      if (it == state.edges.end()) {
        return result;
      }
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return result;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return result;
    }
    state_num = it->second;
  }
  if (!Assert(states_[final_state_].assertions, input, offset)) {
    return result;
  }
  return captures;
}

size_t DFA::GetTotalEdgeCount() const {
  size_t num_edges = 0;
  for (auto const& state : states_) {
    num_edges += state.edges.size();
  }
  return num_edges;
}

bool DFA::GetAssertsBegin() const {
  if ((states_[initial_state_].assertions & Assertions::kBegin) != Assertions::kNone) {
    return true;
  }
  absl::flat_hash_set<uint32_t> states{initial_state_};
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const state_num : states) {
      auto const& edges = states_[state_num].edges;
      auto const it = edges.find(0);
      if (it != edges.end()) {
        auto const transition = it->second;
        if (!states.contains(transition)) {
          new_state_found = true;
          auto const& destination = states_[transition];
          if ((destination.assertions & Assertions::kBegin) != Assertions::kNone) {
            return true;
          }
          states.emplace(transition);
        }
      }
    }
  } while (new_state_found);
  return false;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
