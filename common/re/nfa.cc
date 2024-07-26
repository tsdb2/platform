#include "common/re/nfa.h"

#include <cstddef>
#include <functional>
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
  StateCaptureMap states =
      AssertedEpsilonClosure({{initial_state_, RangeSet(capture_groups_)}}, input, 0);
  for (size_t offset = 0; offset < input.size() && !states.empty(); ++offset) {
    StateCaptureMap next_states;
    char const ch = input[offset];
    for (auto const& [state_num, captures] : states) {
      auto const& state = states_[state_num];
      if (auto const it = state.edges.find(ch); it != state.edges.end()) {
        for (auto const transition : it->second) {
          auto const [next_it, inserted] = next_states.try_emplace(transition, captures);
          if (inserted) {
            auto& next_captures = next_it->second;
            next_captures.Capture(ch, state.innermost_capture_group);
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              next_captures.Next(state.innermost_capture_group);
            }
          }
        }
      }
    }
    states = AssertedEpsilonClosure(std::move(next_states), input, offset + 1);
  }
  if (auto const it = states.find(final_state_); it != states.end()) {
    return std::move(it->second).Close();
  } else {
    return std::nullopt;
  }
}

bool NFA::AssertsBegin() const { return asserts_begin_; }

std::optional<AbstractAutomaton::CaptureSet> NFA::PartialMatchInternal(std::string_view const input,
                                                                       size_t offset) const {
  std::optional<CaptureSet> result = std::nullopt;
  StateCaptureMap states =
      AssertedEpsilonClosure({{initial_state_, RangeSet(capture_groups_)}}, input, offset);
  if (auto const it = states.find(final_state_); it != states.end()) {
    result = it->second.Close();
  }
  for (; offset < input.size() && !states.empty(); ++offset) {
    StateCaptureMap next_states;
    char const ch = input[offset];
    for (auto const& [state_num, captures] : states) {
      auto const& state = states_[state_num];
      if (auto const it = state.edges.find(ch); it != state.edges.end()) {
        for (auto const transition : it->second) {
          auto const [next_it, inserted] = next_states.try_emplace(transition, captures);
          if (inserted) {
            auto& next_captures = next_it->second;
            next_captures.Capture(ch, state.innermost_capture_group);
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              next_captures.Next(state.innermost_capture_group);
            }
          }
        }
      }
    }
    states = AssertedEpsilonClosure(std::move(next_states), input, offset + 1);
    if (auto const it = states.find(final_state_); it != states.end()) {
      result = it->second.Close();
    }
  }
  return result;
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
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const state : states) {
      auto const& edges = states_[state].edges;
      auto const it = edges.find(0);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          auto const [it, inserted] = states.emplace(transition);
          new_state_found |= inserted;
        }
        if (new_state_found) {
          break;
        }
      }
    }
  } while (new_state_found);
  return states;
}

NFA::StateCaptureMap NFA::EpsilonClosure(StateCaptureMap capture_map) const {
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const& [state_num, captures] : capture_map) {
      auto const& state = states_[state_num];
      auto const& edges = state.edges;
      auto const it = edges.find(0);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          auto const [next_it, inserted] = capture_map.try_emplace(transition, captures);
          if (inserted) {
            new_state_found = true;
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              next_it->second.Next(state.innermost_capture_group);
            }
            break;
          }
        }
        if (new_state_found) {
          break;
        }
      }
    }
  } while (new_state_found);
  return capture_map;
}

NFA::StateSet NFA::AssertedEpsilonClosure(StateSet states, std::string_view const input,
                                          size_t const offset) const {
  for (auto it = states.begin(); it != states.end();) {
    if (Assert(states_[*it].assertions, input, offset)) {
      ++it;
    } else {
      it = states.erase(it);
    }
  }
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const state : states) {
      auto const& edges = states_[state].edges;
      auto const it = edges.find(0);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          auto const& destination = states_[transition];
          if (Assert(destination.assertions, input, offset)) {
            auto const [it, inserted] = states.emplace(transition);
            new_state_found |= inserted;
          }
        }
        if (new_state_found) {
          break;
        }
      }
    }
  } while (new_state_found);
  return states;
}

NFA::StateCaptureMap NFA::AssertedEpsilonClosure(StateCaptureMap capture_map,
                                                 std::string_view const input,
                                                 size_t const offset) const {
  for (auto it = capture_map.begin(); it != capture_map.end();) {
    if (Assert(states_[it->first].assertions, input, offset)) {
      ++it;
    } else {
      it = capture_map.erase(it);
    }
  }
  bool new_state_found;
  do {
    new_state_found = false;
    for (auto const& [state_num, captures] : capture_map) {
      auto const& state = states_[state_num];
      auto const& edges = state.edges;
      auto const it = edges.find(0);
      if (it != edges.end()) {
        for (auto const transition : it->second) {
          auto const& destination = states_[transition];
          if (Assert(destination.assertions, input, offset)) {
            auto const [next_it, inserted] = capture_map.try_emplace(transition, captures);
            if (inserted) {
              new_state_found = true;
              if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
                next_it->second.Next(state.innermost_capture_group);
              }
              break;
            }
          }
        }
        if (new_state_found) {
          break;
        }
      }
    }
  } while (new_state_found);
  return capture_map;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
