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

  explicit NFA(States states, size_t const initial_state, size_t const final_state)
      : states_(std::move(states)), initial_state_(initial_state), final_state_(final_state) {}

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
