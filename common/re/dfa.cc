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
  while (true) {
    auto const& edges = dfa_->states_[current_state_].edges;
    auto it = edges.find(ch);
    if (it != edges.end()) {
      current_state_ = it->second;
      return true;
    }
    it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    current_state_ = it->second;
  }
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

std::optional<AbstractAutomaton::CaptureSet> DFA::Match(std::string_view const input) const {
  RangeSet captures{capture_groups_};
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
      captures.Capture(offset, state.innermost_capture_group);
      ++offset;
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      captures.CloseGroup(offset, state.innermost_capture_group);
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
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      captures.CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  if (!Assert(states_[final_state_].assertions, input, offset)) {
    return std::nullopt;
  }
  return captures.ToCaptureSet(input);
}

bool DFA::AssertsBegin() const { return asserts_begin_; }

std::optional<AbstractAutomaton::CaptureSet> DFA::PartialMatchInternal(std::string_view const input,
                                                                       size_t offset) const {
  std::optional<RangeSet> result = std::nullopt;
  uint32_t state_num = initial_state_;
  RangeSet captures{capture_groups_};
  while (offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return MaybeCloseRanges(result, input);
    }
    if (state_num == final_state_) {
      result = captures;
    }
    char const ch = input[offset];
    auto it = state.edges.find(ch);
    if (it != state.edges.end()) {
      captures.Capture(offset, state.innermost_capture_group);
      ++offset;
    } else {
      it = state.edges.find(0);
      if (it == state.edges.end()) {
        return MaybeCloseRanges(result, input);
      }
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      captures.CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return MaybeCloseRanges(result, input);
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return MaybeCloseRanges(result, input);
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      captures.CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  if (!Assert(states_[final_state_].assertions, input, offset)) {
    return MaybeCloseRanges(result, input);
  }
  return captures.ToCaptureSet(input);
}

size_t DFA::GetTotalEdgeCount() const {
  size_t num_edges = 0;
  for (auto const& state : states_) {
    num_edges += state.edges.size();
  }
  return num_edges;
}

bool DFA::GetAssertsBegin() const {
  absl::flat_hash_set<uint32_t> visited;
  std::vector<uint32_t> stack{initial_state_};
  while (!stack.empty()) {
    uint32_t const state_num = stack.back();
    auto const& state = states_[state_num];
    if ((state.assertions & Assertions::kBegin) != Assertions::kNone) {
      return true;
    }
    stack.pop_back();
    visited.emplace(state_num);
    auto const it = state.edges.find(0);
    if (it != state.edges.end() && !visited.contains(it->second)) {
      stack.emplace_back(it->second);
    }
  }
  return false;
}

std::optional<AbstractAutomaton::CaptureSet> DFA::MaybeCloseRanges(
    std::optional<RangeSet> const& ranges, std::string_view const source) {
  if (ranges) {
    return ranges.value().ToCaptureSet(source);
  } else {
    return std::nullopt;
  }
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
