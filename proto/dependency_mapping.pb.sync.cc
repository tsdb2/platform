#include "proto/dependency_mapping.pb.sync.h"

#include "proto/runtime.h"

namespace tsdb2::proto::internal {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<DependencyMapping::DependencyEntry> DependencyMapping::DependencyEntry::Decode(
    ::absl::Span<uint8_t const> const data) {
  DependencyMapping::DependencyEntry proto;
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

::tsdb2::io::Cord DependencyMapping::DependencyEntry::Encode(
    DependencyMapping::DependencyEntry const& proto) {
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
        DEFINE_VAR_OR_RETURN(
            value,
            ::tsdb2::proto::internal::DependencyMapping::DependencyEntry::Decode(child_span));
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
    encoder.EncodeSubMessageField(
        1, ::tsdb2::proto::internal::DependencyMapping::DependencyEntry::Encode(value));
  }
  return std::move(encoder).Finish();
}

::tsdb2::proto::MessageDescriptor<DependencyMapping::DependencyEntry, 2> const
    DependencyMapping::DependencyEntry::MESSAGE_DESCRIPTOR{{
        {"key", &DependencyMapping::DependencyEntry::key},
        {"value", &DependencyMapping::DependencyEntry::value},
    }};

::tsdb2::proto::MessageDescriptor<DependencyMapping, 1> const DependencyMapping::MESSAGE_DESCRIPTOR{
    {
        {"dependency",
         ::tsdb2::proto::RepeatedSubMessageField<DependencyMapping>(
             &DependencyMapping::dependency,
             ::tsdb2::proto::internal::DependencyMapping::DependencyEntry::MESSAGE_DESCRIPTOR)},
    }};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace tsdb2::proto::internal
