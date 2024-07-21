#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/ref_count.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Abstract interface of a finite state automaton that recognizes and decides a regular expression
// language. `NFA` and `DFA` inherit this class.
//
// Automata are thread-safe because they are immutable except for the reference count, which is
// implemented by `SimpleRefCounted` in a thread-safe way.
class AutomatonInterface : public SimpleRefCounted {
 public:
  // Abstract interface for an automaton runner.
  //
  // A runner allows running the automaton in separate steps, processing the input string character
  // by character.
  //
  // The caller needs to call `Step` repeatedly for every character or for every chunk of the input
  // string, and then needs to call `Finish`. The runner keeps the running state (i.e. the current
  // state or set of the current states) of the automaton internally and updates it as necessary at
  // every call.
  //
  // Example usage:
  //
  //   bool TestString(NFA const& nfa, std::string_view const input) {
  //     auto const runner = nfa.CreateRunner();
  //     if (!runner->Step(input)) {
  //       return false; // no match.
  //     }
  //     return runner->Finish();
  //   }
  //
  // The above is equivalent to:
  //
  //   bool TestString(NFA const& nfa, std::string_view const input) {
  //     return nfa.Test(input);
  //   }
  //
  // NFAs and DFAs have different runner implementations, both implementing this interface.
  //
  // WARNING: the automaton must always outlive all of its runners. Runners refer to their parent
  // automata by raw pointer, not by `reffed_ptr`. It's the caller's responsibility to maintain the
  // automaton's reference count alive as long as one or more runners exist.
  class RunnerInterface {
   public:
    explicit RunnerInterface() = default;
    virtual ~RunnerInterface() = default;

    // Clones the runner, duplicating its internal state.
    virtual std::unique_ptr<RunnerInterface> Clone() const = 0;

    // Transitions the automaton into the next state, or returns false if `ch` has no transition
    // (i.e. the string doesn't match). When false is returned the automaton is no longer usable and
    // must be discarded (further calls to `Step` or `Finish` lead to undefined behavior).
    virtual bool Step(char ch) = 0;

    // Runs the automaton on every character in `chars`, effectively processing a chunk of the input
    // string. Bails out early and returns false iff a character doesn't match.
    virtual bool Step(std::string_view chars) = 0;

    // Processes the end of the input string and returns a boolean indicating whether the string
    // matched.
    //
    // If `Finish` returns false the state of the runner will either not change or change in a way
    // that makes it possible to perform further `Step` calls on subsequent pieces of input. This
    // property makes runners easily usable to find strings in tries. If, on the other hand,
    // `Finish` returns true, the automaton is no longer usable and must be discarded.
    virtual bool Finish() = 0;

   protected:
    // Copies are performed by `Clone`.
    RunnerInterface(RunnerInterface const &) = default;
    RunnerInterface &operator=(RunnerInterface const &) = default;

   private:
    // Moves forbidden: the trie search algorithms require pointer stability.
    RunnerInterface(RunnerInterface &&) = delete;
    RunnerInterface &operator=(RunnerInterface &&) = delete;
  };

  explicit AutomatonInterface() = default;
  ~AutomatonInterface() override = default;

  // Returns true if this automaton is a DFA, false if it's an NFA.
  virtual bool IsDeterministic() const = 0;

  // Returns the number of capture groups in the regular expression. That is also the size of the
  // vector returned by `Match` for a matching input string.
  virtual size_t GetNumCaptureGroups() const = 0;

  // Creates a runner for the automaton.
  virtual std::unique_ptr<RunnerInterface> CreateRunner() const = 0;

  // Tests the provided `input` string against the regular expression language decided by this
  // automaton.
  virtual bool Test(std::string_view input) const = 0;

  // Runs the automaton on the provided input string and, if it matches, returns the array of
  // strings captured by the capture groups (if any). If the string doesn't match an empty optional
  // is returned.
  virtual std::optional<std::vector<std::string>> Match(std::string_view input) const = 0;

  // Runs the automaton on the provided input string trying to matches its longest possible prefix.
  // Returns the array of captured substrings if a match is found, or an empty optional otherwise.
  virtual std::optional<std::vector<std::string>> MatchPrefix(std::string_view input) const = 0;

 private:
  // Copies are not needed: automata are immutable and reference-counted, so we can share them
  // rather than copying them.
  AutomatonInterface(AutomatonInterface const &) = delete;
  AutomatonInterface &operator=(AutomatonInterface const &) = delete;

  // Moves are forbidden: we need pointer stability because runners keep a pointer to the parent
  // automaton.
  AutomatonInterface(AutomatonInterface &&) = delete;
  AutomatonInterface &operator=(AutomatonInterface &&) = delete;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_AUTOMATON_H__
