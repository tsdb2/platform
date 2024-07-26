#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "common/ref_count.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Abstract interface of a finite state automaton that recognizes and decides a regular expression
// language. `NFA` and `DFA` inherit this class.
//
// Automata are thread-safe because they are immutable except for the reference count, which is
// implemented by `SimpleRefCounted` in a thread-safe way.
class AbstractAutomaton : public SimpleRefCounted {
 public:
  // Assertions that a state may need to make. Multiple values may be ORed together.
  enum class Assertions {
    kNone = 0,             // No assertions.
    kBegin = 1,            // Assert begin of input (^).
    kEnd = 2,              // Assert end of input ($).
    kWordBoundary = 4,     // Assert word boundary (\b).
    kNotWordBoundary = 8,  // Assert not word boundary (\B).
  };

  // Bitwise OR operator for assertions flags.
  friend Assertions operator|(Assertions const lhs, Assertions const rhs) {
    return static_cast<Assertions>(static_cast<int>(lhs) | static_cast<int>(rhs));
  }

  // Compound OR operator for assertions flags.
  friend Assertions &operator|=(Assertions &lhs, Assertions const rhs) {
    return lhs = operator|(lhs, rhs);
  }

  // Bitwise AND operator for assertions flags.
  friend Assertions operator&(Assertions const lhs, Assertions const rhs) {
    return static_cast<Assertions>(static_cast<int>(lhs) & static_cast<int>(rhs));
  }

  // Compound AND operator for assertions flags.
  friend Assertions &operator&=(Assertions &lhs, Assertions const rhs) {
    return lhs = operator&(lhs, rhs);
  }

  // Individual entry of a capture set (see below).
  using CaptureEntry = absl::InlinedVector<std::string, 1>;

  // Set of captured strings returned by `Match` methods. Each entry corresponds to a capture group
  // and is an array of strings (rather than a single string) because in the presence of a Kleene
  // operator a capture group may capture multiple substrings. For example, the pattern
  // `(f(oo)bar)*` will produce the following capture set when running on the string `foobarfoobar`:
  //
  //   0 -> `foobar`, `foobar`
  //   1 -> `oo`, `oo`
  //
  // Note that we cannot store all substrings captured by a group in a single string. Even if we did
  // that for group #0 (`foobarfoobar`) it would be incorrect to do it for group #1 (`oooo` is not a
  // substring of the original input).
  using CaptureSet = std::vector<CaptureEntry>;

  // Abstract interface for an automaton stepper.
  //
  // A stepper allows running the automaton in separate steps, processing the input string character
  // by character.
  //
  // WARNING: steppers do not support assertions. `^`, `$`, `\b`, and `\B` will be ineffective.
  //
  // TODO: add support for assertions; will need changes in RawTrie.
  //
  // The caller needs to call `Step` repeatedly for every character or for every chunk of the input
  // string, and then needs to call `Finish`. The stepper keeps the running state (i.e. the current
  // state or set of the current states) of the automaton internally and updates it as necessary at
  // every call.
  //
  // Example usage:
  //
  //   bool TestString(NFA const& nfa, std::string_view const input) {
  //     auto const stepper = nfa.CreateStepper();
  //     if (!stepper->Step(input)) {
  //       return false; // no match.
  //     }
  //     return stepper->Finish();
  //   }
  //
  // The above is equivalent to:
  //
  //   bool TestString(NFA const& nfa, std::string_view const input) {
  //     return nfa.Test(input);
  //   }
  //
  // NFAs and DFAs have different stepper implementations, both implementing this interface.
  //
  // WARNING: the automaton must always outlive all of its steppers. Steppers refer to their parent
  // automata by raw pointer, not by `reffed_ptr`. It's the caller's responsibility to maintain the
  // automaton's reference count alive as long as one or more steppers exist.
  class StepperInterface {
   public:
    explicit StepperInterface() = default;
    virtual ~StepperInterface() = default;

    // Clones the stepper, duplicating its internal state.
    virtual std::unique_ptr<StepperInterface> Clone() const = 0;

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
    // If `Finish` returns false the state of the stepper will either not change or change in a way
    // that makes it possible to perform further `Step` calls on subsequent pieces of input. This
    // property makes steppers easily usable to find strings in tries. If, on the other hand,
    // `Finish` returns true, the automaton is no longer usable and must be discarded.
    virtual bool Finish() = 0;

   protected:
    // Copies are performed by `Clone`.
    StepperInterface(StepperInterface const &) = default;
    StepperInterface &operator=(StepperInterface const &) = default;

   private:
    // Moves forbidden: the trie search algorithms require pointer stability.
    StepperInterface(StepperInterface &&) = delete;
    StepperInterface &operator=(StepperInterface &&) = delete;
  };

  explicit AbstractAutomaton() = default;
  ~AbstractAutomaton() override = default;

  // Returns true if this automaton is a DFA, false if it's an NFA.
  virtual bool IsDeterministic() const = 0;

  // Returns the size of the automaton expressed as the number of states (first component of the
  // returned pair) and total number of edges (second component).
  virtual std::pair<size_t, size_t> GetSize() const = 0;

  // Returns the number of capture groups in the regular expression. That is also the size of the
  // vector returned by `Match` for a matching input string.
  virtual size_t GetNumCaptureGroups() const = 0;

  // Creates a stepper for the automaton.
  virtual std::unique_ptr<StepperInterface> MakeStepper() const = 0;

  // Tests the provided `input` string against the regular expression language decided by this
  // automaton.
  virtual bool Test(std::string_view input) const = 0;

  // Runs the automaton on the provided input string and, if it matches, returns the array of
  // strings captured by the capture groups (if any). If the string doesn't match an empty optional
  // is returned.
  virtual std::optional<CaptureSet> Match(std::string_view input) const = 0;

  // Runs the automaton on the provided input string trying to matches its longest possible prefix.
  // Returns the array of captured substrings if a match is found, or an empty optional otherwise.
  std::optional<CaptureSet> MatchPrefix(std::string_view const input) const {
    return PartialMatchInternal(input, 0);
  }

  // Searches for a substring of the `input` string matching this regular expression. The returned
  // match is guaranteed to be the earliest and longest in the input string, with earliest matches
  // taking precedence over longer ones.
  //
  // NOTE: unlike other regular expression implementations this function does not return the full
  // match in the first capture group. If you need that information you need to surround the whole
  // expression in a capture group before compiling it.
  std::optional<CaptureSet> PartialMatch(std::string_view input) const;

 protected:
  // Checks the specified `assertions` on the `input` text at the specified `offset`.
  static bool Assert(Assertions assertions, std::string_view input, size_t offset);

  // Returns a boolean indicating whether the automaton asserts the begin of input (`^`). This is
  // used by `PartialMatch` to determine if it can run `MatchPrefix` on suffixes of the original
  // input string.
  virtual bool AssertsBegin() const = 0;

  // Tries to match a substring of `input` starting at `offset` against this regular expression.
  // This is the internal implementation of `PartialMatch` and `MatchPrefix`.
  virtual std::optional<CaptureSet> PartialMatchInternal(std::string_view input,
                                                         size_t offset) const = 0;

 private:
  // Assertion helpers.
  static bool is_word_character(std::string_view text, size_t offset);
  static bool at_word_boundary(std::string_view text, size_t offset);

  // Copies are not needed: automata are immutable and reference-counted, so we can share them
  // rather than copying them.
  AbstractAutomaton(AbstractAutomaton const &) = delete;
  AbstractAutomaton &operator=(AbstractAutomaton const &) = delete;

  // Moves are forbidden: we need pointer stability because steppers keep a pointer to the parent
  // automaton.
  AbstractAutomaton(AbstractAutomaton &&) = delete;
  AbstractAutomaton &operator=(AbstractAutomaton &&) = delete;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_AUTOMATON_H__
