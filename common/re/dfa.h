#ifndef __TSDB2_COMMON_RE_DFA_H__
#define __TSDB2_COMMON_RE_DFA_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/re/automaton.h"
#include "common/re/capture_groups.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Represents a deterministic finite automaton (DFA).
//
// This class is faster than `NFA` and is used to run all regular expressions that compile into a
// deterministic automaton (this is not possible for all expressions, some will necessarily yield a
// non-deterministic one).
class DFA final : public AbstractAutomaton {
 public:
  // Represents a state of the automaton.
  struct State {
    using Edges = flat_map<char, uint32_t>;

    explicit State(int const capture_group, Assertions const state_assertions, Edges edges)
        : innermost_capture_group(capture_group),
          assertions(state_assertions),
          edges(std::move(edges)) {}

    explicit State(int const capture_group, Edges edges)
        : State(capture_group, Assertions::kNone, std::move(edges)) {}

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
    // associated to a destination state. Character 0 is used to label epsilon-moves.
    Edges edges;
  };

  using States = std::vector<State>;

  // Stepper implementation for DFAs.
  class Stepper final : public AbstractStepper {
   public:
    explicit Stepper(DFA const *const dfa, char const previous_character)
        : dfa_(dfa), current_state_(dfa_->initial_state_), last_character_(previous_character) {}

    Stepper(Stepper const &) = default;
    Stepper &operator=(Stepper const &) = default;

    std::unique_ptr<AbstractStepper> Clone() const override;
    bool Step(char ch) override;
    bool Finish(char next_character) const override;

   private:
    bool Assert(uint32_t const state_num, char const ch) const {
      return dfa_->Assert(state_num, last_character_, ch);
    }

    DFA const *dfa_;
    uint32_t current_state_;

    // The last character consumed by `Step` is cached here because we may need it to perform word
    // boundary assertion checks. Value 0 means no characters have been consumed yet.
    char last_character_;
  };

  explicit DFA(States states, uint32_t const initial_state, uint32_t const final_state,
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
  bool MatchInternal(std::string_view input, CaptureManager *capture_manager) const;

  bool PartialTest(std::string_view input, size_t offset) const override;

  std::optional<CaptureSet> PartialMatch(std::string_view input, size_t offset) const override;

  bool PartialMatchArgs(std::string_view input, size_t offset,
                        absl::Span<std::string_view *const> args) const override;

  template <typename CaptureManager>
  bool PartialMatchInternal(std::string_view input, size_t offset,
                            CaptureManager capture_manager) const;

 private:
  size_t GetTotalEdgeCount() const;
  bool GetAssertsBegin() const;

  // Assertion helpers for Stepper.

  template <typename... Args>
  bool Assert(State const &state, Args &&...args) const {
    return AbstractAutomaton::Assert(state.assertions, std::forward<Args>(args)...);
  }

  template <typename... Args>
  bool Assert(uint32_t const state_num, Args &&...args) const {
    return AbstractAutomaton::Assert(states_[state_num].assertions, std::forward<Args>(args)...);
  }

  States const states_;
  uint32_t const initial_state_;
  uint32_t const final_state_;
  CaptureGroups const capture_groups_;

  size_t const total_edge_count_;
  bool const asserts_begin_;
};

template <typename CaptureManager>
bool DFA::MatchInternal(std::string_view const input, CaptureManager *const capture_manager) const {
  uint32_t state_num = initial_state_;
  size_t offset = 0;
  while (offset < input.size()) {
    auto const &state = states_[state_num];
    if (!Assert(state, input, offset)) {
      return false;
    }
    auto it = state.edges.find(0);
    if (it == state.edges.end()) {
      char const ch = input[offset];
      it = state.edges.find(ch);
      if (it == state.edges.end()) {
        return false;
      }
      capture_manager->Capture(offset, state.innermost_capture_group);
      ++offset;
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      capture_manager->CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const &state = states_[state_num];
    if (!Assert(state, input, offset)) {
      return false;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return false;
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      capture_manager->CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  if (!Assert(final_state_, input, offset)) {
    return false;
  }
  return true;
}

template <typename CaptureManager>
bool DFA::PartialMatchInternal(std::string_view const input, size_t offset,
                               CaptureManager capture_manager) const {
  bool result = false;
  uint32_t state_num = initial_state_;
  while (offset < input.size()) {
    auto const &state = states_[state_num];
    if (!Assert(state, input, offset)) {
      return result;
    }
    if (state_num == final_state_) {
      result = true;
    }
    char const ch = input[offset];
    auto it = state.edges.find(ch);
    if (it != state.edges.end()) {
      capture_manager->Capture(offset, state.innermost_capture_group);
      ++offset;
    } else {
      it = state.edges.find(0);
      if (it == state.edges.end()) {
        return result;
      }
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      capture_manager->CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  while (state_num != final_state_) {
    auto const &state = states_[state_num];
    if (!Assert(state, input, offset)) {
      return result;
    }
    auto const it = state.edges.find(0);
    if (it == state.edges.end()) {
      return result;
    }
    if (states_[it->second].innermost_capture_group < state.innermost_capture_group) {
      capture_manager->CloseGroup(offset, state.innermost_capture_group);
    }
    state_num = it->second;
  }
  if (!Assert(final_state_, input, offset)) {
    return result;
  }
  return true;
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_DFA_H__
