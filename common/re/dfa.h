#ifndef __TSDB2_COMMON_RE_DFA_H__
#define __TSDB2_COMMON_RE_DFA_H__

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
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

  bool Test(std::string_view input) const override;

  std::optional<std::vector<std::string>> Match(std::string_view input) const override;

 private:
  // Runs the full matching algorithm, which matches the string and also returns all captured
  // substrings (as opposed to `DFA::Test` which only matches the string).
  //
  // `Matcher` uses a backtracking algorithm, but thanks to the use of dynamic programming its
  // worst-case complexity is reduced to O(N*M) with N = number of states and M = length of the
  // input string.
  class Matcher {
   public:
    explicit Matcher(DFA const &dfa, std::string_view const input) : dfa_(dfa), input_(input) {}

    // Matches the input string against the DFA (both are provided in the `Matcher` constructor).
    // Returns the array of captured substrings, or an empty optional if the string doesn't match.
    std::optional<std::vector<std::string>> Match();

   private:
    using MatchResults = std::optional<flat_map<size_t, std::string>>;
    using Cache = flat_map<std::pair<size_t, size_t>, MatchResults>;

    MatchResults Cached(size_t current_state_num, size_t offset, MatchResults value);

    // Internal matching algorithm implementation. In order to avoid the exponential complexity of
    // the backtracking algorithm, this function checks the `cache_` before doing any work.
    //
    // When the string matches, a map of capture groups to captured strings is returned; otherwise
    // we return an empty optional.
    //
    // NOTE: due to the way `MatchInternal`'s algorithm works, the returned captured strings are
    // reversed; the public `Match` method takes of care of reversing them.
    MatchResults MatchInternal(size_t current_state_num, size_t offset);

    DFA const &dfa_;
    std::string_view const input_;

    // Caches the results of the `MatchInternal` algorithm. The keys of this map are pairs of
    // integers, corresponding to the two arguments of `MatchInternal`: the first integer is the
    // current state number and the second is the offset in the input string. The values of the map
    // have the same meaning as the return values of `MatchInternal`.
    Cache cache_;
  };

  States const states_;
  size_t const initial_state_ = 0;
  size_t const final_state_ = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_DFA_H__
