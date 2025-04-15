#ifndef __TSDB2_COMMON_RE_H__
#define __TSDB2_COMMON_RE_H__

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/re/automaton.h"
#include "common/re/parser.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

namespace internal {

// This forward declaration refers to the internal implementation of tries. We need it here so that
// we can declare `TrieNode` as a friend of `RE`, allowing integration between the two.
template <typename Label, typename Allocator>
class TrieNode;

// Used internally to produce error statuses.
std::string ClipString(std::string_view text);

}  // namespace internal

// `RE` is the interface to TSDB2's own implementation of regular expressions.
//
// TSDB2 uses its own implementation rather than the one provided by STL or any other C++ regular
// expression library for at least the following reasons:
//
//   1. We need an implementation that is guaranteed to be immune to ReDoS attacks. Ours is immune
//      because it doesn't provide any NP-hard features (most notably we do not support
//      backreferences). It also never uses recursive algorithms except for the parser, where the
//      maximum recursion depth is capped by the `--re_max_recursion_depth` command line flag; this
//      way we can guard against stack overflows.
//   2. We need an implementation that can be integrated with our tries so that we can run finite
//      state automata algorithms on tries, allowing for efficient retrieval of strings based on
//      regular expression patterns.
//   3. Most implementations do not return all the information we need when a capturing group is
//      activated multiple times. This can happen in the presence of a Kleene operator. For example,
//      matching the string `foo bar fee bar ` against the pattern `(f(..) bar )*` will cause the
//      second capture group to return both `oo` and `ee` in our implementation, but most other
//      implementations would only return `ee`.
//
// TODO: describe the syntax.
//
class RE {
 public:
  using Options = regexp_internal::Options;

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
  //
  // The returned strings are `std::string_view` objects referring to substrings of the string
  // provided by the caller, so the latter must remain alive in order for the caller to be able to
  // read the captured strings.
  using CaptureSet = regexp_internal::AbstractAutomaton::CaptureSet;

  // Checks if `input` matches `pattern`.
  //
  // This function doesn't allow the caller to tell if `pattern` fails to compile; in that case it
  // will simply return false. It's recommended to use this function only if the caller is 100% sure
  // the pattern compiles, e.g. it's hard-coded in the program rather than being provided by a
  // human. Otherwise you need to use the non-static version of this method.
  static bool Test(std::string_view input, std::string_view pattern);

  // Checks if the `input` string contains a substring matching `pattern`.
  //
  // This function doesn't allow the caller to tell if `pattern` fails to compile; in that case it
  // will simply return false. It's recommended to use this function only if the caller is 100% sure
  // the pattern compiles, e.g. it's hard-coded in the program rather than being provided by a
  // human. Otherwise you need to use the non-static `ContainedIn` method.
  static bool Contains(std::string_view input, std::string_view pattern);

  // Checks if `input` matches `pattern` and returns an array of the strings captured by the capture
  // groups. An error status is returned if `pattern` fails to compile or `input` doesn't match.
  static absl::StatusOr<CaptureSet> Match(std::string_view input, std::string_view pattern);

  // Same as `Match` above but stores the captured substrings in the provided `std::string_view`
  // objects rather than returning a `CaptureSet`. Returns an OK status if the `input` matched, or
  // an error if the `pattern` didn't compile or the `input` didn't match. The contents of the
  // string views are undefined if false is returned.
  //
  // Example:
  //
  //   std::string_view blah;
  //   RETURN_IF_ERROR(RE::MatchArgs("blah blah", "blah (blah)", &blah));
  //   LOG(INFO) << blah;  // logs "blah"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   std::string_view sv;
  //   RETURN_IF_ERROR(RE::MatchArgs("foo bar fee bar ", "(?:f(..) bar )*", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  static absl::Status MatchArgs(std::string_view input, std::string_view pattern,
                                Args *const... args);

  // Checks if `input` contains a substring matching `pattern` and returns an array of the strings
  // captured by the capture groups. An error status is returned if `pattern` fails to compile or
  // `input` doesn't have any matching substrings.
  //
  // The matching substring is guaranteed to be the earliest and longest possible one, with earliest
  // taking precedence over longest.
  static absl::StatusOr<CaptureSet> PartialMatch(std::string_view input, std::string_view pattern);

  // Same as `PartialMatch` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns an OK status if the
  // `input` matched, or an error if the `pattern` didn't compile or the `input` didn't match. The
  // contents of the string views are undefined if an error is returned.
  //
  // Example:
  //
  //   std::string_view blah;
  //   RETURN_IF_ERROR(RE::PartialMatchArgs("blah blah", "(blah)", &blah));
  //   LOG(INFO) << blah;  // logs "blah"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   std::string_view sv;
  //   RETURN_IF_ERROR(RE::PartialMatchArgs("foo bar fee bar ", "(?:f(..) bar )*", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  static absl::Status PartialMatchArgs(std::string_view input, std::string_view pattern,
                                       Args *const... args);

  // Strips the longest possible prefix matching `pattern` from the provided `input` string and
  // returns an array of the strings captured by the capture groups. An error status is returned if
  // `pattern` fails to compile or `input` doesn't have a matching prefix.
  static absl::StatusOr<CaptureSet> ConsumePrefix(std::string_view *input,
                                                  std::string_view pattern);

  // Same as `ConsumePrefix` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns an OK status if a
  // prefix matched, or an error if the `pattern` didn't compile or no prefix matched. The contents
  // of the string views are undefined if an error is returned.
  //
  // Example:
  //
  //   std::string_view constexpr pi = "3.141592...";
  //   std::string_view integer, decimal;
  //   RETURN_IF_ERROR(RE::ConsumePrefixArgs(pi, "([0-9]+)\\.([0-9]+)", &integer, &decimal));
  //   LOG(INFO) << decimal;  // logs "141592"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   std::string_view sv;
  //   RETURN_IF_ERROR(RE::ConsumePrefixArgs("foo bar fee bar baz", "(?:f(..) bar )*", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  static absl::Status ConsumePrefixArgs(std::string_view *input, std::string_view pattern,
                                        Args *const... args);

  // Searches the `input` string for a substring matching the `pattern` regular expression and
  // returns a new string with the substring replaced by `replacement`. The `input`, `pattern`, and
  // `replacement` strings are not modified.
  //
  // Example usage:
  //
  //   auto const status_or_string = RE::StrReplaceFirst("foo bar baz", "bar", "qux");
  //   assert(status_or_string.ok() && *status_or_string == "foo qux baz");
  //
  // This function works by wrapping the whole `pattern` in round brackets, therefore creating the
  // first top-level capture group, compiling the resulting regular expression, and invoking its
  // non-static version. See `StrReplaceFirst` below for more details.
  static absl::StatusOr<std::string> StrReplaceFirst(std::string_view input,
                                                     std::string_view pattern,
                                                     std::string_view replacement);

  // Like `StrReplaceFirst` but replaces all substrings matching the `pattern`, not just the first.
  //
  // This function works by running the `StrReplaceFirst` algorithm repeatedly, so if the `input`
  // string has a cluster of overlapping matches only the first match in the cluster is replaced.
  static absl::StatusOr<std::string> StrReplaceAll(std::string_view input, std::string_view pattern,
                                                   std::string_view replacement);

  // Compiles the provided `pattern` into a `RE` object that can be run efficiently multiple times.
  static absl::StatusOr<RE> Create(std::string_view pattern, Options const &options = {});

  // Compiles the provided `pattern` into a `RE` object that can be run efficiently multiple times.
  //
  // WARNING: this function checkfails and crashes the process if `pattern` fails to compile, so you
  // must be absolutely sure that it compiles correctly. You should only pass hard-coded regular
  // expression patterns and never use user-provided ones.
  static RE CreateOrDie(std::string_view pattern, Options const &options = {});

  ~RE() = default;

  // Moves and copies are efficient because the inner automaton is immutable and is managed by means
  // of reference counting, so we never move or copy the whole automaton, just a pointer to it.
  //
  // For reference counting the automaton uses `SimpleRefCounted` which is lock-free and
  // thread-safe, so it's okay for different threads to keep `RE` objects referring to the same
  // automaton. It is however not okay for different threads to use the same `RE` object without any
  // synchronization. Only the internal automaton is thread-safe, `RE` is not.

  RE(RE const &) = default;
  RE &operator=(RE const &) = default;
  RE(RE &&) noexcept = default;
  RE &operator=(RE &&) noexcept = default;

  void swap(RE &other) noexcept { automaton_.swap(other.automaton_); }
  friend void swap(RE &lhs, RE &rhs) noexcept { lhs.swap(rhs); }

  // Indicates whether the underlying automaton is deterministic.
  bool IsDeterministic() const { return automaton_->IsDeterministic(); }

  // Returns the size of the automaton expressed as the number of states (first component of the
  // returned pair) and total number of edges (second component).
  std::pair<size_t, size_t> GetSize() const { return automaton_->GetSize(); }

  // Returns the number of capture groups in the regular expression. That is also the size of the
  // vector returned by `Match` for a matching input string.
  size_t GetNumCaptureGroups() const { return automaton_->GetNumCaptureGroups(); }

  // Checks if the provided `input` string matches this compiled regular expression.
  bool Test(std::string_view const input) const { return automaton_->Test(input); }

  // Checks if the regular expression matches a prefix of the provided `input` string.
  bool TestPrefix(std::string_view const input) const { return automaton_->TestPrefix(input); }

  // Checks if the `input` string contains a substring matching this compiled regular expression.
  bool ContainedIn(std::string_view const input) const { return automaton_->PartialTest(input); }

  // Checks if `input` matches this regular expression and returns an array of the strings captured
  // by the capture groups. An empty optional is returned if `input` doesn't match.
  std::optional<CaptureSet> Match(std::string_view const input) const {
    return automaton_->Match(input);
  }

  // Same as `Match` above but stores the captured substrings in the provided `std::string_view`
  // objects rather than returning a `CaptureSet`. Returns `true` if the `input` matched, `false` if
  // the `pattern` didn't compile or the `input` didn't match. The contents of the string views are
  // undefined if false is returned.
  //
  // Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("blah (blah)"));
  //   std::string_view blah;
  //   CHECK(re.MatchArgs("blah blah", &blah));
  //   LOG(INFO) << blah;  // logs "blah"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("(?:f(..) bar )*"));
  //   std::string_view sv;
  //   CHECK(re.MatchArgs("foo bar fee bar ", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  bool MatchArgs(std::string_view const input, Args *const... args) const {
    return automaton_->MatchArgs(input, {args...});
  }

  // Matches the longest possible prefix of the provided string against this regular expression.
  // Returns the array of strings captured by any capture groups, or an empty optional if no
  // matching prefix is found.
  std::optional<CaptureSet> MatchPrefix(std::string_view const input) const {
    return automaton_->MatchPrefix(input);
  }

  // Same as `MatchPrefix` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns `true` if a prefix of
  // the `input` matched, or `false` if the `pattern` didn't compile or no prefix matched. The
  // contents of the string views are undefined if false is returned.
  //
  // Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("(blah)"));
  //   std::string_view blah;
  //   CHECK(re.MatchPrefixArgs("blah blah", &blah));
  //   LOG(INFO) << blah;  // logs "blah"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("(?:f(..) bar )*"));
  //   std::string_view sv;
  //   CHECK(re.MatchPrefixArgs("foo bar fee bar ", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  bool MatchPrefixArgs(std::string_view const input, Args *const... args) const {
    return automaton_->MatchPrefixArgs(input, {args...});
  }

  // Searches for a substring of the `input` string matching this regular expression. The returned
  // match is guaranteed to be the earliest and longest in the input string, with earliest matches
  // taking precedence over longer ones.
  //
  // NOTE: unlike other regular expression implementations this function does not return the full
  // match in the first capture group. If you need that information you need to surround the whole
  // expression in a capture group before compiling it.
  std::optional<CaptureSet> PartialMatch(std::string_view const input) const {
    return automaton_->PartialMatch(input);
  }

  // Same as `PartialMatch` above but stores the captured substrings in the provided
  // `std::string_view` objects rather than returning a `CaptureSet`. Returns `true` if a substring
  // of `input` matched, or `false` if the `pattern` didn't compile or no substring matched. The
  // contents of the string views are undefined if false is returned.
  //
  // Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("(blah)"));
  //   std::string_view blah;
  //   CHECK(re.PartialMatchArgs("blah blah", &blah));
  //   LOG(INFO) << blah;  // logs "blah"
  //
  // NOTE: only one substring is retrieved for each capture group. If the corresponding capture
  // group matched more than one substring, only the last one is returned. Example:
  //
  //   ASSIGN_VAR_OR_RETURN(RE, re, RE::Create("(?:f(..) bar )*"));
  //   std::string_view sv;
  //   CHECK(re.PartialMatchArgs("foo bar fee bar ", &sv));
  //   LOG(INFO) << sv;  // logs "ee", not "oo".
  //
  // NOTE: normally there would be as many `args` as there are capture groups in the `pattern`, but
  // it's okay to provide fewer or more: missing substrings won't be retrieved and extra string
  // views will be ignored.
  template <typename... Args>
  bool PartialMatchArgs(std::string_view const input, Args *const... args) const {
    return automaton_->PartialMatchArgs(input, {args...});
  }

  // Searches the `input` string for a substring matching this regular expression, identifies the
  // i-th capture group (with i == `capture_index`), and returns a new string with the captured
  // substring replaced by `replacement`. Neither the `input` string nor the `replacement` string
  // are modified.
  //
  // Example usage:
  //
  //   RE const pattern = RE::CreateOrDie("foo (bar) baz");
  //   auto const status_or_string = pattern.StrReplaceFirst("foo bar baz", 0, "qux");
  //   assert(status_or_string.ok() && *status_or_string == "foo qux baz");
  //
  // Since `StrReplaceFirst` performs a partial match, the pattern doesn't have to match the entire
  // input string. The following would also work:
  //
  //   RE const pattern = RE::CreateOrDie("o (bar) b");
  //   auto const status_or_string = pattern.StrReplaceFirst("foo bar baz", 0, "qux");
  //   assert(status_or_string.ok() && *status_or_string == "foo qux baz");
  //
  // The most common use case is to wrap the whole pattern in a captuing group. Example:
  //
  //   RE const pattern = RE::CreateOrDie("(opera)");
  //   auto const status_or_string = pattern.StrReplaceFirst(
  //       "sator opera tenet opera rotas", 0, "arepo");
  //   assert(status_or_string.ok());
  //   assert(*status_or_string == "sator arepo tenet arepo rotas");
  //
  // `capture_index` is zero-based. If it's invalid (i.e. it refers to a capture group that doesn't
  // exist), an error status is returned.
  //
  // If no match is found this function simply returns a copy of the input string. If more than one
  // match exists this function is guaranteed to find and replace the first occurring one and to
  // match as many characters as possible. Use `StrReplaceAll` if you need to replace all
  // occurrences.
  //
  // The `replacement` string may contain references to capture groups found in the regular
  // expression. The syntax for a reference to the n-th capture group (with n being zero-based) is
  // "\n", e.g. "\0", "\1", "\2", etc. If a capture group matched multiple substrings the last one
  // is used. If a reference references a capture group that doesn't exist, e.g. if `replacement`
  // contains "\4" and there are only 3 capture groups, an error status is returned.
  //
  // If you want your replacement string to contain a literal sequence that looks like a reference
  // code, e.g. if you want it to literally contain "\2" without replacing it with the second
  // capture group, simply use a double backslash ("\\2").
  //
  // Note that if you're hard-coding the `replacement` string in a C/C++ string literal you'll need
  // to escape all backslashes, so the syntax for a reference code will be "\\n" ("\\0", "\\1",
  // etc.) and in order to include a literal sequence that looks like a reference code you'll need
  // to use four backslashes ("\\\\0", "\\\\1", etc.).
  absl::StatusOr<std::string> StrReplaceFirst(std::string_view input, size_t capture_index,
                                              std::string_view replacement) const;

  // This `StrReplaceFirst` overload defaults the `capture_index` parameter to 0.
  //
  // NOTE: the capture group must still be present in the regular expression pattern, otherwise this
  // method will always return an error status.
  absl::StatusOr<std::string> StrReplaceFirst(std::string_view input,
                                              std::string_view replacement) const {
    return StrReplaceFirst(input, /*capture_index=*/0, replacement);
  }

  // Like `StrReplaceFirst` but replaces all substrings matching the regular expression, not just
  // the first.
  //
  // This function works by running the `StrReplaceFirst` algorithm repeatedly, so if the `input`
  // string has a cluster of overlapping matches only the first match in the cluster is replaced.
  absl::StatusOr<std::string> StrReplaceAll(std::string_view input, size_t capture_index,
                                            std::string_view replacement) const;

  // This `StrReplaceAll` overload defaults the `capture_index` parameter to 0.
  //
  // NOTE: the capture group must still be present in the regular expression pattern, otherwise this
  // method will always return an error status.
  absl::StatusOr<std::string> StrReplaceAll(std::string_view input,
                                            std::string_view replacement) const {
    return StrReplaceAll(input, /*capture_index=*/0, replacement);
  }

 private:
  template <typename Label, typename Allocator>
  friend class internal::TrieNode;

  static absl::StatusOr<std::string> SubstituteRefs(
      std::string_view input, std::string_view replacement,
      absl::Span<std::pair<intptr_t, intptr_t> const> ranges);

  explicit RE(reffed_ptr<regexp_internal::AbstractAutomaton> automaton)
      : automaton_(std::move(automaton)) {}

  reffed_ptr<regexp_internal::AbstractAutomaton> automaton_;
};

template <typename... Args>
absl::Status RE::MatchArgs(std::string_view const input, std::string_view const pattern,
                           Args *const... args) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(pattern));
  if (re.MatchArgs(input, args...)) {
    return absl::OkStatus();
  } else {
    return absl::NotFoundError(absl::StrCat("string \"", absl::CEscape(internal::ClipString(input)),
                                            "\" doesn't match \"", absl::CEscape(pattern), "\""));
  }
}

template <typename... Args>
absl::Status RE::PartialMatchArgs(std::string_view const input, std::string_view const pattern,
                                  Args *const... args) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(pattern));
  if (re.PartialMatchArgs(input, args...)) {
    return absl::OkStatus();
  } else {
    return absl::NotFoundError(absl::StrCat("no substring matching \"", absl::CEscape(pattern),
                                            "\" found in \"", absl::CEscape(input), "\""));
  }
}

template <typename... Args>
absl::Status RE::ConsumePrefixArgs(std::string_view *const input, std::string_view const pattern,
                                   Args *const... args) {
  DEFINE_CONST_OR_RETURN(re, RE::Create(absl::StrCat("(", pattern, ")")));
  std::string_view prefix;
  if (!re.MatchPrefixArgs(*input, &prefix, args...)) {
    return absl::NotFoundError(absl::StrCat("no prefix matching \"", absl::CEscape(pattern),
                                            "\" found in \"",
                                            absl::CEscape(internal::ClipString(*input)), "\""));
  }
  input->remove_prefix(prefix.size());
  return absl::OkStatus();
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_H__
