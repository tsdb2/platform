#ifndef __TSDB2_COMMON_RE_DFA_H__
#define __TSDB2_COMMON_RE_DFA_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    // the innermost, the caller is responsible for inferring all the ancestors up to the root.
    //
    // Every time this state processes a character (that is, every time a character is looked up in
    // the `edges`) that character is added to the strings captured by the innermost group as well
    // as all of its parents.
    int innermost_capture_group;

    // Any assertions this state needs to make.
    Assertions assertions;

    // The edges are represented by a map of input characters to transitions. Each character is
    // associated to a destination state. Character 0 is used to label epsilon-moves.
    Edges edges;
  };

  using States = std::vector<State>;

  // Stepper implementation for DFAs.
  class Stepper final : public StepperInterface {
   public:
    explicit Stepper(DFA const *const dfa) : dfa_(dfa), current_state_(dfa_->initial_state_) {}

    Stepper(Stepper const &) = default;
    Stepper &operator=(Stepper const &) = default;

    std::unique_ptr<StepperInterface> Clone() const override;
    bool Step(char ch) override;
    bool Step(std::string_view chars) override;
    bool Finish() override;

   private:
    DFA const *dfa_;
    uint32_t current_state_;
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
  size_t GetTotalEdgeCount() const;
  bool GetAssertsBegin() const;

  static std::optional<CaptureSet> MaybeCloseRanges(std::optional<RangeSet> const &ranges,
                                                    std::string_view source);

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

#endif  // __TSDB2_COMMON_RE_DFA_H__
