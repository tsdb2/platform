#include "proto/dependency_mapping.pb.sync.h"

#include "proto/runtime.h"

namespace tsdb2::proto::internal {

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
        proto.cc_header.emplace_back(std::move(value));
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
  for (auto const& value : proto.cc_header) {
    encoder.EncodeStringField(1, value);
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DependencyMapping::Dependency* const proto) {
  *proto = DependencyMapping::Dependency();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "cc_header") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->cc_header.emplace_back(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

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
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::tsdb2::proto::internal::DependencyMapping::Dependency::Decode(child_span));
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
    encoder.EncodeSubMessageField(
        2, ::tsdb2::proto::internal::DependencyMapping::Dependency::Encode(proto.value.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DependencyMapping::DependencyEntry* const proto) {
  *proto = DependencyMapping::DependencyEntry();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "key") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->key.emplace(std::move(value));
    } else if (field_name == "value") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message,
          parser->ParseSubMessage<::tsdb2::proto::internal::DependencyMapping::Dependency>());
      proto->value.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
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
        RETURN_IF_ERROR(
            decoder.DecodeMapEntry<::tsdb2::proto::internal::DependencyMapping::DependencyEntry>(
                tag.wire_type, &proto.dependency));
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
  for (auto const& [key, value] : proto.dependency) {
    encoder.EncodeSubMessageField(
        1, ::tsdb2::proto::internal::DependencyMapping::DependencyEntry::Encode(
               {.key = key, .value = value}));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DependencyMapping* const proto) {
  *proto = DependencyMapping();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "dependency") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message,
          parser->ParseSubMessage<::tsdb2::proto::internal::DependencyMapping::DependencyEntry>());
      if (!message.key.has_value()) {
        message.key.emplace();
      }
      if (!message.value.has_value()) {
        message.value.emplace();
      }
      proto->dependency.try_emplace(std::move(message.key).value(),
                                    std::move(message.value).value());
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace tsdb2::proto::internal
