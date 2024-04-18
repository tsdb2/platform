#ifndef __TSDB2_TSZ_TYPES_H__
#define __TSDB2_TSZ_TYPES_H__

#include <cstdint>
#include <string>
#include <variant>

#include "common/flat_map.h"

namespace tsz {

using FieldValue = std::variant<bool, int64_t, std::string>;
using FieldMap = tsdb2::common::flat_map<std::string, FieldValue>;

}  // namespace tsz

#endif  // __TSDB2_TSZ_TYPES_H__
