#ifndef __TSDB2_COMMON_RE_NFA_H__
#define __TSDB2_COMMON_RE_NFA_H__

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
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

    State(State const &) = default;
    State &operator=(State const &) = default;
    State(State &&) noexcept = default;
    State &operator=(State &&) noexcept = default;

    // The innermost capture group this state belongs to. A negative value indicates the state
    // doesn't belong to a capture group.
    //
    // Capture groups may be nested, so each state may belong to more than one. This field indicates
    // the innermost, the caller is responsible for inferring all the ancestors up to the root.
    //
    // Every time this state processes a character (that is, every time a character is looked up in
    // the `edges`) that character is added to the strings captured by the innermost group as well
    // as all of its ancestors.
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
  class Stepper final : public StepperInterface {
   public:
    explicit Stepper(NFA const *const nfa) : nfa_(nfa), states_{nfa_->initial_state_} {
      EpsilonClosure();
    }

    Stepper(Stepper const &) = default;
    Stepper &operator=(Stepper const &) = default;

    std::unique_ptr<StepperInterface> Clone() const override;
    bool Step(char ch) override;
    bool Step(std::string_view chars) override;
    bool Finish() override;

   private:
    void EpsilonClosure();

    NFA const *nfa_;
    StateSet states_;
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

  std::pair<size_t, size_t> GetSize() const override;

  size_t GetNumCaptureGroups() const override;

  std::unique_ptr<StepperInterface> MakeStepper() const override;

  bool Test(std::string_view input) const override;

  std::optional<CaptureSet> Match(std::string_view input) const override;

 protected:
  bool AssertsBegin() const override;

  std::optional<CaptureSet> PartialMatchInternal(std::string_view input,
                                                 size_t offset) const override;

 private:
  // Like `StateSet`, but it also maps capture sets to their states. This is used by `Match`
  // algorithms.
  using StateCaptureMap = flat_map<uint32_t, RangeSet, std::less<uint32_t>,
                                   absl::InlinedVector<std::pair<uint32_t, RangeSet>, 1>>;

  size_t GetTotalEdgeCount() const;
  bool GetAssertsBegin() const;

  // Calculates the epsilon-closure in `Test` algorithms. The implementation performs an iterative
  // depth-first search.
  StateSet EpsilonClosure(StateSet states) const;

  // `AssertedEpsilonClosure` algorithms are like `EpsilonClosure` but they also remove all states
  // that fail one or more assertions. They may return empty state sets.

  StateSet AssertedEpsilonClosure(StateSet states, std::string_view input, size_t offset) const;

  StateCaptureMap AssertedEpsilonClosure(StateCaptureMap capture_map, std::string_view input,
                                         size_t offset) const;

  States const states_;
  uint32_t const initial_state_;
  uint32_t const final_state_;
  CaptureGroups const capture_groups_;

  size_t const total_edge_count_;
  bool const asserts_begin_;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_NFA_H__
