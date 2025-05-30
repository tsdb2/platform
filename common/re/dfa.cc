#include "common/re/dfa.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::unique_ptr<AbstractAutomaton::AbstractStepper> DFA::Stepper::Clone() const {
  return std::make_unique<Stepper>(*this);
}

bool DFA::Stepper::Step(char const ch) {
  if (!Assert(current_state_, ch)) {
    return false;
  }
  while (true) {
    auto const& edges = dfa_->states_[current_state_].edges;
    auto it = edges.find(ch);
    if (it != edges.end()) {
      current_state_ = it->second;
      last_character_ = ch;
      return true;
    }
    it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    current_state_ = it->second;
  }
}

bool DFA::Stepper::Finish(char const next_character) const {
  auto state_num = current_state_;
  if (!Assert(state_num, next_character)) {
    return false;
  }
  while (state_num != dfa_->final_state_) {
    auto const& edges = dfa_->states_[state_num].edges;
    auto const it = edges.find(0);
    if (it == edges.end()) {
      return false;
    }
    state_num = it->second;
    if (!Assert(state_num, next_character)) {
      return false;
    }
  }
  return true;
}

bool DFA::IsDeterministic() const { return true; }

bool DFA::AssertsBeginOfInput() const { return asserts_begin_; }

size_t DFA::GetMinMatchLength() const { return min_match_length_; }

std::pair<size_t, size_t> DFA::GetSize() const {
  return std::make_pair(states_.size(), total_edge_count_);
}

size_t DFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AbstractAutomaton::AbstractStepper> DFA::MakeStepper(
    char const previous_character) const {
  return std::make_unique<Stepper>(this, previous_character);
}

bool DFA::Test(std::string_view const input) const {
  if (input.size() < min_match_length_) {
    return false;
  }
  uint32_t state_num = initial_state_;
  size_t offset = 0;
  while (offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state, input, offset)) {
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
    if (!Assert(state, input, offset)) {
      return false;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return false;
    }
    state_num = it->second;
  }
  return Assert(final_state_, input, offset);
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

std::optional<AbstractAutomaton::RangeSet> DFA::MatchRanges(std::string_view const input) const {
  RangeSetCaptureManager ranges{capture_groups_};
  if (MatchInternal(input, &ranges)) {
    return ranges.ToRanges();
  } else {
    return std::nullopt;
  }
}

bool DFA::PartialTest(std::string_view const input, size_t offset) const {
  if (input.size() < offset + min_match_length_) {
    return false;
  }
  uint32_t state_num = initial_state_;
  while (state_num != final_state_ && offset < input.size()) {
    auto const& state = states_[state_num];
    if (!Assert(state, input, offset)) {
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
    if (!Assert(state, input, offset)) {
      return false;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return false;
    }
    state_num = it->second;
  }
  return Assert(final_state_, input, offset);
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

std::optional<AbstractAutomaton::RangeSet> DFA::PartialMatchRanges(std::string_view const input,
                                                                   size_t const offset) const {
  RangeSetCaptureManager ranges{capture_groups_};
  if (PartialMatchInternal(input, offset, &ranges)) {
    return ranges.ToRanges();
  } else {
    return std::nullopt;
  }
}

size_t DFA::GetTotalEdgeCount() const {
  size_t num_edges = 0;
  for (auto const& state : states_) {
    num_edges += state.edges.size();
  }
  return num_edges;
}

size_t DFA::InferMinMatchLength() const {
  static size_t constexpr kInfinity = std::numeric_limits<size_t>::max();
  std::vector<size_t> distances(states_.size(), kInfinity);
  distances[initial_state_] = 0;
  // The first component of the pairs in the queue is the distance from the initial state, the
  // second component is the state number.
  std::priority_queue<std::pair<size_t, uint32_t>> queue;
  queue.push(std::make_pair(0, initial_state_));
  while (!queue.empty()) {
    auto const [distance, state_num] = queue.top();
    queue.pop();
    if (distance > distances[state_num]) {
      continue;
    }
    auto const& state = states_[state_num];
    for (auto const [ch, neighbor] : state.edges) {
      size_t const cost = distances[state_num] + static_cast<size_t>(ch != 0);
      if (cost < distances[neighbor]) {
        distances[neighbor] = cost;
        queue.push(std::make_pair(cost, neighbor));
      }
    }
  }
  auto const length = distances[final_state_];
  if (length < kInfinity) {
    return length;
  } else {
    return 0;
  }
}

bool DFA::GetAssertsBegin() const {
  absl::flat_hash_set<uint32_t> visited;
  uint32_t state_num = initial_state_;
  while (!visited.contains(state_num)) {
    auto const& state = states_[state_num];
    if ((state.assertions & Assertions::kBegin) != Assertions::kNone) {
      return true;
    }
    visited.emplace(state_num);
    auto const it = state.edges.find(0);
    if (it != state.edges.end()) {
      state_num = it->second;
    }
  }
  return false;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
