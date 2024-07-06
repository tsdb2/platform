#ifndef __TSDB2_COMMON_RE_AUTOMATON_H__
#define __TSDB2_COMMON_RE_AUTOMATON_H__

#include <string_view>

#include "common/ref_count.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

class AutomatonInterface : public SimpleRefCounted {
 public:
  ~AutomatonInterface() override = default;

  virtual bool Run(std::string_view input) const = 0;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_AUTOMATON_H__
