#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <string_view>

#include "common/ref_count.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Abstract interface of a finite state automaton that recognizes a regular expression language.
// `NFA` and `DFA` inherit this class.
class AutomatonInterface : public SimpleRefCounted {
 public:
  explicit AutomatonInterface() = default;
  ~AutomatonInterface() override = default;

  // Returns true if this automaton is a DFA, false if it's an NFA.
  virtual bool IsDeterministic() const = 0;

  // Runs the automaton on the given `input` string and returns true iff the string matches.
  virtual bool Run(std::string_view input) const = 0;

 private:
  // Copies are not needed: automata are immutable and reference-countable.
  AutomatonInterface(AutomatonInterface const &) = delete;
  AutomatonInterface &operator=(AutomatonInterface const &) = delete;

  // Moves are forbidden: we need pointer stability for several reasons, e.g. we need to manage the
  // reference count, runners keep a pointer to the parent automaton, etc.
  AutomatonInterface(AutomatonInterface &&) = delete;
  AutomatonInterface &operator=(AutomatonInterface &&) = delete;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_AUTOMATON_H__
