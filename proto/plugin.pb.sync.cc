#include "proto/plugin.pb.sync.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/flat_set.h"
#include "common/utilities.h"
#include "io/cord.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/proto.h"
#include "proto/wire_format.h"

namespace google::protobuf::compiler {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<Version> Version::Decode(::absl::Span<uint8_t const> const data) {
  Version proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<CodeGeneratorRequest> CodeGeneratorRequest::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorRequest proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 15, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FileDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.source_file_descriptors) {
    encoder.EncodeTag({.field_number = 17, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FileDescriptorProto::Encode(value));
  }
  if (proto.compiler_version.has_value()) {
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::compiler::Version::Encode(proto.compiler_version.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<CodeGeneratorResponse::File> CodeGeneratorResponse::File::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorResponse::File proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 16, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::GeneratedCodeInfo::Encode(proto.generated_code_info.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<CodeGeneratorResponse> CodeGeneratorResponse::Decode(
    ::absl::Span<uint8_t const> const data) {
  CodeGeneratorResponse proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 15, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::compiler::CodeGeneratorResponse::File::Encode(value));
  }
  return std::move(encoder).Finish();
}

::tsdb2::proto::EnumDescriptor<CodeGeneratorResponse::Feature, 3> const
    CodeGeneratorResponse::Feature_ENUM_DESCRIPTOR{{
        {"FEATURE_NONE", 0},
        {"FEATURE_PROTO3_OPTIONAL", 1},
        {"FEATURE_SUPPORTS_EDITIONS", 2},
    }};

::tsdb2::proto::MessageDescriptor<Version, 4> const Version::MESSAGE_DESCRIPTOR{{
    {"major", &Version::major},
    {"minor", &Version::minor},
    {"patch", &Version::patch},
    {"suffix", &Version::suffix},
}};

::tsdb2::proto::MessageDescriptor<CodeGeneratorRequest,
                                  5> const CodeGeneratorRequest::MESSAGE_DESCRIPTOR{{
    {"compiler_version", ::tsdb2::proto::OptionalSubMessageField<CodeGeneratorRequest>(
                             &CodeGeneratorRequest::compiler_version,
                             ::google::protobuf::compiler::Version::MESSAGE_DESCRIPTOR)},
    {"file_to_generate", &CodeGeneratorRequest::file_to_generate},
    {"parameter", &CodeGeneratorRequest::parameter},
    {"proto_file", ::tsdb2::proto::RepeatedSubMessageField<CodeGeneratorRequest>(
                       &CodeGeneratorRequest::proto_file,
                       ::google::protobuf::FileDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"source_file_descriptors", ::tsdb2::proto::RepeatedSubMessageField<CodeGeneratorRequest>(
                                    &CodeGeneratorRequest::source_file_descriptors,
                                    ::google::protobuf::FileDescriptorProto::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<CodeGeneratorResponse,
                                  5> const CodeGeneratorResponse::MESSAGE_DESCRIPTOR{{
    {"error", &CodeGeneratorResponse::error},
    {"file", ::tsdb2::proto::RepeatedSubMessageField<CodeGeneratorResponse>(
                 &CodeGeneratorResponse::file,
                 ::google::protobuf::compiler::CodeGeneratorResponse::File::MESSAGE_DESCRIPTOR)},
    {"maximum_edition", &CodeGeneratorResponse::maximum_edition},
    {"minimum_edition", &CodeGeneratorResponse::minimum_edition},
    {"supported_features", &CodeGeneratorResponse::supported_features},
}};

::tsdb2::proto::MessageDescriptor<CodeGeneratorResponse::File,
                                  4> const CodeGeneratorResponse::File::MESSAGE_DESCRIPTOR{{
    {"content", &CodeGeneratorResponse::File::content},
    {"generated_code_info", ::tsdb2::proto::OptionalSubMessageField<CodeGeneratorResponse::File>(
                                &CodeGeneratorResponse::File::generated_code_info,
                                ::google::protobuf::GeneratedCodeInfo::MESSAGE_DESCRIPTOR)},
    {"insertion_point", &CodeGeneratorResponse::File::insertion_point},
    {"name", &CodeGeneratorResponse::File::name},
}};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf::compiler
