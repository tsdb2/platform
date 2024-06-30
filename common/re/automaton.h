#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <memory>
#include <string_view>

namespace tsdb2 {
namespace common {
namespace regexp_internal {

class AutomatonInterface {
 public:
  virtual ~AutomatonInterface() = default;

  virtual std::unique_ptr<AutomatonInterface> Clone() const = 0;

  virtual bool Run(std::string_view input) const = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_AUTOMATON_H__
