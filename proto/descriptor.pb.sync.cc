#include "proto/descriptor.pb.sync.h"

#include "proto/runtime.h"

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Edition* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, Edition>({
          {"EDITION_UNKNOWN", Edition::EDITION_UNKNOWN},
          {"EDITION_LEGACY", Edition::EDITION_LEGACY},
          {"EDITION_PROTO2", Edition::EDITION_PROTO2},
          {"EDITION_PROTO3", Edition::EDITION_PROTO3},
          {"EDITION_2023", Edition::EDITION_2023},
          {"EDITION_2024", Edition::EDITION_2024},
          {"EDITION_1_TEST_ONLY", Edition::EDITION_1_TEST_ONLY},
          {"EDITION_2_TEST_ONLY", Edition::EDITION_2_TEST_ONLY},
          {"EDITION_99997_TEST_ONLY", Edition::EDITION_99997_TEST_ONLY},
          {"EDITION_99998_TEST_ONLY", Edition::EDITION_99998_TEST_ONLY},
          {"EDITION_99999_TEST_ONLY", Edition::EDITION_99999_TEST_ONLY},
          {"EDITION_MAX", Edition::EDITION_MAX},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                Edition const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<Edition, std::string_view>({
          {Edition::EDITION_UNKNOWN, "EDITION_UNKNOWN"},
          {Edition::EDITION_LEGACY, "EDITION_LEGACY"},
          {Edition::EDITION_PROTO2, "EDITION_PROTO2"},
          {Edition::EDITION_PROTO3, "EDITION_PROTO3"},
          {Edition::EDITION_2023, "EDITION_2023"},
          {Edition::EDITION_2024, "EDITION_2024"},
          {Edition::EDITION_1_TEST_ONLY, "EDITION_1_TEST_ONLY"},
          {Edition::EDITION_2_TEST_ONLY, "EDITION_2_TEST_ONLY"},
          {Edition::EDITION_99997_TEST_ONLY, "EDITION_99997_TEST_ONLY"},
          {Edition::EDITION_99998_TEST_ONLY, "EDITION_99998_TEST_ONLY"},
          {Edition::EDITION_99999_TEST_ONLY, "EDITION_99999_TEST_ONLY"},
          {Edition::EDITION_MAX, "EDITION_MAX"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<FileDescriptorSet> FileDescriptorSet::Decode(
    ::absl::Span<uint8_t const> const data) {
  FileDescriptorSet proto;
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
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FileDescriptorProto::Decode(child_span));
        proto.file.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FileDescriptorSet::Encode(FileDescriptorSet const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.file) {
    encoder.EncodeSubMessageField(1, ::google::protobuf::FileDescriptorProto::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FileDescriptorSet* const proto) {
  *proto = FileDescriptorSet();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "file") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FileDescriptorProto>());
      proto->file.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<FileDescriptorProto> FileDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  FileDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.package.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.dependency.emplace_back(std::move(value));
      } break;
      case 10: {
        RETURN_IF_ERROR(decoder.DecodeRepeatedInt32s(tag.wire_type, &proto.public_dependency));
      } break;
      case 11: {
        RETURN_IF_ERROR(decoder.DecodeRepeatedInt32s(tag.wire_type, &proto.weak_dependency));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::DescriptorProto::Decode(child_span));
        proto.message_type.emplace_back(std::move(value));
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::EnumDescriptorProto::Decode(child_span));
        proto.enum_type.emplace_back(std::move(value));
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::ServiceDescriptorProto::Decode(child_span));
        proto.service.emplace_back(std::move(value));
      } break;
      case 7: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FieldDescriptorProto::Decode(child_span));
        proto.extension.emplace_back(std::move(value));
      } break;
      case 8: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FileOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      case 9: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::SourceCodeInfo::Decode(child_span));
        proto.source_code_info.emplace(std::move(value));
      } break;
      case 12: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.syntax.emplace(std::move(value));
      } break;
      case 14: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FileDescriptorProto::Encode(FileDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.package.has_value()) {
    encoder.EncodeStringField(2, proto.package.value());
  }
  for (auto const& value : proto.dependency) {
    encoder.EncodeStringField(3, value);
  }
  for (auto const& value : proto.public_dependency) {
    encoder.EncodeInt32Field(10, value);
  }
  for (auto const& value : proto.weak_dependency) {
    encoder.EncodeInt32Field(11, value);
  }
  for (auto const& value : proto.message_type) {
    encoder.EncodeSubMessageField(4, ::google::protobuf::DescriptorProto::Encode(value));
  }
  for (auto const& value : proto.enum_type) {
    encoder.EncodeSubMessageField(5, ::google::protobuf::EnumDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.service) {
    encoder.EncodeSubMessageField(6, ::google::protobuf::ServiceDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension) {
    encoder.EncodeSubMessageField(7, ::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(8,
                                  ::google::protobuf::FileOptions::Encode(proto.options.value()));
  }
  if (proto.source_code_info.has_value()) {
    encoder.EncodeSubMessageField(
        9, ::google::protobuf::SourceCodeInfo::Encode(proto.source_code_info.value()));
  }
  if (proto.syntax.has_value()) {
    encoder.EncodeStringField(12, proto.syntax.value());
  }
  if (proto.edition.has_value()) {
    encoder.EncodeEnumField(14, proto.edition.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FileDescriptorProto* const proto) {
  *proto = FileDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "package") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->package.emplace(std::move(value));
    } else if (field_name == "dependency") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->dependency.emplace_back(std::move(value));
    } else if (field_name == "public_dependency") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->public_dependency.emplace_back(value);
    } else if (field_name == "weak_dependency") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->weak_dependency.emplace_back(value);
    } else if (field_name == "message_type") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::DescriptorProto>());
      proto->message_type.emplace_back(std::move(message));
    } else if (field_name == "enum_type") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::EnumDescriptorProto>());
      proto->enum_type.emplace_back(std::move(message));
    } else if (field_name == "service") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::ServiceDescriptorProto>());
      proto->service.emplace_back(std::move(message));
    } else if (field_name == "extension") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FieldDescriptorProto>());
      proto->extension.emplace_back(std::move(message));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FileOptions>());
      proto->options.emplace(std::move(message));
    } else if (field_name == "source_code_info") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::SourceCodeInfo>());
      proto->source_code_info.emplace(std::move(message));
    } else if (field_name == "syntax") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->syntax.emplace(std::move(value));
    } else if (field_name == "edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<DescriptorProto::ExtensionRange> DescriptorProto::ExtensionRange::Decode(
    ::absl::Span<uint8_t const> const data) {
  DescriptorProto::ExtensionRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.start.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.end.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::ExtensionRangeOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord DescriptorProto::ExtensionRange::Encode(
    DescriptorProto::ExtensionRange const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.start.has_value()) {
    encoder.EncodeInt32Field(1, proto.start.value());
  }
  if (proto.end.has_value()) {
    encoder.EncodeInt32Field(2, proto.end.value());
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(
        3, ::google::protobuf::ExtensionRangeOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DescriptorProto::ExtensionRange* const proto) {
  *proto = DescriptorProto::ExtensionRange();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "start") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->start.emplace(value);
    } else if (field_name == "end") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->end.emplace(value);
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::ExtensionRangeOptions>());
      proto->options.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<DescriptorProto::ReservedRange> DescriptorProto::ReservedRange::Decode(
    ::absl::Span<uint8_t const> const data) {
  DescriptorProto::ReservedRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.start.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.end.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord DescriptorProto::ReservedRange::Encode(
    DescriptorProto::ReservedRange const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.start.has_value()) {
    encoder.EncodeInt32Field(1, proto.start.value());
  }
  if (proto.end.has_value()) {
    encoder.EncodeInt32Field(2, proto.end.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DescriptorProto::ReservedRange* const proto) {
  *proto = DescriptorProto::ReservedRange();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "start") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->start.emplace(value);
    } else if (field_name == "end") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->end.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<DescriptorProto> DescriptorProto::Decode(::absl::Span<uint8_t const> const data) {
  DescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FieldDescriptorProto::Decode(child_span));
        proto.field.emplace_back(std::move(value));
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FieldDescriptorProto::Decode(child_span));
        proto.extension.emplace_back(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::DescriptorProto::Decode(child_span));
        proto.nested_type.emplace_back(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::EnumDescriptorProto::Decode(child_span));
        proto.enum_type.emplace_back(std::move(value));
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::google::protobuf::DescriptorProto::ExtensionRange::Decode(child_span));
        proto.extension_range.emplace_back(std::move(value));
      } break;
      case 8: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::OneofDescriptorProto::Decode(child_span));
        proto.oneof_decl.emplace_back(std::move(value));
      } break;
      case 7: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::MessageOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      case 9: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::google::protobuf::DescriptorProto::ReservedRange::Decode(child_span));
        proto.reserved_range.emplace_back(std::move(value));
      } break;
      case 10: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.reserved_name.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord DescriptorProto::Encode(DescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  for (auto const& value : proto.field) {
    encoder.EncodeSubMessageField(2, ::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension) {
    encoder.EncodeSubMessageField(6, ::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.nested_type) {
    encoder.EncodeSubMessageField(3, ::google::protobuf::DescriptorProto::Encode(value));
  }
  for (auto const& value : proto.enum_type) {
    encoder.EncodeSubMessageField(4, ::google::protobuf::EnumDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension_range) {
    encoder.EncodeSubMessageField(
        5, ::google::protobuf::DescriptorProto::ExtensionRange::Encode(value));
  }
  for (auto const& value : proto.oneof_decl) {
    encoder.EncodeSubMessageField(8, ::google::protobuf::OneofDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(
        7, ::google::protobuf::MessageOptions::Encode(proto.options.value()));
  }
  for (auto const& value : proto.reserved_range) {
    encoder.EncodeSubMessageField(
        9, ::google::protobuf::DescriptorProto::ReservedRange::Encode(value));
  }
  for (auto const& value : proto.reserved_name) {
    encoder.EncodeStringField(10, value);
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               DescriptorProto* const proto) {
  *proto = DescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "field") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FieldDescriptorProto>());
      proto->field.emplace_back(std::move(message));
    } else if (field_name == "extension") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FieldDescriptorProto>());
      proto->extension.emplace_back(std::move(message));
    } else if (field_name == "nested_type") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::DescriptorProto>());
      proto->nested_type.emplace_back(std::move(message));
    } else if (field_name == "enum_type") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::EnumDescriptorProto>());
      proto->enum_type.emplace_back(std::move(message));
    } else if (field_name == "extension_range") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::DescriptorProto::ExtensionRange>());
      proto->extension_range.emplace_back(std::move(message));
    } else if (field_name == "oneof_decl") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::OneofDescriptorProto>());
      proto->oneof_decl.emplace_back(std::move(message));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::MessageOptions>());
      proto->options.emplace(std::move(message));
    } else if (field_name == "reserved_range") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::DescriptorProto::ReservedRange>());
      proto->reserved_range.emplace_back(std::move(message));
    } else if (field_name == "reserved_name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->reserved_name.emplace_back(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               ExtensionRangeOptions::VerificationState* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view,
                                         ExtensionRangeOptions::VerificationState>({
          {"DECLARATION", ExtensionRangeOptions::VerificationState::DECLARATION},
          {"UNVERIFIED", ExtensionRangeOptions::VerificationState::UNVERIFIED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                ExtensionRangeOptions::VerificationState const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<ExtensionRangeOptions::VerificationState,
                                         std::string_view>({
          {ExtensionRangeOptions::VerificationState::DECLARATION, "DECLARATION"},
          {ExtensionRangeOptions::VerificationState::UNVERIFIED, "UNVERIFIED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<ExtensionRangeOptions::Declaration> ExtensionRangeOptions::Declaration::Decode(
    ::absl::Span<uint8_t const> const data) {
  ExtensionRangeOptions::Declaration proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.number.emplace(value);
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.full_name.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.type.emplace(std::move(value));
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.reserved.emplace(value);
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.repeated.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord ExtensionRangeOptions::Declaration::Encode(
    ExtensionRangeOptions::Declaration const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.number.has_value()) {
    encoder.EncodeInt32Field(1, proto.number.value());
  }
  if (proto.full_name.has_value()) {
    encoder.EncodeStringField(2, proto.full_name.value());
  }
  if (proto.type.has_value()) {
    encoder.EncodeStringField(3, proto.type.value());
  }
  if (proto.reserved.has_value()) {
    encoder.EncodeBoolField(5, proto.reserved.value());
  }
  if (proto.repeated.has_value()) {
    encoder.EncodeBoolField(6, proto.repeated.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               ExtensionRangeOptions::Declaration* const proto) {
  *proto = ExtensionRangeOptions::Declaration();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "number") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->number.emplace(value);
    } else if (field_name == "full_name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->full_name.emplace(std::move(value));
    } else if (field_name == "type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->type.emplace(std::move(value));
    } else if (field_name == "reserved") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->reserved.emplace(value);
    } else if (field_name == "repeated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->repeated.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<ExtensionRangeOptions> ExtensionRangeOptions::Decode(
    ::absl::Span<uint8_t const> const data) {
  ExtensionRangeOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::google::protobuf::ExtensionRangeOptions::Declaration::Decode(child_span));
        proto.declaration.emplace_back(std::move(value));
      } break;
      case 50: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::ExtensionRangeOptions::VerificationState>(
                tag.wire_type));
        proto.verification = value;
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord ExtensionRangeOptions::Encode(ExtensionRangeOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  for (auto const& value : proto.declaration) {
    encoder.EncodeSubMessageField(
        2, ::google::protobuf::ExtensionRangeOptions::Declaration::Encode(value));
  }
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(50,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeEnumField(3, proto.verification);
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               ExtensionRangeOptions* const proto) {
  *proto = ExtensionRangeOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else if (field_name == "declaration") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message,
          parser->ParseSubMessage<::google::protobuf::ExtensionRangeOptions::Declaration>());
      proto->declaration.emplace_back(std::move(message));
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "verification") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::ExtensionRangeOptions::VerificationState>());
      proto->verification = value;
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldDescriptorProto::Type* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldDescriptorProto::Type>({
          {"TYPE_DOUBLE", FieldDescriptorProto::Type::TYPE_DOUBLE},
          {"TYPE_FLOAT", FieldDescriptorProto::Type::TYPE_FLOAT},
          {"TYPE_INT64", FieldDescriptorProto::Type::TYPE_INT64},
          {"TYPE_UINT64", FieldDescriptorProto::Type::TYPE_UINT64},
          {"TYPE_INT32", FieldDescriptorProto::Type::TYPE_INT32},
          {"TYPE_FIXED64", FieldDescriptorProto::Type::TYPE_FIXED64},
          {"TYPE_FIXED32", FieldDescriptorProto::Type::TYPE_FIXED32},
          {"TYPE_BOOL", FieldDescriptorProto::Type::TYPE_BOOL},
          {"TYPE_STRING", FieldDescriptorProto::Type::TYPE_STRING},
          {"TYPE_GROUP", FieldDescriptorProto::Type::TYPE_GROUP},
          {"TYPE_MESSAGE", FieldDescriptorProto::Type::TYPE_MESSAGE},
          {"TYPE_BYTES", FieldDescriptorProto::Type::TYPE_BYTES},
          {"TYPE_UINT32", FieldDescriptorProto::Type::TYPE_UINT32},
          {"TYPE_ENUM", FieldDescriptorProto::Type::TYPE_ENUM},
          {"TYPE_SFIXED32", FieldDescriptorProto::Type::TYPE_SFIXED32},
          {"TYPE_SFIXED64", FieldDescriptorProto::Type::TYPE_SFIXED64},
          {"TYPE_SINT32", FieldDescriptorProto::Type::TYPE_SINT32},
          {"TYPE_SINT64", FieldDescriptorProto::Type::TYPE_SINT64},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldDescriptorProto::Type const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
          {FieldDescriptorProto::Type::TYPE_DOUBLE, "TYPE_DOUBLE"},
          {FieldDescriptorProto::Type::TYPE_FLOAT, "TYPE_FLOAT"},
          {FieldDescriptorProto::Type::TYPE_INT64, "TYPE_INT64"},
          {FieldDescriptorProto::Type::TYPE_UINT64, "TYPE_UINT64"},
          {FieldDescriptorProto::Type::TYPE_INT32, "TYPE_INT32"},
          {FieldDescriptorProto::Type::TYPE_FIXED64, "TYPE_FIXED64"},
          {FieldDescriptorProto::Type::TYPE_FIXED32, "TYPE_FIXED32"},
          {FieldDescriptorProto::Type::TYPE_BOOL, "TYPE_BOOL"},
          {FieldDescriptorProto::Type::TYPE_STRING, "TYPE_STRING"},
          {FieldDescriptorProto::Type::TYPE_GROUP, "TYPE_GROUP"},
          {FieldDescriptorProto::Type::TYPE_MESSAGE, "TYPE_MESSAGE"},
          {FieldDescriptorProto::Type::TYPE_BYTES, "TYPE_BYTES"},
          {FieldDescriptorProto::Type::TYPE_UINT32, "TYPE_UINT32"},
          {FieldDescriptorProto::Type::TYPE_ENUM, "TYPE_ENUM"},
          {FieldDescriptorProto::Type::TYPE_SFIXED32, "TYPE_SFIXED32"},
          {FieldDescriptorProto::Type::TYPE_SFIXED64, "TYPE_SFIXED64"},
          {FieldDescriptorProto::Type::TYPE_SINT32, "TYPE_SINT32"},
          {FieldDescriptorProto::Type::TYPE_SINT64, "TYPE_SINT64"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldDescriptorProto::Label* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldDescriptorProto::Label>({
          {"LABEL_OPTIONAL", FieldDescriptorProto::Label::LABEL_OPTIONAL},
          {"LABEL_REPEATED", FieldDescriptorProto::Label::LABEL_REPEATED},
          {"LABEL_REQUIRED", FieldDescriptorProto::Label::LABEL_REQUIRED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldDescriptorProto::Label const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Label, std::string_view>({
          {FieldDescriptorProto::Label::LABEL_OPTIONAL, "LABEL_OPTIONAL"},
          {FieldDescriptorProto::Label::LABEL_REPEATED, "LABEL_REPEATED"},
          {FieldDescriptorProto::Label::LABEL_REQUIRED, "LABEL_REQUIRED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<FieldDescriptorProto> FieldDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.number.emplace(value);
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::FieldDescriptorProto::Label>(
                       tag.wire_type));
        proto.label.emplace(value);
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FieldDescriptorProto::Type>(tag.wire_type));
        proto.type.emplace(value);
      } break;
      case 6: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.type_name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.extendee.emplace(std::move(value));
      } break;
      case 7: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.default_value.emplace(std::move(value));
      } break;
      case 9: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.oneof_index.emplace(value);
      } break;
      case 10: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.json_name.emplace(std::move(value));
      } break;
      case 8: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FieldOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      case 17: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.proto3_optional.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FieldDescriptorProto::Encode(FieldDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.number.has_value()) {
    encoder.EncodeInt32Field(3, proto.number.value());
  }
  if (proto.label.has_value()) {
    encoder.EncodeEnumField(4, proto.label.value());
  }
  if (proto.type.has_value()) {
    encoder.EncodeEnumField(5, proto.type.value());
  }
  if (proto.type_name.has_value()) {
    encoder.EncodeStringField(6, proto.type_name.value());
  }
  if (proto.extendee.has_value()) {
    encoder.EncodeStringField(2, proto.extendee.value());
  }
  if (proto.default_value.has_value()) {
    encoder.EncodeStringField(7, proto.default_value.value());
  }
  if (proto.oneof_index.has_value()) {
    encoder.EncodeInt32Field(9, proto.oneof_index.value());
  }
  if (proto.json_name.has_value()) {
    encoder.EncodeStringField(10, proto.json_name.value());
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(8,
                                  ::google::protobuf::FieldOptions::Encode(proto.options.value()));
  }
  if (proto.proto3_optional.has_value()) {
    encoder.EncodeBoolField(17, proto.proto3_optional.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FieldDescriptorProto* const proto) {
  *proto = FieldDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "number") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->number.emplace(value);
    } else if (field_name == "label") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FieldDescriptorProto::Label>());
      proto->label.emplace(value);
    } else if (field_name == "type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FieldDescriptorProto::Type>());
      proto->type.emplace(value);
    } else if (field_name == "type_name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->type_name.emplace(std::move(value));
    } else if (field_name == "extendee") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->extendee.emplace(std::move(value));
    } else if (field_name == "default_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->default_value.emplace(std::move(value));
    } else if (field_name == "oneof_index") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->oneof_index.emplace(value);
    } else if (field_name == "json_name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->json_name.emplace(std::move(value));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FieldOptions>());
      proto->options.emplace(std::move(message));
    } else if (field_name == "proto3_optional") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->proto3_optional.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<OneofDescriptorProto> OneofDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  OneofDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::OneofOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord OneofDescriptorProto::Encode(OneofDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(2,
                                  ::google::protobuf::OneofOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               OneofDescriptorProto* const proto) {
  *proto = OneofDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::OneofOptions>());
      proto->options.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<EnumDescriptorProto::EnumReservedRange>
EnumDescriptorProto::EnumReservedRange::Decode(::absl::Span<uint8_t const> const data) {
  EnumDescriptorProto::EnumReservedRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.start.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.end.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord EnumDescriptorProto::EnumReservedRange::Encode(
    EnumDescriptorProto::EnumReservedRange const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.start.has_value()) {
    encoder.EncodeInt32Field(1, proto.start.value());
  }
  if (proto.end.has_value()) {
    encoder.EncodeInt32Field(2, proto.end.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               EnumDescriptorProto::EnumReservedRange* const proto) {
  *proto = EnumDescriptorProto::EnumReservedRange();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "start") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->start.emplace(value);
    } else if (field_name == "end") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->end.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<EnumDescriptorProto> EnumDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::google::protobuf::EnumValueDescriptorProto::Decode(child_span));
        proto.value.emplace_back(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::EnumOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::google::protobuf::EnumDescriptorProto::EnumReservedRange::Decode(child_span));
        proto.reserved_range.emplace_back(std::move(value));
      } break;
      case 5: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.reserved_name.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord EnumDescriptorProto::Encode(EnumDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  for (auto const& value : proto.value) {
    encoder.EncodeSubMessageField(2, ::google::protobuf::EnumValueDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(3,
                                  ::google::protobuf::EnumOptions::Encode(proto.options.value()));
  }
  for (auto const& value : proto.reserved_range) {
    encoder.EncodeSubMessageField(
        4, ::google::protobuf::EnumDescriptorProto::EnumReservedRange::Encode(value));
  }
  for (auto const& value : proto.reserved_name) {
    encoder.EncodeStringField(5, value);
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               EnumDescriptorProto* const proto) {
  *proto = EnumDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "value") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::EnumValueDescriptorProto>());
      proto->value.emplace_back(std::move(message));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::EnumOptions>());
      proto->options.emplace(std::move(message));
    } else if (field_name == "reserved_range") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message,
          parser->ParseSubMessage<::google::protobuf::EnumDescriptorProto::EnumReservedRange>());
      proto->reserved_range.emplace_back(std::move(message));
    } else if (field_name == "reserved_name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->reserved_name.emplace_back(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<EnumValueDescriptorProto> EnumValueDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumValueDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.number.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::EnumValueOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord EnumValueDescriptorProto::Encode(EnumValueDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.number.has_value()) {
    encoder.EncodeInt32Field(2, proto.number.value());
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(
        3, ::google::protobuf::EnumValueOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               EnumValueDescriptorProto* const proto) {
  *proto = EnumValueDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "number") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->number.emplace(value);
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::EnumValueOptions>());
      proto->options.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<ServiceDescriptorProto> ServiceDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  ServiceDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::MethodDescriptorProto::Decode(child_span));
        proto.method.emplace_back(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::ServiceOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord ServiceDescriptorProto::Encode(ServiceDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  for (auto const& value : proto.method) {
    encoder.EncodeSubMessageField(2, ::google::protobuf::MethodDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(
        3, ::google::protobuf::ServiceOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               ServiceDescriptorProto* const proto) {
  *proto = ServiceDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "method") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::MethodDescriptorProto>());
      proto->method.emplace_back(std::move(message));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::ServiceOptions>());
      proto->options.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<MethodDescriptorProto> MethodDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  MethodDescriptorProto proto;
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
        proto.name.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.input_type.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.output_type.emplace(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::MethodOptions::Decode(child_span));
        proto.options.emplace(std::move(value));
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.client_streaming = value;
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.server_streaming = value;
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord MethodDescriptorProto::Encode(MethodDescriptorProto const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.input_type.has_value()) {
    encoder.EncodeStringField(2, proto.input_type.value());
  }
  if (proto.output_type.has_value()) {
    encoder.EncodeStringField(3, proto.output_type.value());
  }
  if (proto.options.has_value()) {
    encoder.EncodeSubMessageField(4,
                                  ::google::protobuf::MethodOptions::Encode(proto.options.value()));
  }
  encoder.EncodeBoolField(5, proto.client_streaming);
  encoder.EncodeBoolField(6, proto.server_streaming);
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               MethodDescriptorProto* const proto) {
  *proto = MethodDescriptorProto();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "input_type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->input_type.emplace(std::move(value));
    } else if (field_name == "output_type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->output_type.emplace(std::move(value));
    } else if (field_name == "options") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::MethodOptions>());
      proto->options.emplace(std::move(message));
    } else if (field_name == "client_streaming") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->client_streaming = value;
    } else if (field_name == "server_streaming") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->server_streaming = value;
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FileOptions::OptimizeMode* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FileOptions::OptimizeMode>({
          {"SPEED", FileOptions::OptimizeMode::SPEED},
          {"CODE_SIZE", FileOptions::OptimizeMode::CODE_SIZE},
          {"LITE_RUNTIME", FileOptions::OptimizeMode::LITE_RUNTIME},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FileOptions::OptimizeMode const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FileOptions::OptimizeMode, std::string_view>({
          {FileOptions::OptimizeMode::SPEED, "SPEED"},
          {FileOptions::OptimizeMode::CODE_SIZE, "CODE_SIZE"},
          {FileOptions::OptimizeMode::LITE_RUNTIME, "LITE_RUNTIME"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<FileOptions> FileOptions::Decode(::absl::Span<uint8_t const> const data) {
  FileOptions proto;
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
        proto.java_package.emplace(std::move(value));
      } break;
      case 8: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.java_outer_classname.emplace(std::move(value));
      } break;
      case 10: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.java_multiple_files = value;
      } break;
      case 20: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.java_generate_equals_and_hash.emplace(value);
      } break;
      case 27: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.java_string_check_utf8 = value;
      } break;
      case 9: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FileOptions::OptimizeMode>(tag.wire_type));
        proto.optimize_for = value;
      } break;
      case 11: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.go_package.emplace(std::move(value));
      } break;
      case 16: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.cc_generic_services = value;
      } break;
      case 17: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.java_generic_services = value;
      } break;
      case 18: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.py_generic_services = value;
      } break;
      case 23: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 31: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.cc_enable_arenas = value;
      } break;
      case 36: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.objc_class_prefix.emplace(std::move(value));
      } break;
      case 37: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.csharp_namespace.emplace(std::move(value));
      } break;
      case 39: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.swift_prefix.emplace(std::move(value));
      } break;
      case 40: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.php_class_prefix.emplace(std::move(value));
      } break;
      case 41: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.php_namespace.emplace(std::move(value));
      } break;
      case 44: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.php_metadata_namespace.emplace(std::move(value));
      } break;
      case 45: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.ruby_package.emplace(std::move(value));
      } break;
      case 50: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord FileOptions::Encode(FileOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.java_package.has_value()) {
    encoder.EncodeStringField(1, proto.java_package.value());
  }
  if (proto.java_outer_classname.has_value()) {
    encoder.EncodeStringField(8, proto.java_outer_classname.value());
  }
  encoder.EncodeBoolField(10, proto.java_multiple_files);
  if (proto.java_generate_equals_and_hash.has_value()) {
    encoder.EncodeBoolField(20, proto.java_generate_equals_and_hash.value());
  }
  encoder.EncodeBoolField(27, proto.java_string_check_utf8);
  encoder.EncodeEnumField(9, proto.optimize_for);
  if (proto.go_package.has_value()) {
    encoder.EncodeStringField(11, proto.go_package.value());
  }
  encoder.EncodeBoolField(16, proto.cc_generic_services);
  encoder.EncodeBoolField(17, proto.java_generic_services);
  encoder.EncodeBoolField(18, proto.py_generic_services);
  encoder.EncodeBoolField(23, proto.deprecated);
  encoder.EncodeBoolField(31, proto.cc_enable_arenas);
  if (proto.objc_class_prefix.has_value()) {
    encoder.EncodeStringField(36, proto.objc_class_prefix.value());
  }
  if (proto.csharp_namespace.has_value()) {
    encoder.EncodeStringField(37, proto.csharp_namespace.value());
  }
  if (proto.swift_prefix.has_value()) {
    encoder.EncodeStringField(39, proto.swift_prefix.value());
  }
  if (proto.php_class_prefix.has_value()) {
    encoder.EncodeStringField(40, proto.php_class_prefix.value());
  }
  if (proto.php_namespace.has_value()) {
    encoder.EncodeStringField(41, proto.php_namespace.value());
  }
  if (proto.php_metadata_namespace.has_value()) {
    encoder.EncodeStringField(44, proto.php_metadata_namespace.value());
  }
  if (proto.ruby_package.has_value()) {
    encoder.EncodeStringField(45, proto.ruby_package.value());
  }
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(50,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FileOptions* const proto) {
  *proto = FileOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "java_package") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->java_package.emplace(std::move(value));
    } else if (field_name == "java_outer_classname") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->java_outer_classname.emplace(std::move(value));
    } else if (field_name == "java_multiple_files") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->java_multiple_files = value;
    } else if (field_name == "java_generate_equals_and_hash") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->java_generate_equals_and_hash.emplace(value);
    } else if (field_name == "java_string_check_utf8") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->java_string_check_utf8 = value;
    } else if (field_name == "optimize_for") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FileOptions::OptimizeMode>());
      proto->optimize_for = value;
    } else if (field_name == "go_package") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->go_package.emplace(std::move(value));
    } else if (field_name == "cc_generic_services") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->cc_generic_services = value;
    } else if (field_name == "java_generic_services") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->java_generic_services = value;
    } else if (field_name == "py_generic_services") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->py_generic_services = value;
    } else if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "cc_enable_arenas") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->cc_enable_arenas = value;
    } else if (field_name == "objc_class_prefix") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->objc_class_prefix.emplace(std::move(value));
    } else if (field_name == "csharp_namespace") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->csharp_namespace.emplace(std::move(value));
    } else if (field_name == "swift_prefix") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->swift_prefix.emplace(std::move(value));
    } else if (field_name == "php_class_prefix") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->php_class_prefix.emplace(std::move(value));
    } else if (field_name == "php_namespace") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->php_namespace.emplace(std::move(value));
    } else if (field_name == "php_metadata_namespace") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->php_metadata_namespace.emplace(std::move(value));
    } else if (field_name == "ruby_package") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->ruby_package.emplace(std::move(value));
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<MessageOptions> MessageOptions::Decode(::absl::Span<uint8_t const> const data) {
  MessageOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.message_set_wire_format = value;
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.no_standard_descriptor_accessor = value;
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 7: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.map_entry.emplace(value);
      } break;
      case 11: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated_legacy_json_field_conflicts.emplace(value);
      } break;
      case 12: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord MessageOptions::Encode(MessageOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeBoolField(1, proto.message_set_wire_format);
  encoder.EncodeBoolField(2, proto.no_standard_descriptor_accessor);
  encoder.EncodeBoolField(3, proto.deprecated);
  if (proto.map_entry.has_value()) {
    encoder.EncodeBoolField(7, proto.map_entry.value());
  }
  if (proto.deprecated_legacy_json_field_conflicts.has_value()) {
    encoder.EncodeBoolField(11, proto.deprecated_legacy_json_field_conflicts.value());
  }
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(12,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               MessageOptions* const proto) {
  *proto = MessageOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "message_set_wire_format") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->message_set_wire_format = value;
    } else if (field_name == "no_standard_descriptor_accessor") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->no_standard_descriptor_accessor = value;
    } else if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "map_entry") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->map_entry.emplace(value);
    } else if (field_name == "deprecated_legacy_json_field_conflicts") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated_legacy_json_field_conflicts.emplace(value);
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldOptions::CType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldOptions::CType>({
          {"STRING", FieldOptions::CType::STRING},
          {"CORD", FieldOptions::CType::CORD},
          {"STRING_PIECE", FieldOptions::CType::STRING_PIECE},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldOptions::CType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldOptions::CType, std::string_view>({
          {FieldOptions::CType::STRING, "STRING"},
          {FieldOptions::CType::CORD, "CORD"},
          {FieldOptions::CType::STRING_PIECE, "STRING_PIECE"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldOptions::JSType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldOptions::JSType>({
          {"JS_NORMAL", FieldOptions::JSType::JS_NORMAL},
          {"JS_STRING", FieldOptions::JSType::JS_STRING},
          {"JS_NUMBER", FieldOptions::JSType::JS_NUMBER},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldOptions::JSType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldOptions::JSType, std::string_view>({
          {FieldOptions::JSType::JS_NORMAL, "JS_NORMAL"},
          {FieldOptions::JSType::JS_STRING, "JS_STRING"},
          {FieldOptions::JSType::JS_NUMBER, "JS_NUMBER"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldOptions::OptionRetention* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldOptions::OptionRetention>({
          {"RETENTION_UNKNOWN", FieldOptions::OptionRetention::RETENTION_UNKNOWN},
          {"RETENTION_RUNTIME", FieldOptions::OptionRetention::RETENTION_RUNTIME},
          {"RETENTION_SOURCE", FieldOptions::OptionRetention::RETENTION_SOURCE},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldOptions::OptionRetention const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldOptions::OptionRetention, std::string_view>({
          {FieldOptions::OptionRetention::RETENTION_UNKNOWN, "RETENTION_UNKNOWN"},
          {FieldOptions::OptionRetention::RETENTION_RUNTIME, "RETENTION_RUNTIME"},
          {FieldOptions::OptionRetention::RETENTION_SOURCE, "RETENTION_SOURCE"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldOptions::OptionTargetType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldOptions::OptionTargetType>({
          {"TARGET_TYPE_UNKNOWN", FieldOptions::OptionTargetType::TARGET_TYPE_UNKNOWN},
          {"TARGET_TYPE_FILE", FieldOptions::OptionTargetType::TARGET_TYPE_FILE},
          {"TARGET_TYPE_EXTENSION_RANGE",
           FieldOptions::OptionTargetType::TARGET_TYPE_EXTENSION_RANGE},
          {"TARGET_TYPE_MESSAGE", FieldOptions::OptionTargetType::TARGET_TYPE_MESSAGE},
          {"TARGET_TYPE_FIELD", FieldOptions::OptionTargetType::TARGET_TYPE_FIELD},
          {"TARGET_TYPE_ONEOF", FieldOptions::OptionTargetType::TARGET_TYPE_ONEOF},
          {"TARGET_TYPE_ENUM", FieldOptions::OptionTargetType::TARGET_TYPE_ENUM},
          {"TARGET_TYPE_ENUM_ENTRY", FieldOptions::OptionTargetType::TARGET_TYPE_ENUM_ENTRY},
          {"TARGET_TYPE_SERVICE", FieldOptions::OptionTargetType::TARGET_TYPE_SERVICE},
          {"TARGET_TYPE_METHOD", FieldOptions::OptionTargetType::TARGET_TYPE_METHOD},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FieldOptions::OptionTargetType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldOptions::OptionTargetType, std::string_view>({
          {FieldOptions::OptionTargetType::TARGET_TYPE_UNKNOWN, "TARGET_TYPE_UNKNOWN"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_FILE, "TARGET_TYPE_FILE"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_EXTENSION_RANGE,
           "TARGET_TYPE_EXTENSION_RANGE"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_MESSAGE, "TARGET_TYPE_MESSAGE"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_FIELD, "TARGET_TYPE_FIELD"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_ONEOF, "TARGET_TYPE_ONEOF"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_ENUM, "TARGET_TYPE_ENUM"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_ENUM_ENTRY, "TARGET_TYPE_ENUM_ENTRY"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_SERVICE, "TARGET_TYPE_SERVICE"},
          {FieldOptions::OptionTargetType::TARGET_TYPE_METHOD, "TARGET_TYPE_METHOD"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<FieldOptions::EditionDefault> FieldOptions::EditionDefault::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldOptions::EditionDefault proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 3: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition.emplace(value);
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

::tsdb2::io::Cord FieldOptions::EditionDefault::Encode(FieldOptions::EditionDefault const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.edition.has_value()) {
    encoder.EncodeEnumField(3, proto.edition.value());
  }
  if (proto.value.has_value()) {
    encoder.EncodeStringField(2, proto.value.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FieldOptions::EditionDefault* const proto) {
  *proto = FieldOptions::EditionDefault();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition.emplace(value);
    } else if (field_name == "value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->value.emplace(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<FieldOptions::FeatureSupport> FieldOptions::FeatureSupport::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldOptions::FeatureSupport proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition_introduced.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition_deprecated.emplace(value);
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.deprecation_warning.emplace(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition_removed.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FieldOptions::FeatureSupport::Encode(FieldOptions::FeatureSupport const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.edition_introduced.has_value()) {
    encoder.EncodeEnumField(1, proto.edition_introduced.value());
  }
  if (proto.edition_deprecated.has_value()) {
    encoder.EncodeEnumField(2, proto.edition_deprecated.value());
  }
  if (proto.deprecation_warning.has_value()) {
    encoder.EncodeStringField(3, proto.deprecation_warning.value());
  }
  if (proto.edition_removed.has_value()) {
    encoder.EncodeEnumField(4, proto.edition_removed.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FieldOptions::FeatureSupport* const proto) {
  *proto = FieldOptions::FeatureSupport();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "edition_introduced") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition_introduced.emplace(value);
    } else if (field_name == "edition_deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition_deprecated.emplace(value);
    } else if (field_name == "deprecation_warning") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->deprecation_warning.emplace(std::move(value));
    } else if (field_name == "edition_removed") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition_removed.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<FieldOptions> FieldOptions::Decode(::absl::Span<uint8_t const> const data) {
  FieldOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::FieldOptions::CType>(tag.wire_type));
        proto.ctype = value;
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.packed.emplace(value);
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FieldOptions::JSType>(tag.wire_type));
        proto.jstype = value;
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.lazy = value;
      } break;
      case 15: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.unverified_lazy = value;
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 10: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.weak = value;
      } break;
      case 16: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.debug_redact = value;
      } break;
      case 17: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::FieldOptions::OptionRetention>(
                       tag.wire_type));
        proto.retention.emplace(value);
      } break;
      case 19: {
        RETURN_IF_ERROR(
            decoder.DecodeRepeatedEnums<::google::protobuf::FieldOptions::OptionTargetType>(
                tag.wire_type, &proto.targets));
      } break;
      case 20: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::google::protobuf::FieldOptions::EditionDefault::Decode(child_span));
        proto.edition_defaults.emplace_back(std::move(value));
      } break;
      case 21: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 22: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::google::protobuf::FieldOptions::FeatureSupport::Decode(child_span));
        proto.feature_support.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord FieldOptions::Encode(FieldOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeEnumField(1, proto.ctype);
  if (proto.packed.has_value()) {
    encoder.EncodeBoolField(2, proto.packed.value());
  }
  encoder.EncodeEnumField(6, proto.jstype);
  encoder.EncodeBoolField(5, proto.lazy);
  encoder.EncodeBoolField(15, proto.unverified_lazy);
  encoder.EncodeBoolField(3, proto.deprecated);
  encoder.EncodeBoolField(10, proto.weak);
  encoder.EncodeBoolField(16, proto.debug_redact);
  if (proto.retention.has_value()) {
    encoder.EncodeEnumField(17, proto.retention.value());
  }
  for (auto const& value : proto.targets) {
    encoder.EncodeEnumField(19, value);
  }
  for (auto const& value : proto.edition_defaults) {
    encoder.EncodeSubMessageField(20,
                                  ::google::protobuf::FieldOptions::EditionDefault::Encode(value));
  }
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(21,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  if (proto.feature_support.has_value()) {
    encoder.EncodeSubMessageField(22, ::google::protobuf::FieldOptions::FeatureSupport::Encode(
                                          proto.feature_support.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FieldOptions* const proto) {
  *proto = FieldOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "ctype") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::FieldOptions::CType>());
      proto->ctype = value;
    } else if (field_name == "packed") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->packed.emplace(value);
    } else if (field_name == "jstype") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::FieldOptions::JSType>());
      proto->jstype = value;
    } else if (field_name == "lazy") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->lazy = value;
    } else if (field_name == "unverified_lazy") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->unverified_lazy = value;
    } else if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "weak") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->weak = value;
    } else if (field_name == "debug_redact") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->debug_redact = value;
    } else if (field_name == "retention") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::FieldOptions::OptionRetention>());
      proto->retention.emplace(value);
    } else if (field_name == "targets") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::FieldOptions::OptionTargetType>());
      proto->targets.emplace_back(value);
    } else if (field_name == "edition_defaults") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::FieldOptions::EditionDefault>());
      proto->edition_defaults.emplace_back(std::move(message));
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "feature_support") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::FieldOptions::FeatureSupport>());
      proto->feature_support.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<OneofOptions> OneofOptions::Decode(::absl::Span<uint8_t const> const data) {
  OneofOptions proto;
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
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord OneofOptions::Encode(OneofOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(1,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               OneofOptions* const proto) {
  *proto = OneofOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<EnumOptions> EnumOptions::Decode(::absl::Span<uint8_t const> const data) {
  EnumOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.allow_alias.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated_legacy_json_field_conflicts.emplace(value);
      } break;
      case 7: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord EnumOptions::Encode(EnumOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.allow_alias.has_value()) {
    encoder.EncodeBoolField(2, proto.allow_alias.value());
  }
  encoder.EncodeBoolField(3, proto.deprecated);
  if (proto.deprecated_legacy_json_field_conflicts.has_value()) {
    encoder.EncodeBoolField(6, proto.deprecated_legacy_json_field_conflicts.value());
  }
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(7,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               EnumOptions* const proto) {
  *proto = EnumOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "allow_alias") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->allow_alias.emplace(value);
    } else if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "deprecated_legacy_json_field_conflicts") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated_legacy_json_field_conflicts.emplace(value);
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<EnumValueOptions> EnumValueOptions::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumValueOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.debug_redact = value;
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::google::protobuf::FieldOptions::FeatureSupport::Decode(child_span));
        proto.feature_support.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord EnumValueOptions::Encode(EnumValueOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeBoolField(1, proto.deprecated);
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(2,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeBoolField(3, proto.debug_redact);
  if (proto.feature_support.has_value()) {
    encoder.EncodeSubMessageField(
        4, ::google::protobuf::FieldOptions::FeatureSupport::Encode(proto.feature_support.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               EnumValueOptions* const proto) {
  *proto = EnumValueOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "debug_redact") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->debug_redact = value;
    } else if (field_name == "feature_support") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::FieldOptions::FeatureSupport>());
      proto->feature_support.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<ServiceOptions> ServiceOptions::Decode(::absl::Span<uint8_t const> const data) {
  ServiceOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 34: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 33: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord ServiceOptions::Encode(ServiceOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(34,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeBoolField(33, proto.deprecated);
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               ServiceOptions* const proto) {
  *proto = ServiceOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               MethodOptions::IdempotencyLevel* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, MethodOptions::IdempotencyLevel>({
          {"IDEMPOTENCY_UNKNOWN", MethodOptions::IdempotencyLevel::IDEMPOTENCY_UNKNOWN},
          {"NO_SIDE_EFFECTS", MethodOptions::IdempotencyLevel::NO_SIDE_EFFECTS},
          {"IDEMPOTENT", MethodOptions::IdempotencyLevel::IDEMPOTENT},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                MethodOptions::IdempotencyLevel const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<MethodOptions::IdempotencyLevel, std::string_view>({
          {MethodOptions::IdempotencyLevel::IDEMPOTENCY_UNKNOWN, "IDEMPOTENCY_UNKNOWN"},
          {MethodOptions::IdempotencyLevel::NO_SIDE_EFFECTS, "NO_SIDE_EFFECTS"},
          {MethodOptions::IdempotencyLevel::IDEMPOTENT, "IDEMPOTENT"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<MethodOptions> MethodOptions::Decode(::absl::Span<uint8_t const> const data) {
  MethodOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 33: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.deprecated = value;
      } break;
      case 34: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::MethodOptions::IdempotencyLevel>(
                       tag.wire_type));
        proto.idempotency_level = value;
      } break;
      case 35: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.features.emplace(std::move(value));
      } break;
      case 999: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::UninterpretedOption::Decode(child_span));
        proto.uninterpreted_option.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord MethodOptions::Encode(MethodOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeBoolField(33, proto.deprecated);
  encoder.EncodeEnumField(34, proto.idempotency_level);
  if (proto.features.has_value()) {
    encoder.EncodeSubMessageField(35,
                                  ::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeSubMessageField(999, ::google::protobuf::UninterpretedOption::Encode(value));
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               MethodOptions* const proto) {
  *proto = MethodOptions();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "deprecated") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->deprecated = value;
    } else if (field_name == "idempotency_level") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::MethodOptions::IdempotencyLevel>());
      proto->idempotency_level = value;
    } else if (field_name == "features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->features.emplace(std::move(message));
    } else if (field_name == "uninterpreted_option") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::UninterpretedOption>());
      proto->uninterpreted_option.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<UninterpretedOption::NamePart> UninterpretedOption::NamePart::Decode(
    ::absl::Span<uint8_t const> const data) {
  UninterpretedOption::NamePart proto;
  ::tsdb2::common::flat_set<size_t> decoded;
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
        proto.name_part = std::move(value);
        decoded.emplace(1);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeBoolField(tag.wire_type));
        proto.is_extension = value;
        decoded.emplace(2);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  if (!decoded.contains(1)) {
    return absl::InvalidArgumentError("missing required field \"name_part\"");
  }
  if (!decoded.contains(2)) {
    return absl::InvalidArgumentError("missing required field \"is_extension\"");
  }
  return std::move(proto);
}

::tsdb2::io::Cord UninterpretedOption::NamePart::Encode(
    UninterpretedOption::NamePart const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeStringField(1, proto.name_part);
  encoder.EncodeBoolField(2, proto.is_extension);
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               UninterpretedOption::NamePart* const proto) {
  *proto = UninterpretedOption::NamePart();
  ::tsdb2::common::flat_set<size_t> parsed;
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name_part") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name_part = std::move(value);
      parsed.emplace(1);
    } else if (field_name == "is_extension") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseBoolean());
      proto->is_extension = value;
      parsed.emplace(2);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  if (!parsed.contains(1)) {
    return absl::InvalidArgumentError("missing required field \"name_part\"");
  }
  if (!parsed.contains(2)) {
    return absl::InvalidArgumentError("missing required field \"is_extension\"");
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<UninterpretedOption> UninterpretedOption::Decode(
    ::absl::Span<uint8_t const> const data) {
  UninterpretedOption proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 2: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value,
                             ::google::protobuf::UninterpretedOption::NamePart::Decode(child_span));
        proto.name.emplace_back(std::move(value));
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.identifier_value.emplace(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt64Field(tag.wire_type));
        proto.positive_int_value.emplace(value);
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt64Field(tag.wire_type));
        proto.negative_int_value.emplace(value);
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeDoubleField(tag.wire_type));
        proto.double_value.emplace(value);
      } break;
      case 7: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeBytesField(tag.wire_type));
        proto.string_value.emplace(std::move(value));
      } break;
      case 8: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.aggregate_value.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord UninterpretedOption::Encode(UninterpretedOption const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.name) {
    encoder.EncodeSubMessageField(2,
                                  ::google::protobuf::UninterpretedOption::NamePart::Encode(value));
  }
  if (proto.identifier_value.has_value()) {
    encoder.EncodeStringField(3, proto.identifier_value.value());
  }
  if (proto.positive_int_value.has_value()) {
    encoder.EncodeUInt64Field(4, proto.positive_int_value.value());
  }
  if (proto.negative_int_value.has_value()) {
    encoder.EncodeInt64Field(5, proto.negative_int_value.value());
  }
  if (proto.double_value.has_value()) {
    encoder.EncodeDoubleField(6, proto.double_value.value());
  }
  if (proto.string_value.has_value()) {
    encoder.EncodeBytesField(7, proto.string_value.value());
  }
  if (proto.aggregate_value.has_value()) {
    encoder.EncodeStringField(8, proto.aggregate_value.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               UninterpretedOption* const proto) {
  *proto = UninterpretedOption();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::UninterpretedOption::NamePart>());
      proto->name.emplace_back(std::move(message));
    } else if (field_name == "identifier_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->identifier_value.emplace(std::move(value));
    } else if (field_name == "positive_int_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<uint64_t>());
      proto->positive_int_value.emplace(value);
    } else if (field_name == "negative_int_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int64_t>());
      proto->negative_int_value.emplace(value);
    } else if (field_name == "double_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseFloat<double>());
      proto->double_value.emplace(value);
    } else if (field_name == "string_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseBytes());
      proto->string_value.emplace(std::move(value));
    } else if (field_name == "aggregate_value") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->aggregate_value.emplace(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::FieldPresence* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::FieldPresence>({
          {"FIELD_PRESENCE_UNKNOWN", FeatureSet::FieldPresence::FIELD_PRESENCE_UNKNOWN},
          {"EXPLICIT", FeatureSet::FieldPresence::EXPLICIT},
          {"IMPLICIT", FeatureSet::FieldPresence::IMPLICIT},
          {"LEGACY_REQUIRED", FeatureSet::FieldPresence::LEGACY_REQUIRED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::FieldPresence const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::FieldPresence, std::string_view>({
          {FeatureSet::FieldPresence::FIELD_PRESENCE_UNKNOWN, "FIELD_PRESENCE_UNKNOWN"},
          {FeatureSet::FieldPresence::EXPLICIT, "EXPLICIT"},
          {FeatureSet::FieldPresence::IMPLICIT, "IMPLICIT"},
          {FeatureSet::FieldPresence::LEGACY_REQUIRED, "LEGACY_REQUIRED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::EnumType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::EnumType>({
          {"ENUM_TYPE_UNKNOWN", FeatureSet::EnumType::ENUM_TYPE_UNKNOWN},
          {"OPEN", FeatureSet::EnumType::OPEN},
          {"CLOSED", FeatureSet::EnumType::CLOSED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::EnumType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::EnumType, std::string_view>({
          {FeatureSet::EnumType::ENUM_TYPE_UNKNOWN, "ENUM_TYPE_UNKNOWN"},
          {FeatureSet::EnumType::OPEN, "OPEN"},
          {FeatureSet::EnumType::CLOSED, "CLOSED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::RepeatedFieldEncoding* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::RepeatedFieldEncoding>({
          {"REPEATED_FIELD_ENCODING_UNKNOWN",
           FeatureSet::RepeatedFieldEncoding::REPEATED_FIELD_ENCODING_UNKNOWN},
          {"PACKED", FeatureSet::RepeatedFieldEncoding::PACKED},
          {"EXPANDED", FeatureSet::RepeatedFieldEncoding::EXPANDED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::RepeatedFieldEncoding const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::RepeatedFieldEncoding, std::string_view>({
          {FeatureSet::RepeatedFieldEncoding::REPEATED_FIELD_ENCODING_UNKNOWN,
           "REPEATED_FIELD_ENCODING_UNKNOWN"},
          {FeatureSet::RepeatedFieldEncoding::PACKED, "PACKED"},
          {FeatureSet::RepeatedFieldEncoding::EXPANDED, "EXPANDED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::Utf8Validation* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::Utf8Validation>({
          {"UTF8_VALIDATION_UNKNOWN", FeatureSet::Utf8Validation::UTF8_VALIDATION_UNKNOWN},
          {"VERIFY", FeatureSet::Utf8Validation::VERIFY},
          {"NONE", FeatureSet::Utf8Validation::NONE},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::Utf8Validation const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::Utf8Validation, std::string_view>({
          {FeatureSet::Utf8Validation::UTF8_VALIDATION_UNKNOWN, "UTF8_VALIDATION_UNKNOWN"},
          {FeatureSet::Utf8Validation::VERIFY, "VERIFY"},
          {FeatureSet::Utf8Validation::NONE, "NONE"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::MessageEncoding* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::MessageEncoding>({
          {"MESSAGE_ENCODING_UNKNOWN", FeatureSet::MessageEncoding::MESSAGE_ENCODING_UNKNOWN},
          {"LENGTH_PREFIXED", FeatureSet::MessageEncoding::LENGTH_PREFIXED},
          {"DELIMITED", FeatureSet::MessageEncoding::DELIMITED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::MessageEncoding const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::MessageEncoding, std::string_view>({
          {FeatureSet::MessageEncoding::MESSAGE_ENCODING_UNKNOWN, "MESSAGE_ENCODING_UNKNOWN"},
          {FeatureSet::MessageEncoding::LENGTH_PREFIXED, "LENGTH_PREFIXED"},
          {FeatureSet::MessageEncoding::DELIMITED, "DELIMITED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FeatureSet::JsonFormat* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FeatureSet::JsonFormat>({
          {"JSON_FORMAT_UNKNOWN", FeatureSet::JsonFormat::JSON_FORMAT_UNKNOWN},
          {"ALLOW", FeatureSet::JsonFormat::ALLOW},
          {"LEGACY_BEST_EFFORT", FeatureSet::JsonFormat::LEGACY_BEST_EFFORT},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                FeatureSet::JsonFormat const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FeatureSet::JsonFormat, std::string_view>({
          {FeatureSet::JsonFormat::JSON_FORMAT_UNKNOWN, "JSON_FORMAT_UNKNOWN"},
          {FeatureSet::JsonFormat::ALLOW, "ALLOW"},
          {FeatureSet::JsonFormat::LEGACY_BEST_EFFORT, "LEGACY_BEST_EFFORT"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<FeatureSet> FeatureSet::Decode(::absl::Span<uint8_t const> const data) {
  FeatureSet proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FeatureSet::FieldPresence>(tag.wire_type));
        proto.field_presence.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FeatureSet::EnumType>(tag.wire_type));
        proto.enum_type.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::FeatureSet::RepeatedFieldEncoding>(
                       tag.wire_type));
        proto.repeated_field_encoding.emplace(value);
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FeatureSet::Utf8Validation>(tag.wire_type));
        proto.utf8_validation.emplace(value);
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::google::protobuf::FeatureSet::MessageEncoding>(
                       tag.wire_type));
        proto.message_encoding.emplace(value);
      } break;
      case 6: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::FeatureSet::JsonFormat>(tag.wire_type));
        proto.json_format.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.AddRecordToExtensionData(tag));
        break;
    }
  }
  proto.extension_data = ::tsdb2::proto::ExtensionData(std::move(decoder).GetExtensionData());
  return std::move(proto);
}

::tsdb2::io::Cord FeatureSet::Encode(FeatureSet const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.field_presence.has_value()) {
    encoder.EncodeEnumField(1, proto.field_presence.value());
  }
  if (proto.enum_type.has_value()) {
    encoder.EncodeEnumField(2, proto.enum_type.value());
  }
  if (proto.repeated_field_encoding.has_value()) {
    encoder.EncodeEnumField(3, proto.repeated_field_encoding.value());
  }
  if (proto.utf8_validation.has_value()) {
    encoder.EncodeEnumField(4, proto.utf8_validation.value());
  }
  if (proto.message_encoding.has_value()) {
    encoder.EncodeEnumField(5, proto.message_encoding.value());
  }
  if (proto.json_format.has_value()) {
    encoder.EncodeEnumField(6, proto.json_format.value());
  }
  auto cord = std::move(encoder).Finish();
  proto.extension_data.AppendTo(&cord);
  return cord;
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FeatureSet* const proto) {
  *proto = FeatureSet();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "field_presence") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FeatureSet::FieldPresence>());
      proto->field_presence.emplace(value);
    } else if (field_name == "enum_type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::FeatureSet::EnumType>());
      proto->enum_type.emplace(value);
    } else if (field_name == "repeated_field_encoding") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::FeatureSet::RepeatedFieldEncoding>());
      proto->repeated_field_encoding.emplace(value);
    } else if (field_name == "utf8_validation") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FeatureSet::Utf8Validation>());
      proto->utf8_validation.emplace(value);
    } else if (field_name == "message_encoding") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FeatureSet::MessageEncoding>());
      proto->message_encoding.emplace(value);
    } else if (field_name == "json_format") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value,
                             parser->ParseEnum<::google::protobuf::FeatureSet::JsonFormat>());
      proto->json_format.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<FeatureSetDefaults::FeatureSetEditionDefault>
FeatureSetDefaults::FeatureSetEditionDefault::Decode(::absl::Span<uint8_t const> const data) {
  FeatureSetDefaults::FeatureSetEditionDefault proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 3: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.edition.emplace(value);
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.overridable_features.emplace(std::move(value));
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FeatureSet::Decode(child_span));
        proto.fixed_features.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FeatureSetDefaults::FeatureSetEditionDefault::Encode(
    FeatureSetDefaults::FeatureSetEditionDefault const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.edition.has_value()) {
    encoder.EncodeEnumField(3, proto.edition.value());
  }
  if (proto.overridable_features.has_value()) {
    encoder.EncodeSubMessageField(
        4, ::google::protobuf::FeatureSet::Encode(proto.overridable_features.value()));
  }
  if (proto.fixed_features.has_value()) {
    encoder.EncodeSubMessageField(
        5, ::google::protobuf::FeatureSet::Encode(proto.fixed_features.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FeatureSetDefaults::FeatureSetEditionDefault* const proto) {
  *proto = FeatureSetDefaults::FeatureSetEditionDefault();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->edition.emplace(value);
    } else if (field_name == "overridable_features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->overridable_features.emplace(std::move(message));
    } else if (field_name == "fixed_features") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message, parser->ParseSubMessage<::google::protobuf::FeatureSet>());
      proto->fixed_features.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<FeatureSetDefaults> FeatureSetDefaults::Decode(
    ::absl::Span<uint8_t const> const data) {
  FeatureSetDefaults proto;
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
            ::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault::Decode(child_span));
        proto.defaults.emplace_back(std::move(value));
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.minimum_edition.emplace(value);
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::google::protobuf::Edition>(tag.wire_type));
        proto.maximum_edition.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord FeatureSetDefaults::Encode(FeatureSetDefaults const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.defaults) {
    encoder.EncodeSubMessageField(
        1, ::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault::Encode(value));
  }
  if (proto.minimum_edition.has_value()) {
    encoder.EncodeEnumField(4, proto.minimum_edition.value());
  }
  if (proto.maximum_edition.has_value()) {
    encoder.EncodeEnumField(5, proto.maximum_edition.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               FeatureSetDefaults* const proto) {
  *proto = FeatureSetDefaults();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "defaults") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<
                               ::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault>());
      proto->defaults.emplace_back(std::move(message));
    } else if (field_name == "minimum_edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->minimum_edition.emplace(value);
    } else if (field_name == "maximum_edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::google::protobuf::Edition>());
      proto->maximum_edition.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<SourceCodeInfo::Location> SourceCodeInfo::Location::Decode(
    ::absl::Span<uint8_t const> const data) {
  SourceCodeInfo::Location proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        RETURN_IF_ERROR(decoder.DecodeRepeatedInt32s(tag.wire_type, &proto.path));
      } break;
      case 2: {
        RETURN_IF_ERROR(decoder.DecodeRepeatedInt32s(tag.wire_type, &proto.span));
      } break;
      case 3: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.leading_comments.emplace(std::move(value));
      } break;
      case 4: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.trailing_comments.emplace(std::move(value));
      } break;
      case 6: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.leading_detached_comments.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord SourceCodeInfo::Location::Encode(SourceCodeInfo::Location const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodePackedInt32s(1, proto.path);
  encoder.EncodePackedInt32s(2, proto.span);
  if (proto.leading_comments.has_value()) {
    encoder.EncodeStringField(3, proto.leading_comments.value());
  }
  if (proto.trailing_comments.has_value()) {
    encoder.EncodeStringField(4, proto.trailing_comments.value());
  }
  for (auto const& value : proto.leading_detached_comments) {
    encoder.EncodeStringField(6, value);
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               SourceCodeInfo::Location* const proto) {
  *proto = SourceCodeInfo::Location();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "path") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->path.emplace_back(value);
    } else if (field_name == "span") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->span.emplace_back(value);
    } else if (field_name == "leading_comments") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->leading_comments.emplace(std::move(value));
    } else if (field_name == "trailing_comments") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->trailing_comments.emplace(std::move(value));
    } else if (field_name == "leading_detached_comments") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->leading_detached_comments.emplace_back(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<SourceCodeInfo> SourceCodeInfo::Decode(::absl::Span<uint8_t const> const data) {
  SourceCodeInfo proto;
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
                             ::google::protobuf::SourceCodeInfo::Location::Decode(child_span));
        proto.location.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord SourceCodeInfo::Encode(SourceCodeInfo const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.location) {
    encoder.EncodeSubMessageField(1, ::google::protobuf::SourceCodeInfo::Location::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               SourceCodeInfo* const proto) {
  *proto = SourceCodeInfo();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "location") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::SourceCodeInfo::Location>());
      proto->location.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               GeneratedCodeInfo::Annotation::Semantic* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, GeneratedCodeInfo::Annotation::Semantic>(
          {
              {"NONE", GeneratedCodeInfo::Annotation::Semantic::NONE},
              {"SET", GeneratedCodeInfo::Annotation::Semantic::SET},
              {"ALIAS", GeneratedCodeInfo::Annotation::Semantic::ALIAS},
          });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                                GeneratedCodeInfo::Annotation::Semantic const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<GeneratedCodeInfo::Annotation::Semantic, std::string_view>(
          {
              {GeneratedCodeInfo::Annotation::Semantic::NONE, "NONE"},
              {GeneratedCodeInfo::Annotation::Semantic::SET, "SET"},
              {GeneratedCodeInfo::Annotation::Semantic::ALIAS, "ALIAS"},
          });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<GeneratedCodeInfo::Annotation> GeneratedCodeInfo::Annotation::Decode(
    ::absl::Span<uint8_t const> const data) {
  GeneratedCodeInfo::Annotation proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 1: {
        RETURN_IF_ERROR(decoder.DecodeRepeatedInt32s(tag.wire_type, &proto.path));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.source_file.emplace(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.begin.emplace(value);
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.end.emplace(value);
      } break;
      case 5: {
        DEFINE_CONST_OR_RETURN(
            value,
            decoder.DecodeEnumField<::google::protobuf::GeneratedCodeInfo::Annotation::Semantic>(
                tag.wire_type));
        proto.semantic.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord GeneratedCodeInfo::Annotation::Encode(
    GeneratedCodeInfo::Annotation const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodePackedInt32s(1, proto.path);
  if (proto.source_file.has_value()) {
    encoder.EncodeStringField(2, proto.source_file.value());
  }
  if (proto.begin.has_value()) {
    encoder.EncodeInt32Field(3, proto.begin.value());
  }
  if (proto.end.has_value()) {
    encoder.EncodeInt32Field(4, proto.end.value());
  }
  if (proto.semantic.has_value()) {
    encoder.EncodeEnumField(5, proto.semantic.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               GeneratedCodeInfo::Annotation* const proto) {
  *proto = GeneratedCodeInfo::Annotation();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "path") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->path.emplace_back(value);
    } else if (field_name == "source_file") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->source_file.emplace(std::move(value));
    } else if (field_name == "begin") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->begin.emplace(value);
    } else if (field_name == "end") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->end.emplace(value);
    } else if (field_name == "semantic") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(
          value, parser->ParseEnum<::google::protobuf::GeneratedCodeInfo::Annotation::Semantic>());
      proto->semantic.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<GeneratedCodeInfo> GeneratedCodeInfo::Decode(
    ::absl::Span<uint8_t const> const data) {
  GeneratedCodeInfo proto;
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
                             ::google::protobuf::GeneratedCodeInfo::Annotation::Decode(child_span));
        proto.annotation.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord GeneratedCodeInfo::Encode(GeneratedCodeInfo const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.annotation) {
    encoder.EncodeSubMessageField(1,
                                  ::google::protobuf::GeneratedCodeInfo::Annotation::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               GeneratedCodeInfo* const proto) {
  *proto = GeneratedCodeInfo();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "annotation") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message, parser->ParseSubMessage<::google::protobuf::GeneratedCodeInfo::Annotation>());
      proto->annotation.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
