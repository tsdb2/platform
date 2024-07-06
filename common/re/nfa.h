#ifndef __TSDB2_COMMON_RE_NFA_H__
#define __TSDB2_COMMON_RE_NFA_H__

#include <cstddef>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "common/flat_map.h"
#include "common/re/automaton.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Represents a non-deterministic finite automaton (NFA), aka a compiled regular expression.
class NFA final : public AutomatonInterface {
 public:
  // `State` is represented by a map of input characters to transitions. Each character is
  // associated to all the edges that are labeled with it. Character 0 is used to label
  // epsilon-moves.
  using State = flat_map<char, absl::InlinedVector<size_t, 1>>;
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
