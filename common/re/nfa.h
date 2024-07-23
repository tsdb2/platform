#ifndef __TSDB2_COMMON_RE_NFA_H__
#define __TSDB2_COMMON_RE_NFA_H__

#include <cstddef>
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
  using Transitions = flat_set<size_t, std::less<size_t>, absl::InlinedVector<size_t, 1>>;

  // Represents a state of the automaton.
  struct State {
    using Edges = flat_map<char, Transitions>;

    explicit State(int const capture_group, Edges edges)
        : innermost_capture_group(capture_group), edges(std::move(edges)) {}

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

    // The edges are represented by a map of input characters to transitions. Each character is
    // associated to all the edges that are labeled with it (the `Transitions` set). Character 0 is
    // used to label epsilon-moves.
    Edges edges;
  };

  // The array of states. The state numbers are the indices in this array. For example, `states[0]`
  // is state 0, `states[1]` is state 1, and so on. State numbers are used as values in the
  // `Transitions` sets.
  using States = std::vector<State>;

 private:
  // Used by our NFA algorithms to keep track of the current set of states efficiently.
  using StateSet = flat_set<size_t, std::less<size_t>, absl::InlinedVector<size_t, 1>>;

 public:
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

  explicit NFA(States states, size_t const initial_state, size_t const final_state,
               CaptureGroups capture_groups)
      : states_(std::move(states)),
        initial_state_(initial_state),
        final_state_(final_state),
        capture_groups_(std::move(capture_groups)) {}

  bool IsDeterministic() const override;

  size_t GetNumCaptureGroups() const override;

  std::unique_ptr<StepperInterface> MakeStepper() const override;

  bool Test(std::string_view input) const override;

  std::optional<std::vector<std::string>> Match(std::string_view input) const override;

  std::optional<std::vector<std::string>> MatchPrefix(std::string_view input) const override;

 private:
  // Like `StateSet`, but it also maps capture sets to their states. This is used by `Match`
  // algorithms.
  using CaptureMap = flat_map<size_t, std::vector<std::string>, std::less<size_t>,
                              absl::InlinedVector<std::pair<size_t, std::vector<std::string>>, 1>>;

  // Calculates the epsilon-closure in `Test` algorithms.
  StateSet EpsilonClosure(StateSet states) const;

  // Calculates the epsilon-closure in `Match` algorithms.
  CaptureMap EpsilonClosure(CaptureMap captures) const;

  States const states_;
  size_t const initial_state_;
  size_t const final_state_;
  CaptureGroups const capture_groups_;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_NFA_H__
