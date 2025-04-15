#include "proto/timestamp.pb.sync.h"

#include "proto/runtime.h"

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<Timestamp> Timestamp::Decode(::absl::Span<uint8_t const> const data) {
  Timestamp proto;
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

::absl::StatusOr<Timestamp> Timestamp::Parse(::absl::Nonnull<std::string_view*> const text) {
  Timestamp proto;
  ::tsdb2::proto::text::Parser parser{*text, /*options=*/{}};
  parser.ConsumeSeparators();
  while (true) {
    auto const maybe_field_name = parser.ParseFieldName();
    if (maybe_field_name.has_value()) {
      break;
    }
    auto const& field_name = maybe_field_name.value();
    if (field_name == "seconds") {
      DEFINE_CONST_OR_RETURN(value, parser.ParseInteger<int64_t>());
      proto.seconds.emplace(value);
    } else if (field_name == "nanos") {
      DEFINE_CONST_OR_RETURN(value, parser.ParseInteger<int32_t>());
      proto.nanos.emplace(value);
    } else {
      RETURN_IF_ERROR(parser.SkipField());
    }
    parser.ConsumeFieldSeparators();
  }
  *text = std::move(parser).remainder();
  return std::move(proto);
}

std::string Timestamp::Stringify(Timestamp const& proto) {
  // TODO
  return "";
}

::tsdb2::proto::MessageDescriptor<Timestamp, /*num_fields=*/2, /*num_required_fields=*/0> const
    Timestamp::MESSAGE_DESCRIPTOR{{
        {"seconds", &Timestamp::seconds},
        {"nanos", &Timestamp::nanos},
    }};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
