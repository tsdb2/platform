#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "common/re/capture_groups.h"
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

  // Bitwise OR operator for assertion flags.
  friend Assertions operator|(Assertions const lhs, Assertions const rhs) {
    return static_cast<Assertions>(static_cast<int>(lhs) | static_cast<int>(rhs));
  }

  // Compound OR operator for assertion flags.
  friend Assertions &operator|=(Assertions &lhs, Assertions const rhs) {
    return lhs = operator|(lhs, rhs);
  }

  // Bitwise AND operator for assertion flags.
  friend Assertions operator&(Assertions const lhs, Assertions const rhs) {
    return static_cast<Assertions>(static_cast<int>(lhs) & static_cast<int>(rhs));
  }

  // Compound AND operator for assertion flags.
  friend Assertions &operator&=(Assertions &lhs, Assertions const rhs) {
    return lhs = operator&(lhs, rhs);
  }

  // Individual entry of a capture set (see below).
  using CaptureEntry = absl::InlinedVector<std::string_view, 1>;

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

  // Represents the substrings captured by `Match` methods expressed as ranges rather than
  // `std::string_view` substring objects. A range is an <offset,length> pair relative to the
  // original input string.
  //
  // Note that this type provides only one range for each capture group, so if a capture group gets
  // triggered more than once only the last captured range is provided. If a capture group doesn't
  // get triggered at all, the corresponding range is set to <-1,-1>.
  using RangeSet = std::vector<std::pair<intptr_t, intptr_t>>;

  // Abstract interface for an automaton stepper.
  //
  // A stepper allows running the automaton in separate steps, processing the input string character
  // by character.
  //
  // The caller needs to call `Step` repeatedly for every character or for every chunk of the input
  // string, and then needs to call `Finish`. The stepper keeps the running state (i.e. the current
  // state or set of the current states) of the automaton internally and updates it as necessary at
  // every call.
  //
  // Example usage:
  //
  //   bool TestString(NFA const& nfa, std::string_view const input) {
  //     auto const stepper = nfa.MakeStepper();
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
  class AbstractStepper {
   public:
    explicit AbstractStepper() = default;
    virtual ~AbstractStepper() = default;

    // Clones the stepper, duplicating its internal state.
    virtual std::unique_ptr<AbstractStepper> Clone() const = 0;

    // Transitions the automaton into the next state, or returns false if `ch` has no transition
    // (i.e. the string doesn't match). When false is returned the stepper is no longer usable and
    // must be discarded (further calls to `Step` or `Finish` lead to undefined behavior).
    virtual bool Step(char ch) = 0;

    // Runs the automaton on every character in `chars`, effectively processing a chunk of the input
    // string. Bails out early and returns false iff a character doesn't match.
    bool Step(std::string_view chars);

    // Processes the end of the input string and returns a boolean indicating whether the string
    // matched.
    //
    // In a partial match, `next_character` is the character following the end of the substring
    // scanned by the stepper. It must be 0 when using the stepper for full matches.
    //
    // Note that this method is `const`, so it doesn't change the state of the stepper and it is
    // okay to make further `Step` calls to test an alternate suffix even after a successful
    // `Finish` call. This property makes steppers easily usable to find strings in tries.
    virtual bool Finish(char next_character) const = 0;

    // Shorthand for `Finish(0)`.
    bool Finish() const { return Finish(0); }

   protected:
    // Copies are performed by `Clone`.
    AbstractStepper(AbstractStepper const &) = default;
    AbstractStepper &operator=(AbstractStepper const &) = default;

   private:
    // Moves forbidden: the trie search algorithms require pointer stability.
    AbstractStepper(AbstractStepper &&) = delete;
    AbstractStepper &operator=(AbstractStepper &&) = delete;
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

  // Returns a boolean indicating whether the automaton asserts the begin of input (`^`). This is
  // used by `PartialMatch` to determine if it can run `MatchPrefix` on suffixes of the original
  // input string.
  //
  // Note that this method indicates whether or not the automaton *always* asserts the begin of
  // input. For example, it would return false for the pattern `lorem|^ipsum` because the
  // corresponding automaton would still be able to match `lorem` after the begin of input.
  virtual bool AssertsBeginOfInput() const = 0;

  // Returns the minim length of the strings matched by this automaton. The value is inferred at
  // compilation time using Dijkstra's algorithm and cached in the automaton, and all testing and
  // matching methods are optimized to skip strings shorter than this value.
  virtual size_t GetMinMatchLength() const = 0;

  // Creates a stepper for the automaton. `previous_character` is the character preceding the
  // substring that the stepper will scan, or 0 if the stepper will scan a prefix of the original
  // input or the entire string. This information is needed to check possible assertions in the
  // initial state of the automaton.
  virtual std::unique_ptr<AbstractStepper> MakeStepper(char previous_character) const = 0;

  // Creates a stepper for the automaton using 0 as the previous character.
  std::unique_ptr<AbstractStepper> MakeStepper() const { return MakeStepper(0); }

  // Tests the provided `input` string against the regular expression language decided by this
  // automaton.
  virtual bool Test(std::string_view input) const = 0;

  // Runs the automaton on the provided input string, returning true if it finds a prefix that
  // matches this regular expression or false otherwise.
  bool TestPrefix(std::string_view const input) const { return PartialTest(input, 0); }

  // Checks if the `input` string contains a substring matching this regular expression.
  bool PartialTest(std::string_view input) const;

  // Runs the automaton on the provided input string and, if it matches, returns the array of
  // strings captured by the capture groups (if any). If the string doesn't match an empty optional
  // is returned.
  virtual std::optional<CaptureSet> Match(std::string_view input) const = 0;

  // Same as `Match` above but stores the captured substrings in the provided `std::string_view`
  // objects rather than returning a `CaptureSet`. Returns a boolean indicating whether the `input`
  // string matched. The contents of the string views are undefined if false is returned.
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned.
  //
  // NOTE: `args` would normally have as many elements as there are capture groups in the
  // expression, but it's okay to provide a different number: missing substrings won't be retrieved
  // and extra string views will be ignored.
  virtual bool MatchArgs(std::string_view input,
                         absl::Span<std::string_view *const> args) const = 0;

  // Same as `Match` above but returns a `RangeSet` rather than a `CaptureSet`.
  virtual std::optional<RangeSet> MatchRanges(std::string_view input) const = 0;

  // Runs the automaton on the provided input string trying to match the longest possible prefix.
  // Returns the array of captured substrings if a match is found, or an empty optional otherwise.
  std::optional<CaptureSet> MatchPrefix(std::string_view const input) const {
    return PartialMatch(input, 0);
  }

  // Same as `MatchPrefix` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns a boolean indicating
  // whether a matching prefix was found. The contents of the string views are undefined if false is
  // returned.
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned.
  //
  // NOTE: `args` would normally have as many elements as there are capture groups in the
  // expression, but it's okay to provide a different number: missing substrings won't be retrieved
  // and extra string views will be ignored.
  bool MatchPrefixArgs(std::string_view const input,
                       absl::Span<std::string_view *const> const args) const {
    return PartialMatchArgs(input, 0, args);
  }

  // Same as `MatchPrefix` above but returns a `RangeSet` rather than a `CaptureSet`.
  std::optional<RangeSet> MatchPrefixRanges(std::string_view const input) const {
    return PartialMatchRanges(input, 0);
  }

  // Searches for a substring of the `input` string matching this regular expression. The returned
  // match is guaranteed to be the earliest and longest in the input string, with earliest matches
  // taking precedence over longer ones.
  //
  // NOTE: unlike other regular expression implementations this function does not return the full
  // match in the first capture group. If you need that information you need to surround the whole
  // expression in a capture group before compiling it.
  std::optional<CaptureSet> PartialMatch(std::string_view input) const;

  // Same as `PartialMatch` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns a boolean indicating
  // whether a matching substring was found. The contents of the string views are undefined if false
  // is returned.
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned.
  //
  // NOTE: `args` would normally have as many elements as there are capture groups in the
  // expression, but it's okay to provide a different number: missing substrings won't be retrieved
  // and extra string views will be ignored.
  bool PartialMatchArgs(std::string_view input, absl::Span<std::string_view *const> args) const;

  // Same as `PartialMatch` above but returns a `RangeSet` rather than a `CaptureSet`.
  std::optional<RangeSet> PartialMatchRanges(std::string_view input) const;

 protected:
  // Used in subclasses by non-Args versions of the `Match*` methods to track the boundaries of the
  // captured substrings and build the final `CaptureSet`.
  //
  // Internally this class maintains arrays of "ranges", each range being the pair of boundaries of
  // a captured string. `CaptureSet` has a similar structure but holds `std::string_view`s rather
  // than pairs of boundaries.
  class RangeSetCaptureManager {
   public:
    explicit RangeSetCaptureManager(CaptureGroups const &capture_groups)
        : capture_groups_(&capture_groups),
          ranges_(capture_groups.size(),
                  absl::InlinedVector<std::pair<intptr_t, intptr_t>, 2>{{-1, -1}}) {}

    ~RangeSetCaptureManager() = default;

    RangeSetCaptureManager(RangeSetCaptureManager const &) = default;
    RangeSetCaptureManager &operator=(RangeSetCaptureManager const &) = default;
    RangeSetCaptureManager(RangeSetCaptureManager &&) noexcept = default;
    RangeSetCaptureManager &operator=(RangeSetCaptureManager &&) noexcept = default;

    // Closes the current capture group.
    void CloseGroup(intptr_t offset, int capture_group);

    // Captures a single character in the specified group and its ancestors.
    void Capture(intptr_t offset, int innermost_capture_group);

    // Builds a `RangeSet` from the ranges captured so far.
    RangeSet ToRanges() const;

    // Builds a `CaptureSet` from the ranges captured so far.
    CaptureSet ToCaptureSet(std::string_view source) const;

   private:
    CaptureGroups const *capture_groups_;

    // `ranges_` has the same structure of a `CaptureSet`: the i-th element of the main vector keeps
    // track of the strings captured by the i-th capture group. The innermost values are "ranges",
    // i.e. pairs of the boundaries of the corresponding substring.
    //
    // We always have at least 1 element for each capture group because the last element in that
    // group is a "pending string" (the one we're currently building). For this reason we inline the
    // first 2 elements: the first is inlined with the same rationale as `CaptureSet` (it's often
    // the case that a capture group captures only 1 string), and the second is inlined because it's
    // the pending string.
    std::vector<absl::InlinedVector<std::pair<intptr_t, intptr_t>, 2>> ranges_;
  };

  // Used in subclasses by the `Args` versions of the `Match*` methods to track the latest capture
  // of each group.
  //
  // Internally this class maintains a single array of "ranges", each range being the pair of
  // boundaries of the last substring captured by the corresponding capture group.
  class SingleRangeCaptureManager {
   public:
    explicit SingleRangeCaptureManager(CaptureGroups const &capture_groups,
                                       std::string_view const source,
                                       absl::Span<std::string_view *const> const args)
        : capture_groups_(&capture_groups),
          source_(source),
          args_(args),
          ranges_(std::min(capture_groups.size(), args.size()), Range()) {}

    ~SingleRangeCaptureManager() = default;

    SingleRangeCaptureManager(SingleRangeCaptureManager const &) = default;
    SingleRangeCaptureManager &operator=(SingleRangeCaptureManager const &) = default;
    SingleRangeCaptureManager(SingleRangeCaptureManager &&) noexcept = default;
    SingleRangeCaptureManager &operator=(SingleRangeCaptureManager &&) noexcept = default;

    // Closes the current capture group.
    void CloseGroup(intptr_t offset, int capture_group);

    // Captures a single character in the specified group and its ancestors.
    void Capture(intptr_t offset, int innermost_capture_group);

    // Dumps the strings captured so far to the caller-provided arg span cached in `args_`.
    //
    // Note that we can't simply dump each string as we close the corresponding group because NFAs
    // may make many failed attempts with many different capture managers that we'll eventually
    // discard. We need an actual signal that this capture manager has been accepted, which we get
    // when the caller calls `Dump`.
    void Dump() const;

   private:
    struct Range {
      std::string_view closed_string = "";
      intptr_t begin = -1;
      intptr_t end = -1;
    };

    CaptureGroups const *capture_groups_;
    std::string_view source_;
    absl::Span<std::string_view *const> args_;
    std::vector<Range> ranges_;
  };

  // Checks the specified `assertions` on the `input` text at the specified `offset`.
  static bool Assert(Assertions assertions, std::string_view input, size_t offset);

  // Checks the specified `assertions` on the two input characters.
  static bool Assert(Assertions assertions, char ch1, char ch2);

  // Tries to match a substring of `input` starting at `offset` against this regular expression.
  // This is the internal implementation of `PartialTest` and `TestPrefix`.
  virtual bool PartialTest(std::string_view input, size_t offset) const = 0;

  // Tries to match a substring of `input` starting at `offset` against this regular expression.
  // This is the internal implementation of `PartialMatch` and `MatchPrefix`.
  virtual std::optional<CaptureSet> PartialMatch(std::string_view input, size_t offset) const = 0;

  // Same as `PartialMatch` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. This is the internal
  // implementation of `PartialMatchArgs` and `MatchPrefixArgs`.
  virtual bool PartialMatchArgs(std::string_view input, size_t offset,
                                absl::Span<std::string_view *const> args) const = 0;

  // Same as `PartialMatch` above but returns a `RangeSet` rather than a `CaptureSet`.
  virtual std::optional<RangeSet> PartialMatchRanges(std::string_view input,
                                                     size_t offset) const = 0;

 private:
  // Assertion helpers.
  static bool is_word_character(char ch);
  static bool is_word_character(std::string_view text, size_t offset);
  static bool at_word_boundary(char ch1, char ch2);
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
