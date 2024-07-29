#include "common/re/nfa.h"

#include <cstddef>
#include <functional>
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

std::unique_ptr<AbstractAutomaton::StepperInterface> NFA::Stepper::Clone() const {
  return std::make_unique<Stepper>(*this);
}

bool NFA::Stepper::Step(char const ch) {
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

bool NFA::Stepper::Step(std::string_view const chars) {
  for (char const ch : chars) {
    if (!Step(ch)) {
      return false;
    }
  }
  return true;
}

bool NFA::Stepper::Finish() { return states_.contains(nfa_->final_state_); }

void NFA::Stepper::EpsilonClosure() { states_ = nfa_->EpsilonClosure(std::move(states_)); }

bool NFA::IsDeterministic() const { return false; }

std::pair<size_t, size_t> NFA::GetSize() const {
  return std::make_pair(states_.size(), total_edge_count_);
}

size_t NFA::GetNumCaptureGroups() const { return capture_groups_.size(); }

std::unique_ptr<AbstractAutomaton::StepperInterface> NFA::MakeStepper() const {
  return std::make_unique<Stepper>(this);
}

bool NFA::Test(std::string_view const input) const {
  StateSet states = AssertedEpsilonClosure({initial_state_}, input, 0);
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
    states = AssertedEpsilonClosure(std::move(next_states), input, offset + 1);
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
  return MatchInternal(input, SingleRangeCaptureManager(capture_groups_, input, args)).has_value();
}

bool NFA::AssertsBegin() const { return asserts_begin_; }

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
  return PartialMatchInternal(input, offset,
                              SingleRangeCaptureManager(capture_groups_, input, args))
      .has_value();
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
  for (auto const state_num : EpsilonClosure(StateSet{initial_state_})) {
    auto const& state = states_[state_num];
    if ((state.assertions & Assertions::kBegin) != Assertions::kNone) {
      return true;
    }
  }
  return false;
}

NFA::StateSet NFA::EpsilonClosure(StateSet states) const {
  std::vector<uint32_t> stack{states.begin(), states.end()};
  while (!stack.empty()) {
    uint32_t const state_num = stack.back();
    stack.pop_back();
    states.emplace(state_num);
    auto const& edges = states_[state_num].edges;
    auto const it = edges.find(0);
    if (it != edges.end()) {
      for (auto const transition : it->second) {
        if (!states.contains(transition)) {
          stack.emplace_back(transition);
        }
      }
    }
  }
  return states;
}

NFA::StateSet NFA::AssertedEpsilonClosure(StateSet states, std::string_view const input,
                                          size_t const offset) const {
  auto stack = std::move(states).rep();
  states = StateSet();
  while (!stack.empty()) {
    uint32_t const state_num = stack.back();
    stack.pop_back();
    auto const& state = states_[state_num];
    if (Assert(state.assertions, input, offset)) {
      states.emplace(state_num);
      auto const it = state.edges.find(0);
      if (it != state.edges.end()) {
        for (auto const transition : it->second) {
          if (!states.contains(transition)) {
            stack.emplace_back(transition);
          }
        }
      }
    }
  }
  return states;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
