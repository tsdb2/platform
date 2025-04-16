#include "proto/plugin.pb.sync.h"

#include "proto/descriptor.pb.sync.h"
#include "proto/runtime.h"

namespace google::protobuf::compiler {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<Version> Version::Decode(::absl::Span<uint8_t const> const data) {
  Version proto;
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
        proto.major.emplace(value);
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.minor.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.patch.emplace(value);
      } break;
      case 4: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.suffix.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord Version::Encode(Version const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.major.has_value()) {
    encoder.EncodeInt32Field(1, proto.major.value());
  }
  if (proto.minor.has_value()) {
    encoder.EncodeInt32Field(2, proto.minor.value());
  }
  if (proto.patch.has_value()) {
    encoder.EncodeInt32Field(3, proto.patch.value());
  }
  if (proto.suffix.has_value()) {
    encoder.EncodeStringField(4, proto.suffix.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser, Version* const proto) {
  *proto = Version();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "major") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->major.emplace(value);
    } else if (field_name == "minor") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->minor.emplace(value);
    } else if (field_name == "patch") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->patch.emplace(value);
    } else if (field_name == "suffix") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->suffix.emplace(std::move(value));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<CodeGeneratorRequest> CodeGeneratorRequest::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorRequest proto;
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
        proto.file_to_generate.emplace_back(std::move(value));
      } break;
      case 2: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.parameter.emplace(std::move(value));
      } break;
      case 15: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FileDescriptorProto::Decode(child_span));
        proto.proto_file.emplace_back(std::move(value));
      } break;
      case 17: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::FileDescriptorProto::Decode(child_span));
        proto.source_file_descriptors.emplace_back(std::move(value));
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::compiler::Version::Decode(child_span));
        proto.compiler_version.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord CodeGeneratorRequest::Encode(CodeGeneratorRequest const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.file_to_generate) {
    encoder.EncodeStringField(1, value);
  }
  if (proto.parameter.has_value()) {
    encoder.EncodeStringField(2, proto.parameter.value());
  }
  for (auto const& value : proto.proto_file) {
    encoder.EncodeSubMessageField(15, ::google::protobuf::FileDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.source_file_descriptors) {
    encoder.EncodeSubMessageField(17, ::google::protobuf::FileDescriptorProto::Encode(value));
  }
  if (proto.compiler_version.has_value()) {
    encoder.EncodeSubMessageField(
        3, ::google::protobuf::compiler::Version::Encode(proto.compiler_version.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               CodeGeneratorRequest* const proto) {
  *proto = CodeGeneratorRequest();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "file_to_generate") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->file_to_generate.emplace_back(std::move(value));
    } else if (field_name == "parameter") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->parameter.emplace(std::move(value));
    } else if (field_name == "proto_file") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FileDescriptorProto>());
      proto->proto_file.emplace_back(std::move(message));
    } else if (field_name == "source_file_descriptors") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::FileDescriptorProto>());
      proto->source_file_descriptors.emplace_back(std::move(message));
    } else if (field_name == "compiler_version") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::compiler::Version>());
      proto->compiler_version.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               CodeGeneratorResponse::Feature* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, CodeGeneratorResponse::Feature>({
          {"FEATURE_NONE", CodeGeneratorResponse::Feature::FEATURE_NONE},
          {"FEATURE_PROTO3_OPTIONAL", CodeGeneratorResponse::Feature::FEATURE_PROTO3_OPTIONAL},
          {"FEATURE_SUPPORTS_EDITIONS", CodeGeneratorResponse::Feature::FEATURE_SUPPORTS_EDITIONS},
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
                                CodeGeneratorResponse::Feature const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<CodeGeneratorResponse::Feature, std::string_view>({
          {CodeGeneratorResponse::Feature::FEATURE_NONE, "FEATURE_NONE"},
          {CodeGeneratorResponse::Feature::FEATURE_PROTO3_OPTIONAL, "FEATURE_PROTO3_OPTIONAL"},
          {CodeGeneratorResponse::Feature::FEATURE_SUPPORTS_EDITIONS, "FEATURE_SUPPORTS_EDITIONS"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    return std::string(it->second);
  } else {
    return ::absl::StrCat(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<CodeGeneratorResponse::File> CodeGeneratorResponse::File::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorResponse::File proto;
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
        proto.insertion_point.emplace(std::move(value));
      } break;
      case 15: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeStringField(tag.wire_type));
        proto.content.emplace(std::move(value));
      } break;
      case 16: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(value, ::google::protobuf::GeneratedCodeInfo::Decode(child_span));
        proto.generated_code_info.emplace(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord CodeGeneratorResponse::File::Encode(CodeGeneratorResponse::File const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.name.has_value()) {
    encoder.EncodeStringField(1, proto.name.value());
  }
  if (proto.insertion_point.has_value()) {
    encoder.EncodeStringField(2, proto.insertion_point.value());
  }
  if (proto.content.has_value()) {
    encoder.EncodeStringField(15, proto.content.value());
  }
  if (proto.generated_code_info.has_value()) {
    encoder.EncodeSubMessageField(
        16, ::google::protobuf::GeneratedCodeInfo::Encode(proto.generated_code_info.value()));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               CodeGeneratorResponse::File* const proto) {
  *proto = CodeGeneratorResponse::File();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "name") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->name.emplace(std::move(value));
    } else if (field_name == "insertion_point") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->insertion_point.emplace(std::move(value));
    } else if (field_name == "content") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->content.emplace(std::move(value));
    } else if (field_name == "generated_code_info") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(message,
                           parser->ParseSubMessage<::google::protobuf::GeneratedCodeInfo>());
      proto->generated_code_info.emplace(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

::absl::StatusOr<CodeGeneratorResponse> CodeGeneratorResponse::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorResponse proto;
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
        proto.error.emplace(std::move(value));
      } break;
      case 2: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt64Field(tag.wire_type));
        proto.supported_features.emplace(value);
      } break;
      case 3: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.minimum_edition.emplace(value);
      } break;
      case 4: {
        DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32Field(tag.wire_type));
        proto.maximum_edition.emplace(value);
      } break;
      case 15: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
        DEFINE_VAR_OR_RETURN(
            value, ::google::protobuf::compiler::CodeGeneratorResponse::File::Decode(child_span));
        proto.file.emplace_back(std::move(value));
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord CodeGeneratorResponse::Encode(CodeGeneratorResponse const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.error.has_value()) {
    encoder.EncodeStringField(1, proto.error.value());
  }
  if (proto.supported_features.has_value()) {
    encoder.EncodeUInt64Field(2, proto.supported_features.value());
  }
  if (proto.minimum_edition.has_value()) {
    encoder.EncodeInt32Field(3, proto.minimum_edition.value());
  }
  if (proto.maximum_edition.has_value()) {
    encoder.EncodeInt32Field(4, proto.maximum_edition.value());
  }
  for (auto const& value : proto.file) {
    encoder.EncodeSubMessageField(
        15, ::google::protobuf::compiler::CodeGeneratorResponse::File::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               CodeGeneratorResponse* const proto) {
  *proto = CodeGeneratorResponse();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "error") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_VAR_OR_RETURN(value, parser->ParseString());
      proto->error.emplace(std::move(value));
    } else if (field_name == "supported_features") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<uint64_t>());
      proto->supported_features.emplace(value);
    } else if (field_name == "minimum_edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->minimum_edition.emplace(value);
    } else if (field_name == "maximum_edition") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseInteger<int32_t>());
      proto->maximum_edition.emplace(value);
    } else if (field_name == "file") {
      parser->ConsumePrefix(":");
      DEFINE_VAR_OR_RETURN(
          message,
          parser->ParseSubMessage<::google::protobuf::compiler::CodeGeneratorResponse::File>());
      proto->file.emplace_back(std::move(message));
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf::compiler
