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
  using State = flat_map<char, size_t>;
  using States = std::vector<State>;

  explicit DFA() = default;

  explicit DFA(States states, size_t const initial_state, size_t const final_state)
      : states_(std::move(states)), initial_state_(initial_state), final_state_(final_state) {}

  DFA(DFA const &) = default;
  DFA &operator=(DFA const &) = default;
  DFA(DFA &&) noexcept = default;
  DFA &operator=(DFA &&) noexcept = default;

  std::unique_ptr<AutomatonInterface> Clone() const override;

  bool Run(std::string_view input) const override;

 private:
  States states_;
  size_t initial_state_ = 0;
  size_t final_state_ = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_DFA_H__
