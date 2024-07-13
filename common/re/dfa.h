#ifndef __TSDB2_COMMON_RE_DFA_H__
#define __TSDB2_COMMON_RE_DFA_H__

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "common/flat_map.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Represents a deterministic finite automaton (DFA).
//
// This class is faster than `NFA` and is used to run all regular expressions that compile into a
// deterministic automaton (this is not possible for all expressions, some will necessarily yield a
// non-deterministic one).
class DFA final : public AutomatonInterface {
 public:
  // Represents a state of the automaton.
  struct State {
    using Edges = flat_map<char, size_t>;

    explicit State(size_t const capture_group, Edges edges)
        : capture_group(capture_group), edges(std::move(edges)) {}

    State(State const &) = default;
    State &operator=(State const &) = default;
    State(State &&) noexcept = default;
    State &operator=(State &&) noexcept = default;

    // What capture group this state belongs to. Every time this state processes a character (that
    // is, every time a character is looked up in the `edges`) that character is added to the string
    // captured by the group.
    size_t capture_group;

    // The edges are represented by a map of input characters to transitions. Each character is
    // associated to a destination state. Character 0 is used to label epsilon-moves.
    Edges edges;
  };

  using States = std::vector<State>;

  // Runner implementation for DFAs.
  class Runner final : public RunnerInterface {
   public:
    explicit Runner(DFA const *const dfa) : dfa_(dfa), current_state_(dfa_->initial_state_) {}

    Runner(Runner const &) = default;
    Runner &operator=(Runner const &) = default;

    std::unique_ptr<RunnerInterface> Clone() const override;
    bool Step(char ch) override;
    bool Step(std::string_view chars) override;
    bool Finish() override;

   private:
    DFA const *dfa_;
    size_t current_state_;
  };

  explicit DFA(States states, size_t const initial_state, size_t const final_state)
      : states_(std::move(states)), initial_state_(initial_state), final_state_(final_state) {}

  bool IsDeterministic() const override;

  std::unique_ptr<RunnerInterface> CreateRunner() const override;

  bool Run(std::string_view input) const override;

 private:
  States const states_;
  size_t const initial_state_ = 0;
  size_t const final_state_ = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_DFA_H__
