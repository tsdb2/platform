#include "common/re/nfa.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

std::unique_ptr<AbstractAutomaton::AbstractStepper> NFA::Stepper::Clone() const {
  return std::make_unique<Stepper>(*this);
}

bool NFA::Stepper::Step(char const ch) {
  states_ = EpsilonClosure(std::move(states_), ch);
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
  last_character_ = ch;
  return true;
}

bool NFA::Stepper::Finish(char const next_character) const {
  return EpsilonClosure(states_, next_character).contains(nfa_->final_state_);
}

NFA::StateSet NFA::Stepper::EpsilonClosure(StateSet states, char const ch) const {
  return nfa_->EpsilonClosure(std::move(states), last_character_, ch);
}

bool NFA::IsDeterministic() const { return false; }

bool NFA::AssertsBeginOfInput() const { return asserts_begin_; }

std::pair<size_t, size_t> NFA::GetSize() const {
  return std::make_pair(states_.size(), total_edge_count_);
}

size_t NFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AbstractAutomaton::AbstractStepper> NFA::MakeStepper(
    char const previous_character) const {
  return std::make_unique<Stepper>(this, previous_character);
}

bool NFA::Test(std::string_view const input) const {
  StateSet states = EpsilonClosure({initial_state_}, input, 0);
  for (size_t offset = 0; offset < input.size() && !states.empty(); ++offset) {
    StateSet next_states;
    for (auto const state : states) {
      auto const& edges = states_[state].edges;
      auto const it = edges.find(input[offset]);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          next_states.emplace(transition);
        }
      }
    }
    states = EpsilonClosure(std::move(next_states), input, offset + 1);
  }
  return states.contains(final_state_);
}

std::optional<AbstractAutomaton::CaptureSet> NFA::Match(std::string_view const input) const {
  auto maybe_captures = MatchInternal(input, RangeSetCaptureManager(capture_groups_));
  if (maybe_captures) {
    return std::move(maybe_captures).value().ToCaptureSet(input);
  } else {
    return std::nullopt;
  }
}

bool NFA::MatchArgs(std::string_view const input,
                    absl::Span<std::string_view* const> const args) const {
  if (args.empty()) {
    return Test(input);
  }
  auto const maybe_result =
      MatchInternal(input, SingleRangeCaptureManager(capture_groups_, input, args));
  if (maybe_result.has_value()) {
    maybe_result->Dump();
    return true;
  } else {
    return false;
  }
}

std::optional<AbstractAutomaton::RangeSet> NFA::MatchRanges(std::string_view const input) const {
  auto maybe_captures = MatchInternal(input, RangeSetCaptureManager(capture_groups_));
  if (maybe_captures) {
    return std::move(maybe_captures).value().ToRanges();
  } else {
    return std::nullopt;
  }
}

bool NFA::PartialTest(std::string_view const input, size_t offset) const {
  StateSet states = EpsilonClosure({initial_state_}, input, offset);
  if (states.contains(final_state_)) {
    return true;
  }
  for (; offset < input.size() && !states.empty(); ++offset) {
    StateSet next_states;
    char const ch = input[offset];
    for (auto const state_num : states) {
      auto const& state = states_[state_num];
      if (auto const it = state.edges.find(ch); it != state.edges.end()) {
        for (auto const transition : it->second) {
          next_states.emplace(transition);
        }
      }
    }
    states = EpsilonClosure(std::move(next_states), input, offset + 1);
    if (states.contains(final_state_)) {
      return true;
    }
  }
  return false;
}

std::optional<AbstractAutomaton::CaptureSet> NFA::PartialMatch(std::string_view const input,
                                                               size_t const offset) const {
  auto maybe_captures =
      PartialMatchInternal(input, offset, RangeSetCaptureManager(capture_groups_));
  if (maybe_captures) {
    return std::move(maybe_captures).value().ToCaptureSet(input);
  } else {
    return std::nullopt;
  }
}

bool NFA::PartialMatchArgs(std::string_view const input, size_t const offset,
                           absl::Span<std::string_view* const> const args) const {
  if (args.empty()) {
    return PartialTest(input, offset);
  }
  auto const maybe_result =
      PartialMatchInternal(input, offset, SingleRangeCaptureManager(capture_groups_, input, args));
  if (maybe_result.has_value()) {
    maybe_result->Dump();
    return true;
  } else {
    return false;
  }
}

std::optional<AbstractAutomaton::RangeSet> NFA::PartialMatchRanges(std::string_view const input,
                                                                   size_t const offset) const {
  auto maybe_captures =
      PartialMatchInternal(input, offset, RangeSetCaptureManager(capture_groups_));
  if (maybe_captures) {
    return std::move(maybe_captures).value().ToRanges();
  } else {
    return std::nullopt;
  }
}

size_t NFA::GetTotalEdgeCount() const {
  size_t num_edges = 0;
  for (auto const& state : states_) {
    for (auto const& [ch, edges] : state.edges) {
      num_edges += edges.size();
    }
  }
  return num_edges;
}

bool NFA::GetAssertsBegin() const {
  StateSet visited;
  std::vector<uint32_t> stack{initial_state_};
  while (!stack.empty()) {
    uint32_t const state_num = stack.back();
    stack.pop_back();
    auto const& state = states_[state_num];
    if ((state.assertions & Assertions::kBegin) == Assertions::kNone) {
      for (auto const& [ch, transitions] : state.edges) {
        if (ch != 0) {
          return false;
        } else {
          visited.emplace(state_num);
          for (auto const transition : transitions) {
            if (!visited.contains(transition)) {
              stack.emplace_back(transition);
            }
          }
        }
      }
    }
  }
  return true;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
