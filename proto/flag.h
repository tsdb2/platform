#ifndef __TSDB2_PROTO_FLAG_H__
#define __TSDB2_PROTO_FLAG_H__

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "proto/reflection.h"
#include "proto/text_format.h"

namespace tsdb2 {
namespace proto {

template <typename Proto, std::enable_if_t<HasProtoReflectionV<Proto>, bool> = true>
bool AbslParseFlag(std::string_view const text, Proto* const value, std::string* const error) {
  auto status_or_value = text::Parse<Proto>(text);
  if (status_or_value.ok()) {
    *value = std::move(status_or_value).value();
    return true;
  } else {
    *error = status_or_value.status().ToString();
    return false;
  }
}

template <typename Proto, std::enable_if_t<HasProtoReflectionV<Proto>, bool> = true>
std::string AbslUnparseFlag(Proto const& value) {
  return text::Stringify(value, {.compressed = true});
}

}  // namespace proto
}  // namespace tsdb2

#endif  // TSDB2_PROTO_FLAG_H__
