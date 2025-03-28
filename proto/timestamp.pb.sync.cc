#include "proto/timestamp.pb.sync.h"

#include <cstddef>
#include <cstdint>
#include <functional>
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

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<Timestamp> Timestamp::Decode(::absl::Span<uint8_t const> const data) {
  Timestamp proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt64Field(tag.wire_type));
        proto.seconds.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.nanos.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord Timestamp::Encode(Timestamp const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.seconds.has_value()) {
    encoder.EncodeInt64Field(1, proto.seconds.value());
  }
  if (proto.nanos.has_value()) {
    encoder.EncodeInt32Field(2, proto.nanos.value());
  }
  return std::move(encoder).Finish();
}

::tsdb2::proto::MessageDescriptor<Timestamp, 2> const Timestamp::MESSAGE_DESCRIPTOR{{
    {"seconds", &Timestamp::seconds},
    {"nanos", &Timestamp::nanos},
}};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
