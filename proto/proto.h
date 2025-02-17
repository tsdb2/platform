#ifndef __TSDB2_PROTO_PROTO_H__
#define __TSDB2_PROTO_PROTO_H__

#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace tsdb2 {
namespace proto {

// Base class for all protobuf messages.
struct Message {};

template <typename Value>
absl::StatusOr<Value const*> RequireField(std::optional<Value> const& field) {
  if (field.has_value()) {
    return absl::StatusOr<Value const*>(&field.value());
  } else {
    return absl::InvalidArgumentError("missing required field");
  }
}

#define REQUIRE_FIELD_OR_RETURN(name, object, field)                     \
  auto const& maybe_##name = (object).field;                             \
  if (!maybe_##name.has_value()) {                                       \
    return absl::InvalidArgumentError("missing required field " #field); \
  }                                                                      \
  auto const& name = maybe_##name.value();

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_PROTO_H__
