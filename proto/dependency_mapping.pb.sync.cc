#include "proto/dependency_mapping.pb.sync.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/flat_set.h"
#include "common/utilities.h"
#include "io/cord.h"
#include "proto/proto.h"
#include "proto/wire_format.h"

namespace tsdb2::proto {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<DependencyMapping::Dependency> DependencyMapping::Dependency::Decode(
    ::absl::Span<uint8_t const> const data) {
  DependencyMapping::Dependency proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.key.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.value.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord DependencyMapping::Dependency::Encode(
    DependencyMapping::Dependency const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.key.has_value()) {
    encoder.EncodeStringField(1, proto.key.value());
  }
  if (proto.value.has_value()) {
    encoder.EncodeStringField(2, proto.value.value());
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<DependencyMapping> DependencyMapping::Decode(
    ::absl::Span<uint8_t const> const data) {
  DependencyMapping proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::tsdb2::proto::DependencyMapping::Dependency::Decode(child_span));
        proto.dependency.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord DependencyMapping::Encode(DependencyMapping const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.dependency) {
    encoder.EncodeSubMessageField(1, ::tsdb2::proto::DependencyMapping::Dependency::Encode(value));
  }
  return std::move(encoder).Finish();
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace tsdb2::proto
