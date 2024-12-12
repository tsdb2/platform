#ifndef __TSDB2_COMMON_RE_NFA_H__
#define __TSDB2_COMMON_RE_NFA_H__

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/re/automaton.h"
#include "common/re/capture_groups.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Represents a non-deterministic finite automaton (NFA), aka a compiled regular expression.
class NFA final : public AbstractAutomaton {
 public:
  // Represents the set of transitions from a given state with a given label. It's a sorted array of
  // unique destination states. Most commonly there will be only one destination state for that
  // label, so we use `absl::InlinedVector<..., 1>` as the underlying representation.
  using StateSet = flat_set<uint32_t, std::less<uint32_t>, absl::InlinedVector<uint32_t, 1>>;

  // Represents a state of the automaton.
  struct State {
    using Edges = flat_map<char, StateSet>;

    explicit State(int const capture_group, Assertions const state_assertions, Edges edges)
        : innermost_capture_group(capture_group),
          assertions(state_assertions),
          edges(std::move(edges)) {}

    explicit State(int const capture_group, Edges edges)
        : State(capture_group, Assertions::kNone, std::move(edges)) {}

    ~State() = default;

    State(State const &) = default;
    State &operator=(State const &) = default;
    State(State &&) noexcept = default;
    State &operator=(State &&) noexcept = default;

    // The innermost capture group this state belongs to. A negative value indicates the state
    // doesn't belong to a capture group.
    //
    // Capture groups may be nested, so each state may belong to more than one. This field indicates
    // the innermost, the caller is responsible for inferring all ancestors up to the root.
    //
    // Every time this state processes a character (that is, every time a character is looked up in
    // the `edges`) the character is added to the strings captured by the innermost group as well as
    // all of its ancestors.
    int innermost_capture_group;

    // Any assertions this state needs to make.
    Assertions assertions;

    // The edges are represented by a map of input characters to transitions. Each character is
    // associated to all the edges that are labeled with it (the `StateSet`). Character 0 is used to
    // label epsilon-moves.
    Edges edges;
  };

  // The array of states. The state numbers are the indices in this array. For example, `states[0]`
  // is state 0, `states[1]` is state 1, and so on. State numbers are used as values in the
  // `StateSet` sets.
  using States = std::vector<State>;

  // Stepper implementation for NFAs.
  class Stepper final : public AbstractStepper {
   public:
    explicit Stepper(NFA const *const nfa, char const previous_character)
        : nfa_(nfa), states_{nfa_->initial_state_}, last_character_(previous_character) {}

    ~Stepper() override = default;

    Stepper(Stepper const &) = default;
    Stepper &operator=(Stepper const &) = default;

    std::unique_ptr<AbstractStepper> Clone() const override;
    bool Step(char ch) override;
    bool Finish(char next_character) const override;

   private:
    Stepper(Stepper &&) = delete;
    Stepper &operator=(Stepper &&) = delete;

    StateSet EpsilonClosure(StateSet states, char ch) const;

    NFA const *nfa_;
    StateSet states_;

    // The last character consumed by `Step` is cached here because we may need it to perform word
    // boundary assertion checks. Value 0 means no characters have been consumed yet.
    char last_character_;
  };

  explicit NFA(States states, uint32_t const initial_state, uint32_t const final_state,
               CaptureGroups capture_groups)
      : states_(std::move(states)),
        initial_state_(initial_state),
        final_state_(final_state),
        capture_groups_(std::move(capture_groups)),
        total_edge_count_(GetTotalEdgeCount()),
        asserts_begin_(GetAssertsBegin()) {}

  bool IsDeterministic() const override;

  bool AssertsBeginOfInput() const override;

  std::pair<size_t, size_t> GetSize() const override;

  size_t GetNumCaptureGroups() const override;

  std::unique_ptr<AbstractStepper> MakeStepper(char previous_character) const override;

  bool Test(std::string_view input) const override;

  std::optional<CaptureSet> Match(std::string_view input) const override;

  bool MatchArgs(std::string_view input, absl::Span<std::string_view *const> args) const override;

 protected:
  template <typename CaptureManager>
  std::optional<CaptureManager> MatchInternal(std::string_view input,
                                              CaptureManager capture_manager) const;

  bool PartialTest(std::string_view input, size_t offset) const override;

  std::optional<CaptureSet> PartialMatch(std::string_view input, size_t offset) const override;

  bool PartialMatchArgs(std::string_view input, size_t offset,
                        absl::Span<std::string_view *const> args) const override;

  template <typename CaptureManager>
  std::optional<CaptureManager> PartialMatchInternal(std::string_view input, size_t offset,
                                                     CaptureManager capture_manager) const;

 private:
  // Like `StateSet`, but it also maps capture managers to their states. This is used by `Match`
  // algorithms.
  template <typename CaptureManager>
  using StateCaptureMap = flat_map<uint32_t, CaptureManager, std::less<uint32_t>,
                                   absl::InlinedVector<std::pair<uint32_t, CaptureManager>, 1>>;

  size_t GetTotalEdgeCount() const;
  bool GetAssertsBegin() const;

  // Calculates the epsilon-closure of a set of states, excluding the ones that fail to assert. The
  // implementation performs an iterative depth-first search.
  template <typename... Args>
  StateSet EpsilonClosure(StateSet states, Args &&...args) const;

  // Like `EpsilonClosure`, but the states in the set are mapped to their respective capture
  // managers.
  template <typename CaptureManager>
  StateCaptureMap<CaptureManager> EpsilonClosure(StateCaptureMap<CaptureManager> capture_map,
                                                 std::string_view input, size_t offset) const;

  States const states_;
  uint32_t const initial_state_;
  uint32_t const final_state_;
  CaptureGroups const capture_groups_;

  size_t const total_edge_count_;
  bool const asserts_begin_;
};

template <typename CaptureManager>
std::optional<CaptureManager> NFA::MatchInternal(std::string_view const input,
                                                 CaptureManager capture_manager) const {
  StateCaptureMap<CaptureManager> states =
      EpsilonClosure<CaptureManager>({{initial_state_, std::move(capture_manager)}}, input, 0);
  for (size_t offset = 0; offset < input.size() && !states.empty(); ++offset) {
    StateCaptureMap<CaptureManager> next_states;
    char const ch = input[offset];
    for (auto const &[state_num, captures] : states) {
      auto const &state = states_[state_num];
      if (auto const it = state.edges.find(ch); it != state.edges.end()) {
        for (auto const transition : it->second) {
          auto const [next_it, inserted] = next_states.try_emplace(transition, captures);
          if (inserted) {
            auto &next_captures = next_it->second;
            next_captures.Capture(offset, state.innermost_capture_group);
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              next_captures.CloseGroup(offset, state.innermost_capture_group);
            }
          }
        }
      }
    }
    states = EpsilonClosure(std::move(next_states), input, offset + 1);
  }
  if (auto const it = states.find(final_state_); it != states.end()) {
    return std::move(it->second);
  } else {
    return std::nullopt;
  }
}

template <typename CaptureManager>
std::optional<CaptureManager> NFA::PartialMatchInternal(std::string_view const input, size_t offset,
                                                        CaptureManager capture_manager) const {
  std::optional<CaptureManager> result = std::nullopt;
  StateCaptureMap<CaptureManager> states =
      EpsilonClosure<CaptureManager>({{initial_state_, std::move(capture_manager)}}, input, offset);
  if (auto const it = states.find(final_state_); it != states.end()) {
    result = it->second;
  }
  for (; offset < input.size() && !states.empty(); ++offset) {
    StateCaptureMap<CaptureManager> next_states;
    char const ch = input[offset];
    for (auto const &[state_num, captures] : states) {
      auto const &state = states_[state_num];
      if (auto const it = state.edges.find(ch); it != state.edges.end()) {
        for (auto const transition : it->second) {
          auto const [next_it, inserted] = next_states.try_emplace(transition, captures);
          if (inserted) {
            auto &next_captures = next_it->second;
            next_captures.Capture(offset, state.innermost_capture_group);
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              next_captures.CloseGroup(offset, state.innermost_capture_group);
            }
          }
        }
      }
    }
    states = EpsilonClosure(std::move(next_states), input, offset + 1);
    if (auto const it = states.find(final_state_); it != states.end()) {
      result = it->second;
    }
  }
  return result;
}

template <typename... AssertionArgs>
NFA::StateSet NFA::EpsilonClosure(StateSet states, AssertionArgs &&...assertion_args) const {
  auto stack = std::move(states).rep();
  states = StateSet();
  while (!stack.empty()) {
    uint32_t const state_num = stack.back();
    stack.pop_back();
    auto const &state = states_[state_num];
    if (AbstractAutomaton::Assert(state.assertions,
                                  std::forward<AssertionArgs>(assertion_args)...)) {
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

template <typename CaptureManager>
NFA::StateCaptureMap<CaptureManager> NFA::EpsilonClosure(
    StateCaptureMap<CaptureManager> capture_map, std::string_view const input,
    size_t const offset) const {
  auto stack = std::move(capture_map).rep();
  capture_map = StateCaptureMap<CaptureManager>();
  while (!stack.empty()) {
    std::pair<uint32_t, CaptureManager> frame = std::move(stack.back());
    stack.pop_back();
    auto const &state = states_[frame.first];
    if (Assert(state.assertions, input, offset)) {
      auto const [it, inserted] = capture_map.emplace(std::move(frame));
      if (auto const edge_it = state.edges.find(0); edge_it != state.edges.end()) {
        for (auto const transition : edge_it->second) {
          if (!capture_map.contains(transition)) {
            CaptureManager captures = it->second;
            if (states_[transition].innermost_capture_group < state.innermost_capture_group) {
              captures.CloseGroup(offset, state.innermost_capture_group);
            }
            stack.emplace_back(std::make_pair(transition, std::move(captures)));
          }
        }
      }
    }
  }
  return capture_map;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_NFA_H__
