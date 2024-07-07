#ifndef __TSDB2_COMMON_RE_NFA_H__
#define __TSDB2_COMMON_RE_NFA_H__

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Represents a non-deterministic finite automaton (NFA), aka a compiled regular expression.
class NFA final : public AutomatonInterface {
 public:
  // Represents the set of transitions from a given state with a given label. It's a sorted array of
  // unique destination states. Most commonly there will be only one destination state for that
  // label, so we use `absl::InlinedVector<..., 1>` as the underlying representation.
  using Transitions = flat_set<size_t, std::less<size_t>, absl::InlinedVector<size_t, 1>>;

  // `State` is represented by a map of input characters to transitions. Each character is
  // associated to all the edges that are labeled with it (the `Transitions` set). Character 0 is
  // used to label epsilon-moves.
  using State = flat_map<char, Transitions>;

  // The array of states. The state numbers are the indices in this array. For example, `states[0]`
  // is state 0, `states[1]` is state 1, and so on. State numbers are used as values in
  // `Transitions` sets.
  using States = std::vector<State>;

  // Runs an NFA in steps, allowing the caller to process the input string character by character.
  //
  // The caller needs to call `Step` repeatedly for every character or for every chunk of the input
  // string, and then needs to call `Finish`. The Runner keeps the running state (i.e. the set of
  // the current states) of the automaton internally and updates it as necessary at every call.
  //
  // Example usage:
  //
  //   DFA::Runner runner{&nfa};
  //   runner.Step(input);
  //   if (runner.Finish()) {
  //     // match!
  //   } else {
  //     // no match.
  //   }
  //
  // The above is equivalent to:
  //
  //   if (nfa.Run(input)) {
  //     // match!
  //   } else {
  //     // no match.
  //   }
  //
  // If `Finish` returns false the state of the runner will either not change or change in a way
  // that makes it possible to perform further `Step` calls on subsequent pieces of input. This
  // property makes runners easily usable to find strings in tries. If, on the other hand, `Finish`
  // returns true, the automaton is no longer usable and must be discarded.
  class Runner final {
   public:
    explicit Runner(NFA *const nfa) : nfa_(nfa), states_{nfa_->initial_state_} { EpsilonClosure(); }

    // REQUIRES: `automaton` MUST be an `NFA` instance.
    explicit Runner(AutomatonInterface *const automaton) : Runner(dynamic_cast<NFA *>(automaton)) {}

    Runner(Runner const &) = default;
    Runner &operator=(Runner const &) = default;
    Runner(Runner &&) noexcept = default;
    Runner &operator=(Runner &&) noexcept = default;

    // Transitions the automaton into the next state, or returns false if `ch` has no transition
    // (i.e. the string doesn't match).
    bool Step(char ch);

    // Runs the automaton on every character in `chars`, effectively processing a chunk of the input
    // string. Bails out early and returns false iff a character doesn't match.
    bool Step(std::string_view chars);

    // Processes the end of the input string and returns a boolean indicating whether the string
    // matched.
    bool Finish() const;

   private:
    void EpsilonClosure();

    NFA const *nfa_;
    absl::flat_hash_set<size_t> states_;
  };

  explicit NFA(States states, size_t const initial_state, size_t const final_state)
      : states_(std::move(states)), initial_state_(initial_state), final_state_(final_state) {}

  bool IsDeterministic() const override;

  bool Run(std::string_view input) const override;

 private:
  void EpsilonClosure(absl::flat_hash_set<size_t> *states) const;

  States const states_;
  size_t const initial_state_ = 0;
  size_t const final_state_ = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_NFA_H__
