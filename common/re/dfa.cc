#include "common/re/dfa.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
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
      if (!HalfAssert(it->second, ch)) {
        return false;
      }
      current_state_ = it->second;
      last_character_ = ch;
      return true;
    }
    it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    if (!HalfAssert(it->second, ch)) {
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
    if (!HalfAssert(it->second, 0)) {
      return false;
    }
    current_state_ = it->second;
  }
  return HalfAssert(dfa_->final_state_, 0);
}

bool DFA::IsDeterministic() const { return true; }

bool DFA::AssertsBeginOfInput() const { return asserts_begin_; }

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
  return Assert(states_[final_state_].assertions, input, offset);
}

std::optional<AbstractAutomaton::CaptureSet> DFA::Match(std::string_view const input) const {
  RangeSetCaptureManager ranges{capture_groups_};
  if (MatchInternal(input, &ranges)) {
    return ranges.ToCaptureSet(input);
  } else {
    return std::nullopt;
  }
}

bool DFA::MatchArgs(std::string_view const input,
                    absl::Span<std::string_view* const> const args) const {
  if (args.empty()) {
    return Test(input);
  }
  SingleRangeCaptureManager capture_manager{capture_groups_, input, args};
  if (MatchInternal(input, &capture_manager)) {
    capture_manager.Dump();
    return true;
  } else {
    return false;
  }
}

bool DFA::PartialTest(std::string_view const input, size_t offset) const {
  uint32_t state_num = initial_state_;
  while (state_num != final_state_ && offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state.assertions, input, offset)) {
      return false;
    }
    char const ch = input[offset];
    auto it = state.edges.find(ch);
    if (it != state.edges.end()) {
      ++offset;
    } else {
      it = state.edges.find(0);
      if (it == state.edges.end()) {
        return false;
      }
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
  return Assert(states_[final_state_].assertions, input, offset);
}

std::optional<AbstractAutomaton::CaptureSet> DFA::PartialMatch(std::string_view const input,
                                                               size_t const offset) const {
  RangeSetCaptureManager ranges{capture_groups_};
  if (PartialMatchInternal(input, offset, &ranges)) {
    return ranges.ToCaptureSet(input);
  } else {
    return std::nullopt;
  }
}

bool DFA::PartialMatchArgs(std::string_view const input, size_t const offset,
                           absl::Span<std::string_view* const> const args) const {
  if (args.empty()) {
    return PartialTest(input, offset);
  }
  SingleRangeCaptureManager capture_manager{capture_groups_, input, args};
  if (PartialMatchInternal(input, offset, &capture_manager)) {
    capture_manager.Dump();
    return true;
  } else {
    return false;
  }
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

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
