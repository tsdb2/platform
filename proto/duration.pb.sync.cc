#include "proto/duration.pb.sync.h"

#include "proto/runtime.h"

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<Duration> Duration::Decode(::absl::Span<uint8_t const> const data) {
  Duration proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
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

::tsdb2::io::Cord Duration::Encode(Duration const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.seconds.has_value()) {
    encoder.EncodeInt64Field(1, proto.seconds.value());
  }
  if (proto.nanos.has_value()) {
    encoder.EncodeInt32Field(2, proto.nanos.value());
  }
  return std::move(encoder).Finish();
}

::tsdb2::proto::MessageDescriptor<Duration, 2> const Duration::MESSAGE_DESCRIPTOR{{
    {"seconds", &Duration::seconds},
    {"nanos", &Duration::nanos},
}};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
