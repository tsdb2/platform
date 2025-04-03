#include "proto/annotations.pb.sync.h"

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
#include "proto/descriptor.pb.sync.h"
#include "proto/proto.h"
#include "proto/wire_format.h"

namespace tsdb2::proto {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<google_protobuf_FieldOptions_extension>
google_protobuf_FieldOptions_extension::Decode(::absl::Span<uint8_t const> const data) {
  google_protobuf_FieldOptions_extension proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 71104: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::tsdb2::proto::FieldIndirectionType>(tag.wire_type));
        proto.indirect.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord google_protobuf_FieldOptions_extension::Encode(
    google_protobuf_FieldOptions_extension const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.indirect.has_value()) {
    encoder.EncodeEnumField(71104, proto.indirect.value());
  }
  return std::move(encoder).Finish();
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace tsdb2::proto
