#include "proto/descriptor.pb.sync.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/flat_set.h"
#include "common/utilities.h"
#include "io/cord.h"
#include "proto/proto.h"
#include "proto/wire_format.h"

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::StatusOr<FileDescriptorSet> FileDescriptorSet::Decode(
    ::absl::Span<uint8_t const> const data) {
  FileDescriptorSet proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 1, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FileDescriptorProto::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<FileDescriptorProto> FileDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  FileDescriptorProto proto;
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
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::DescriptorProto::Encode(value));
  }
  for (auto const& value : proto.enum_type) {
    encoder.EncodeTag({.field_number = 5, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::EnumDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.service) {
    encoder.EncodeTag({.field_number = 6, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::ServiceDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension) {
    encoder.EncodeTag({.field_number = 7, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeTag({.field_number = 8, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FileOptions::Encode(proto.options.value()));
  }
  if (proto.source_code_info.has_value()) {
    encoder.EncodeTag({.field_number = 9, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::SourceCodeInfo::Encode(proto.source_code_info.value()));
  }
  if (proto.syntax.has_value()) {
    encoder.EncodeStringField(12, proto.syntax.value());
  }
  if (proto.edition.has_value()) {
    encoder.EncodeEnumField(14, proto.edition.value());
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<DescriptorProto::ExtensionRange> DescriptorProto::ExtensionRange::Decode(
    ::absl::Span<uint8_t const> const data) {
  DescriptorProto::ExtensionRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::ExtensionRangeOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<DescriptorProto::ReservedRange> DescriptorProto::ReservedRange::Decode(
    ::absl::Span<uint8_t const> const data) {
  DescriptorProto::ReservedRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<DescriptorProto> DescriptorProto::Decode(::absl::Span<uint8_t const> const data) {
  DescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension) {
    encoder.EncodeTag({.field_number = 6, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FieldDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.nested_type) {
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::DescriptorProto::Encode(value));
  }
  for (auto const& value : proto.enum_type) {
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::EnumDescriptorProto::Encode(value));
  }
  for (auto const& value : proto.extension_range) {
    encoder.EncodeTag({.field_number = 5, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::DescriptorProto::ExtensionRange::Encode(value));
  }
  for (auto const& value : proto.oneof_decl) {
    encoder.EncodeTag({.field_number = 8, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::OneofDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeTag({.field_number = 7, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::MessageOptions::Encode(proto.options.value()));
  }
  for (auto const& value : proto.reserved_range) {
    encoder.EncodeTag({.field_number = 9, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::DescriptorProto::ReservedRange::Encode(value));
  }
  for (auto const& value : proto.reserved_name) {
    encoder.EncodeStringField(10, value);
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<ExtensionRangeOptions::Declaration> ExtensionRangeOptions::Declaration::Decode(
    ::absl::Span<uint8_t const> const data) {
  ExtensionRangeOptions::Declaration proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<ExtensionRangeOptions> ExtensionRangeOptions::Decode(
    ::absl::Span<uint8_t const> const data) {
  ExtensionRangeOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord ExtensionRangeOptions::Encode(ExtensionRangeOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  for (auto const& value : proto.declaration) {
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::ExtensionRangeOptions::Declaration::Encode(value));
  }
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 50, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeEnumField(3, proto.verification);
  return std::move(encoder).Finish();
}

::absl::StatusOr<FieldDescriptorProto> FieldDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldDescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 8, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FieldOptions::Encode(proto.options.value()));
  }
  if (proto.proto3_optional.has_value()) {
    encoder.EncodeBoolField(17, proto.proto3_optional.value());
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<OneofDescriptorProto> OneofDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  OneofDescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::OneofOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<EnumDescriptorProto::EnumReservedRange>
EnumDescriptorProto::EnumReservedRange::Decode(::absl::Span<uint8_t const> const data) {
  EnumDescriptorProto::EnumReservedRange proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<EnumDescriptorProto> EnumDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumDescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::EnumValueDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::EnumOptions::Encode(proto.options.value()));
  }
  for (auto const& value : proto.reserved_range) {
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::EnumDescriptorProto::EnumReservedRange::Encode(value));
  }
  for (auto const& value : proto.reserved_name) {
    encoder.EncodeStringField(5, value);
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<EnumValueDescriptorProto> EnumValueDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumValueDescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::EnumValueOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<ServiceDescriptorProto> ServiceDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  ServiceDescriptorProto proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::MethodDescriptorProto::Encode(value));
  }
  if (proto.options.has_value()) {
    encoder.EncodeTag({.field_number = 3, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::ServiceOptions::Encode(proto.options.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<MethodDescriptorProto> MethodDescriptorProto::Decode(
    ::absl::Span<uint8_t const> const data) {
  MethodDescriptorProto proto;
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
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::MethodOptions::Encode(proto.options.value()));
  }
  encoder.EncodeBoolField(5, proto.client_streaming);
  encoder.EncodeBoolField(6, proto.server_streaming);
  return std::move(encoder).Finish();
}

::absl::StatusOr<FileOptions> FileOptions::Decode(::absl::Span<uint8_t const> const data) {
  FileOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
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
    encoder.EncodeTag({.field_number = 50, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<MessageOptions> MessageOptions::Decode(::absl::Span<uint8_t const> const data) {
  MessageOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
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
    encoder.EncodeTag({.field_number = 12, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<FieldOptions::EditionDefault> FieldOptions::EditionDefault::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldOptions::EditionDefault proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<FieldOptions::FeatureSupport> FieldOptions::FeatureSupport::Decode(
    ::absl::Span<uint8_t const> const data) {
  FieldOptions::FeatureSupport proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<FieldOptions> FieldOptions::Decode(::absl::Span<uint8_t const> const data) {
  FieldOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
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
    encoder.EncodeTag({.field_number = 20, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FieldOptions::EditionDefault::Encode(value));
  }
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 21, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  if (proto.feature_support.has_value()) {
    encoder.EncodeTag({.field_number = 22, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::FieldOptions::FeatureSupport::Encode(proto.feature_support.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<OneofOptions> OneofOptions::Decode(::absl::Span<uint8_t const> const data) {
  OneofOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord OneofOptions::Encode(OneofOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 1, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<EnumOptions> EnumOptions::Decode(::absl::Span<uint8_t const> const data) {
  EnumOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
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
    encoder.EncodeTag({.field_number = 7, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<EnumValueOptions> EnumValueOptions::Decode(
    ::absl::Span<uint8_t const> const data) {
  EnumValueOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord EnumValueOptions::Encode(EnumValueOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeBoolField(1, proto.deprecated);
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeBoolField(3, proto.debug_redact);
  if (proto.feature_support.has_value()) {
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::FieldOptions::FeatureSupport::Encode(proto.feature_support.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<ServiceOptions> ServiceOptions::Decode(::absl::Span<uint8_t const> const data) {
  ServiceOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord ServiceOptions::Encode(ServiceOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 34, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  encoder.EncodeBoolField(33, proto.deprecated);
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<MethodOptions> MethodOptions::Decode(::absl::Span<uint8_t const> const data) {
  MethodOptions proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord MethodOptions::Encode(MethodOptions const& proto) {
  ::tsdb2::proto::Encoder encoder;
  encoder.EncodeBoolField(33, proto.deprecated);
  encoder.EncodeEnumField(34, proto.idempotency_level);
  if (proto.features.has_value()) {
    encoder.EncodeTag({.field_number = 35, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.features.value()));
  }
  for (auto const& value : proto.uninterpreted_option) {
    encoder.EncodeTag({.field_number = 999, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<UninterpretedOption::NamePart> UninterpretedOption::NamePart::Decode(
    ::absl::Span<uint8_t const> const data) {
  UninterpretedOption::NamePart proto;
  ::tsdb2::common::flat_set<size_t> decoded;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<UninterpretedOption> UninterpretedOption::Decode(
    ::absl::Span<uint8_t const> const data) {
  UninterpretedOption proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 2, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::UninterpretedOption::NamePart::Encode(value));
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

::absl::StatusOr<FeatureSet> FeatureSet::Decode(::absl::Span<uint8_t const> const data) {
  FeatureSet proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
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
  return std::move(encoder).Finish();
}

::absl::StatusOr<FeatureSetDefaults::FeatureSetEditionDefault>
FeatureSetDefaults::FeatureSetEditionDefault::Decode(::absl::Span<uint8_t const> const data) {
  FeatureSetDefaults::FeatureSetEditionDefault proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 4, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::FeatureSet::Encode(proto.overridable_features.value()));
  }
  if (proto.fixed_features.has_value()) {
    encoder.EncodeTag({.field_number = 5, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::FeatureSet::Encode(proto.fixed_features.value()));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<FeatureSetDefaults> FeatureSetDefaults::Decode(
    ::absl::Span<uint8_t const> const data) {
  FeatureSetDefaults proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 1, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(
        ::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault::Encode(value));
  }
  if (proto.minimum_edition.has_value()) {
    encoder.EncodeEnumField(4, proto.minimum_edition.value());
  }
  if (proto.maximum_edition.has_value()) {
    encoder.EncodeEnumField(5, proto.maximum_edition.value());
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<SourceCodeInfo::Location> SourceCodeInfo::Location::Decode(
    ::absl::Span<uint8_t const> const data) {
  SourceCodeInfo::Location proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<SourceCodeInfo> SourceCodeInfo::Decode(::absl::Span<uint8_t const> const data) {
  SourceCodeInfo proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 1, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::SourceCodeInfo::Location::Encode(value));
  }
  return std::move(encoder).Finish();
}

::absl::StatusOr<GeneratedCodeInfo::Annotation> GeneratedCodeInfo::Annotation::Decode(
    ::absl::Span<uint8_t const> const data) {
  GeneratedCodeInfo::Annotation proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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

::absl::StatusOr<GeneratedCodeInfo> GeneratedCodeInfo::Decode(
    ::absl::Span<uint8_t const> const data) {
  GeneratedCodeInfo proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
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
    encoder.EncodeTag({.field_number = 1, .wire_type = ::tsdb2::proto::WireType::kLength});
    encoder.EncodeSubMessage(::google::protobuf::GeneratedCodeInfo::Annotation::Encode(value));
  }
  return std::move(encoder).Finish();
}

::tsdb2::proto::EnumDescriptor<FeatureSet::EnumType, 3> const FeatureSet::EnumType_ENUM_DESCRIPTOR{{
    {"ENUM_TYPE_UNKNOWN", 0},
    {"OPEN", 1},
    {"CLOSED", 2},
}};

::tsdb2::proto::EnumDescriptor<FeatureSet::FieldPresence, 4> const
    FeatureSet::FieldPresence_ENUM_DESCRIPTOR{{
        {"FIELD_PRESENCE_UNKNOWN", 0},
        {"EXPLICIT", 1},
        {"IMPLICIT", 2},
        {"LEGACY_REQUIRED", 3},
    }};

::tsdb2::proto::EnumDescriptor<FeatureSet::JsonFormat, 3> const
    FeatureSet::JsonFormat_ENUM_DESCRIPTOR{{
        {"JSON_FORMAT_UNKNOWN", 0},
        {"ALLOW", 1},
        {"LEGACY_BEST_EFFORT", 2},
    }};

::tsdb2::proto::EnumDescriptor<FeatureSet::MessageEncoding, 3> const
    FeatureSet::MessageEncoding_ENUM_DESCRIPTOR{{
        {"MESSAGE_ENCODING_UNKNOWN", 0},
        {"LENGTH_PREFIXED", 1},
        {"DELIMITED", 2},
    }};

::tsdb2::proto::EnumDescriptor<FeatureSet::RepeatedFieldEncoding, 3> const
    FeatureSet::RepeatedFieldEncoding_ENUM_DESCRIPTOR{{
        {"REPEATED_FIELD_ENCODING_UNKNOWN", 0},
        {"PACKED", 1},
        {"EXPANDED", 2},
    }};

::tsdb2::proto::EnumDescriptor<FeatureSet::Utf8Validation, 3> const
    FeatureSet::Utf8Validation_ENUM_DESCRIPTOR{{
        {"UTF8_VALIDATION_UNKNOWN", 0},
        {"VERIFY", 2},
        {"NONE", 3},
    }};

::tsdb2::proto::EnumDescriptor<ExtensionRangeOptions::VerificationState, 2> const
    ExtensionRangeOptions::VerificationState_ENUM_DESCRIPTOR{{
        {"DECLARATION", 0},
        {"UNVERIFIED", 1},
    }};

::tsdb2::proto::EnumDescriptor<Edition, 12> const Edition_ENUM_DESCRIPTOR{{
    {"EDITION_UNKNOWN", 0},
    {"EDITION_LEGACY", 900},
    {"EDITION_PROTO2", 998},
    {"EDITION_PROTO3", 999},
    {"EDITION_2023", 1000},
    {"EDITION_2024", 1001},
    {"EDITION_1_TEST_ONLY", 1},
    {"EDITION_2_TEST_ONLY", 2},
    {"EDITION_99997_TEST_ONLY", 99997},
    {"EDITION_99998_TEST_ONLY", 99998},
    {"EDITION_99999_TEST_ONLY", 99999},
    {"EDITION_MAX", 2147483647},
}};

::tsdb2::proto::EnumDescriptor<FieldDescriptorProto::Label, 3> const
    FieldDescriptorProto::Label_ENUM_DESCRIPTOR{{
        {"LABEL_OPTIONAL", 1},
        {"LABEL_REPEATED", 3},
        {"LABEL_REQUIRED", 2},
    }};

::tsdb2::proto::EnumDescriptor<FieldOptions::CType, 3> const FieldOptions::CType_ENUM_DESCRIPTOR{{
    {"STRING", 0},
    {"CORD", 1},
    {"STRING_PIECE", 2},
}};

::tsdb2::proto::EnumDescriptor<FieldOptions::JSType, 3> const FieldOptions::JSType_ENUM_DESCRIPTOR{{
    {"JS_NORMAL", 0},
    {"JS_STRING", 1},
    {"JS_NUMBER", 2},
}};

::tsdb2::proto::EnumDescriptor<FieldOptions::OptionRetention, 3> const
    FieldOptions::OptionRetention_ENUM_DESCRIPTOR{{
        {"RETENTION_UNKNOWN", 0},
        {"RETENTION_RUNTIME", 1},
        {"RETENTION_SOURCE", 2},
    }};

::tsdb2::proto::EnumDescriptor<FieldDescriptorProto::Type, 18> const
    FieldDescriptorProto::Type_ENUM_DESCRIPTOR{{
        {"TYPE_DOUBLE", 1},
        {"TYPE_FLOAT", 2},
        {"TYPE_INT64", 3},
        {"TYPE_UINT64", 4},
        {"TYPE_INT32", 5},
        {"TYPE_FIXED64", 6},
        {"TYPE_FIXED32", 7},
        {"TYPE_BOOL", 8},
        {"TYPE_STRING", 9},
        {"TYPE_GROUP", 10},
        {"TYPE_MESSAGE", 11},
        {"TYPE_BYTES", 12},
        {"TYPE_UINT32", 13},
        {"TYPE_ENUM", 14},
        {"TYPE_SFIXED32", 15},
        {"TYPE_SFIXED64", 16},
        {"TYPE_SINT32", 17},
        {"TYPE_SINT64", 18},
    }};

::tsdb2::proto::EnumDescriptor<FieldOptions::OptionTargetType, 10> const
    FieldOptions::OptionTargetType_ENUM_DESCRIPTOR{{
        {"TARGET_TYPE_UNKNOWN", 0},
        {"TARGET_TYPE_FILE", 1},
        {"TARGET_TYPE_EXTENSION_RANGE", 2},
        {"TARGET_TYPE_MESSAGE", 3},
        {"TARGET_TYPE_FIELD", 4},
        {"TARGET_TYPE_ONEOF", 5},
        {"TARGET_TYPE_ENUM", 6},
        {"TARGET_TYPE_ENUM_ENTRY", 7},
        {"TARGET_TYPE_SERVICE", 8},
        {"TARGET_TYPE_METHOD", 9},
    }};

::tsdb2::proto::EnumDescriptor<FileOptions::OptimizeMode, 3> const
    FileOptions::OptimizeMode_ENUM_DESCRIPTOR{{
        {"SPEED", 1},
        {"CODE_SIZE", 2},
        {"LITE_RUNTIME", 3},
    }};

::tsdb2::proto::EnumDescriptor<GeneratedCodeInfo::Annotation::Semantic, 3> const
    GeneratedCodeInfo::Annotation::Semantic_ENUM_DESCRIPTOR{{
        {"NONE", 0},
        {"SET", 1},
        {"ALIAS", 2},
    }};

::tsdb2::proto::EnumDescriptor<MethodOptions::IdempotencyLevel, 3> const
    MethodOptions::IdempotencyLevel_ENUM_DESCRIPTOR{{
        {"IDEMPOTENCY_UNKNOWN", 0},
        {"NO_SIDE_EFFECTS", 1},
        {"IDEMPOTENT", 2},
    }};

::tsdb2::proto::MessageDescriptor<FeatureSet, 6> const FeatureSet::MESSAGE_DESCRIPTOR{{
    {"enum_type",
     ::tsdb2::proto::OptionalEnumField<FeatureSet>(
         &FeatureSet::enum_type, ::google::protobuf::FeatureSet::EnumType_ENUM_DESCRIPTOR)},
    {"field_presence", ::tsdb2::proto::OptionalEnumField<FeatureSet>(
                           &FeatureSet::field_presence,
                           ::google::protobuf::FeatureSet::FieldPresence_ENUM_DESCRIPTOR)},
    {"json_format",
     ::tsdb2::proto::OptionalEnumField<FeatureSet>(
         &FeatureSet::json_format, ::google::protobuf::FeatureSet::JsonFormat_ENUM_DESCRIPTOR)},
    {"message_encoding", ::tsdb2::proto::OptionalEnumField<FeatureSet>(
                             &FeatureSet::message_encoding,
                             ::google::protobuf::FeatureSet::MessageEncoding_ENUM_DESCRIPTOR)},
    {"repeated_field_encoding",
     ::tsdb2::proto::OptionalEnumField<FeatureSet>(
         &FeatureSet::repeated_field_encoding,
         ::google::protobuf::FeatureSet::RepeatedFieldEncoding_ENUM_DESCRIPTOR)},
    {"utf8_validation", ::tsdb2::proto::OptionalEnumField<FeatureSet>(
                            &FeatureSet::utf8_validation,
                            ::google::protobuf::FeatureSet::Utf8Validation_ENUM_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<MessageOptions, 7> const MessageOptions::MESSAGE_DESCRIPTOR{{
    {"deprecated", &MessageOptions::deprecated},
    {"deprecated_legacy_json_field_conflicts",
     &MessageOptions::deprecated_legacy_json_field_conflicts},
    {"features",
     ::tsdb2::proto::OptionalSubMessageField<MessageOptions>(
         &MessageOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"map_entry", &MessageOptions::map_entry},
    {"message_set_wire_format", &MessageOptions::message_set_wire_format},
    {"no_standard_descriptor_accessor", &MessageOptions::no_standard_descriptor_accessor},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<MessageOptions>(
                                 &MessageOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<DescriptorProto, 10> const DescriptorProto::MESSAGE_DESCRIPTOR{{
    {"enum_type",
     ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
         &DescriptorProto::enum_type, ::google::protobuf::EnumDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"extension", ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
                      &DescriptorProto::extension,
                      ::google::protobuf::FieldDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"extension_range",
     ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
         &DescriptorProto::extension_range,
         ::google::protobuf::DescriptorProto::ExtensionRange::MESSAGE_DESCRIPTOR)},
    {"field",
     ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
         &DescriptorProto::field, ::google::protobuf::FieldDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"name", &DescriptorProto::name},
    {"nested_type",
     ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
         &DescriptorProto::nested_type, ::google::protobuf::DescriptorProto::MESSAGE_DESCRIPTOR)},
    {"oneof_decl", ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
                       &DescriptorProto::oneof_decl,
                       ::google::protobuf::OneofDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"options",
     ::tsdb2::proto::OptionalSubMessageField<DescriptorProto>(
         &DescriptorProto::options, ::google::protobuf::MessageOptions::MESSAGE_DESCRIPTOR)},
    {"reserved_name", &DescriptorProto::reserved_name},
    {"reserved_range", ::tsdb2::proto::RepeatedSubMessageField<DescriptorProto>(
                           &DescriptorProto::reserved_range,
                           ::google::protobuf::DescriptorProto::ReservedRange::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<ExtensionRangeOptions, 4> const
    ExtensionRangeOptions::MESSAGE_DESCRIPTOR{{
        {"declaration",
         ::tsdb2::proto::RepeatedSubMessageField<ExtensionRangeOptions>(
             &ExtensionRangeOptions::declaration,
             ::google::protobuf::ExtensionRangeOptions::Declaration::MESSAGE_DESCRIPTOR)},
        {"features",
         ::tsdb2::proto::OptionalSubMessageField<ExtensionRangeOptions>(
             &ExtensionRangeOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
        {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<ExtensionRangeOptions>(
                                     &ExtensionRangeOptions::uninterpreted_option,
                                     ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
        {"verification",
         ::tsdb2::proto::RawEnumField<ExtensionRangeOptions>(
             &ExtensionRangeOptions::verification,
             ::google::protobuf::ExtensionRangeOptions::VerificationState_ENUM_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<DescriptorProto::ExtensionRange, 3> const
    DescriptorProto::ExtensionRange::MESSAGE_DESCRIPTOR{{
        {"end", &DescriptorProto::ExtensionRange::end},
        {"options", ::tsdb2::proto::OptionalSubMessageField<DescriptorProto::ExtensionRange>(
                        &DescriptorProto::ExtensionRange::options,
                        ::google::protobuf::ExtensionRangeOptions::MESSAGE_DESCRIPTOR)},
        {"start", &DescriptorProto::ExtensionRange::start},
    }};

::tsdb2::proto::MessageDescriptor<DescriptorProto::ReservedRange, 2> const
    DescriptorProto::ReservedRange::MESSAGE_DESCRIPTOR{{
        {"end", &DescriptorProto::ReservedRange::end},
        {"start", &DescriptorProto::ReservedRange::start},
    }};

::tsdb2::proto::MessageDescriptor<EnumOptions, 5> const EnumOptions::MESSAGE_DESCRIPTOR{{
    {"allow_alias", &EnumOptions::allow_alias},
    {"deprecated", &EnumOptions::deprecated},
    {"deprecated_legacy_json_field_conflicts",
     &EnumOptions::deprecated_legacy_json_field_conflicts},
    {"features", ::tsdb2::proto::OptionalSubMessageField<EnumOptions>(
                     &EnumOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<EnumOptions>(
                                 &EnumOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<EnumDescriptorProto, 5> const
    EnumDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"name", &EnumDescriptorProto::name},
        {"options",
         ::tsdb2::proto::OptionalSubMessageField<EnumDescriptorProto>(
             &EnumDescriptorProto::options, ::google::protobuf::EnumOptions::MESSAGE_DESCRIPTOR)},
        {"reserved_name", &EnumDescriptorProto::reserved_name},
        {"reserved_range",
         ::tsdb2::proto::RepeatedSubMessageField<EnumDescriptorProto>(
             &EnumDescriptorProto::reserved_range,
             ::google::protobuf::EnumDescriptorProto::EnumReservedRange::MESSAGE_DESCRIPTOR)},
        {"value", ::tsdb2::proto::RepeatedSubMessageField<EnumDescriptorProto>(
                      &EnumDescriptorProto::value,
                      ::google::protobuf::EnumValueDescriptorProto::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<EnumDescriptorProto::EnumReservedRange, 2> const
    EnumDescriptorProto::EnumReservedRange::MESSAGE_DESCRIPTOR{{
        {"end", &EnumDescriptorProto::EnumReservedRange::end},
        {"start", &EnumDescriptorProto::EnumReservedRange::start},
    }};

::tsdb2::proto::MessageDescriptor<FieldOptions::FeatureSupport, 4> const
    FieldOptions::FeatureSupport::MESSAGE_DESCRIPTOR{{
        {"deprecation_warning", &FieldOptions::FeatureSupport::deprecation_warning},
        {"edition_deprecated", ::tsdb2::proto::OptionalEnumField<FieldOptions::FeatureSupport>(
                                   &FieldOptions::FeatureSupport::edition_deprecated,
                                   ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
        {"edition_introduced", ::tsdb2::proto::OptionalEnumField<FieldOptions::FeatureSupport>(
                                   &FieldOptions::FeatureSupport::edition_introduced,
                                   ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
        {"edition_removed", ::tsdb2::proto::OptionalEnumField<FieldOptions::FeatureSupport>(
                                &FieldOptions::FeatureSupport::edition_removed,
                                ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<EnumValueOptions, 5> const EnumValueOptions::MESSAGE_DESCRIPTOR{{
    {"debug_redact", &EnumValueOptions::debug_redact},
    {"deprecated", &EnumValueOptions::deprecated},
    {"feature_support", ::tsdb2::proto::OptionalSubMessageField<EnumValueOptions>(
                            &EnumValueOptions::feature_support,
                            ::google::protobuf::FieldOptions::FeatureSupport::MESSAGE_DESCRIPTOR)},
    {"features",
     ::tsdb2::proto::OptionalSubMessageField<EnumValueOptions>(
         &EnumValueOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<EnumValueOptions>(
                                 &EnumValueOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<EnumValueDescriptorProto, 3> const
    EnumValueDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"name", &EnumValueDescriptorProto::name},
        {"number", &EnumValueDescriptorProto::number},
        {"options", ::tsdb2::proto::OptionalSubMessageField<EnumValueDescriptorProto>(
                        &EnumValueDescriptorProto::options,
                        ::google::protobuf::EnumValueOptions::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<ExtensionRangeOptions::Declaration, 5> const
    ExtensionRangeOptions::Declaration::MESSAGE_DESCRIPTOR{{
        {"full_name", &ExtensionRangeOptions::Declaration::full_name},
        {"number", &ExtensionRangeOptions::Declaration::number},
        {"repeated", &ExtensionRangeOptions::Declaration::repeated},
        {"reserved", &ExtensionRangeOptions::Declaration::reserved},
        {"type", &ExtensionRangeOptions::Declaration::type},
    }};

::tsdb2::proto::MessageDescriptor<FeatureSetDefaults, 3> const
    FeatureSetDefaults::MESSAGE_DESCRIPTOR{{
        {"defaults",
         ::tsdb2::proto::RepeatedSubMessageField<FeatureSetDefaults>(
             &FeatureSetDefaults::defaults,
             ::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault::MESSAGE_DESCRIPTOR)},
        {"maximum_edition",
         ::tsdb2::proto::OptionalEnumField<FeatureSetDefaults>(
             &FeatureSetDefaults::maximum_edition, ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
        {"minimum_edition",
         ::tsdb2::proto::OptionalEnumField<FeatureSetDefaults>(
             &FeatureSetDefaults::minimum_edition, ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<FeatureSetDefaults::FeatureSetEditionDefault, 3> const
    FeatureSetDefaults::FeatureSetEditionDefault::MESSAGE_DESCRIPTOR{{
        {"edition", ::tsdb2::proto::OptionalEnumField<FeatureSetDefaults::FeatureSetEditionDefault>(
                        &FeatureSetDefaults::FeatureSetEditionDefault::edition,
                        ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
        {"fixed_features",
         ::tsdb2::proto::OptionalSubMessageField<FeatureSetDefaults::FeatureSetEditionDefault>(
             &FeatureSetDefaults::FeatureSetEditionDefault::fixed_features,
             ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
        {"overridable_features",
         ::tsdb2::proto::OptionalSubMessageField<FeatureSetDefaults::FeatureSetEditionDefault>(
             &FeatureSetDefaults::FeatureSetEditionDefault::overridable_features,
             ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<FieldOptions, 14> const FieldOptions::MESSAGE_DESCRIPTOR{{
    {"ctype", ::tsdb2::proto::RawEnumField<FieldOptions>(
                  &FieldOptions::ctype, ::google::protobuf::FieldOptions::CType_ENUM_DESCRIPTOR)},
    {"debug_redact", &FieldOptions::debug_redact},
    {"deprecated", &FieldOptions::deprecated},
    {"edition_defaults", ::tsdb2::proto::RepeatedSubMessageField<FieldOptions>(
                             &FieldOptions::edition_defaults,
                             ::google::protobuf::FieldOptions::EditionDefault::MESSAGE_DESCRIPTOR)},
    {"feature_support", ::tsdb2::proto::OptionalSubMessageField<FieldOptions>(
                            &FieldOptions::feature_support,
                            ::google::protobuf::FieldOptions::FeatureSupport::MESSAGE_DESCRIPTOR)},
    {"features", ::tsdb2::proto::OptionalSubMessageField<FieldOptions>(
                     &FieldOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"jstype",
     ::tsdb2::proto::RawEnumField<FieldOptions>(
         &FieldOptions::jstype, ::google::protobuf::FieldOptions::JSType_ENUM_DESCRIPTOR)},
    {"lazy", &FieldOptions::lazy},
    {"packed", &FieldOptions::packed},
    {"retention", ::tsdb2::proto::OptionalEnumField<FieldOptions>(
                      &FieldOptions::retention,
                      ::google::protobuf::FieldOptions::OptionRetention_ENUM_DESCRIPTOR)},
    {"targets", ::tsdb2::proto::RepeatedEnumField<FieldOptions>(
                    &FieldOptions::targets,
                    ::google::protobuf::FieldOptions::OptionTargetType_ENUM_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<FieldOptions>(
                                 &FieldOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
    {"unverified_lazy", &FieldOptions::unverified_lazy},
    {"weak", &FieldOptions::weak},
}};

::tsdb2::proto::MessageDescriptor<FieldDescriptorProto, 11> const
    FieldDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"default_value", &FieldDescriptorProto::default_value},
        {"extendee", &FieldDescriptorProto::extendee},
        {"json_name", &FieldDescriptorProto::json_name},
        {"label", ::tsdb2::proto::OptionalEnumField<FieldDescriptorProto>(
                      &FieldDescriptorProto::label,
                      ::google::protobuf::FieldDescriptorProto::Label_ENUM_DESCRIPTOR)},
        {"name", &FieldDescriptorProto::name},
        {"number", &FieldDescriptorProto::number},
        {"oneof_index", &FieldDescriptorProto::oneof_index},
        {"options",
         ::tsdb2::proto::OptionalSubMessageField<FieldDescriptorProto>(
             &FieldDescriptorProto::options, ::google::protobuf::FieldOptions::MESSAGE_DESCRIPTOR)},
        {"proto3_optional", &FieldDescriptorProto::proto3_optional},
        {"type", ::tsdb2::proto::OptionalEnumField<FieldDescriptorProto>(
                     &FieldDescriptorProto::type,
                     ::google::protobuf::FieldDescriptorProto::Type_ENUM_DESCRIPTOR)},
        {"type_name", &FieldDescriptorProto::type_name},
    }};

::tsdb2::proto::MessageDescriptor<FieldOptions::EditionDefault, 2> const
    FieldOptions::EditionDefault::MESSAGE_DESCRIPTOR{{
        {"edition",
         ::tsdb2::proto::OptionalEnumField<FieldOptions::EditionDefault>(
             &FieldOptions::EditionDefault::edition, ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
        {"value", &FieldOptions::EditionDefault::value},
    }};

::tsdb2::proto::MessageDescriptor<FileOptions, 21> const FileOptions::MESSAGE_DESCRIPTOR{{
    {"cc_enable_arenas", &FileOptions::cc_enable_arenas},
    {"cc_generic_services", &FileOptions::cc_generic_services},
    {"csharp_namespace", &FileOptions::csharp_namespace},
    {"deprecated", &FileOptions::deprecated},
    {"features", ::tsdb2::proto::OptionalSubMessageField<FileOptions>(
                     &FileOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"go_package", &FileOptions::go_package},
    {"java_generate_equals_and_hash", &FileOptions::java_generate_equals_and_hash},
    {"java_generic_services", &FileOptions::java_generic_services},
    {"java_multiple_files", &FileOptions::java_multiple_files},
    {"java_outer_classname", &FileOptions::java_outer_classname},
    {"java_package", &FileOptions::java_package},
    {"java_string_check_utf8", &FileOptions::java_string_check_utf8},
    {"objc_class_prefix", &FileOptions::objc_class_prefix},
    {"optimize_for", ::tsdb2::proto::RawEnumField<FileOptions>(
                         &FileOptions::optimize_for,
                         ::google::protobuf::FileOptions::OptimizeMode_ENUM_DESCRIPTOR)},
    {"php_class_prefix", &FileOptions::php_class_prefix},
    {"php_metadata_namespace", &FileOptions::php_metadata_namespace},
    {"php_namespace", &FileOptions::php_namespace},
    {"py_generic_services", &FileOptions::py_generic_services},
    {"ruby_package", &FileOptions::ruby_package},
    {"swift_prefix", &FileOptions::swift_prefix},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<FileOptions>(
                                 &FileOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<SourceCodeInfo, 1> const SourceCodeInfo::MESSAGE_DESCRIPTOR{{
    {"location", ::tsdb2::proto::RepeatedSubMessageField<SourceCodeInfo>(
                     &SourceCodeInfo::location,
                     ::google::protobuf::SourceCodeInfo::Location::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<FileDescriptorProto,
                                  13> const FileDescriptorProto::MESSAGE_DESCRIPTOR{{
    {"dependency", &FileDescriptorProto::dependency},
    {"edition", ::tsdb2::proto::OptionalEnumField<FileDescriptorProto>(
                    &FileDescriptorProto::edition, ::google::protobuf::Edition_ENUM_DESCRIPTOR)},
    {"enum_type", ::tsdb2::proto::RepeatedSubMessageField<FileDescriptorProto>(
                      &FileDescriptorProto::enum_type,
                      ::google::protobuf::EnumDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"extension", ::tsdb2::proto::RepeatedSubMessageField<FileDescriptorProto>(
                      &FileDescriptorProto::extension,
                      ::google::protobuf::FieldDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"message_type", ::tsdb2::proto::RepeatedSubMessageField<FileDescriptorProto>(
                         &FileDescriptorProto::message_type,
                         ::google::protobuf::DescriptorProto::MESSAGE_DESCRIPTOR)},
    {"name", &FileDescriptorProto::name},
    {"options",
     ::tsdb2::proto::OptionalSubMessageField<FileDescriptorProto>(
         &FileDescriptorProto::options, ::google::protobuf::FileOptions::MESSAGE_DESCRIPTOR)},
    {"package", &FileDescriptorProto::package},
    {"public_dependency", &FileDescriptorProto::public_dependency},
    {"service", ::tsdb2::proto::RepeatedSubMessageField<FileDescriptorProto>(
                    &FileDescriptorProto::service,
                    ::google::protobuf::ServiceDescriptorProto::MESSAGE_DESCRIPTOR)},
    {"source_code_info", ::tsdb2::proto::OptionalSubMessageField<FileDescriptorProto>(
                             &FileDescriptorProto::source_code_info,
                             ::google::protobuf::SourceCodeInfo::MESSAGE_DESCRIPTOR)},
    {"syntax", &FileDescriptorProto::syntax},
    {"weak_dependency", &FileDescriptorProto::weak_dependency},
}};

::tsdb2::proto::MessageDescriptor<FileDescriptorSet, 1> const FileDescriptorSet::MESSAGE_DESCRIPTOR{
    {
        {"file", ::tsdb2::proto::RepeatedSubMessageField<FileDescriptorSet>(
                     &FileDescriptorSet::file,
                     ::google::protobuf::FileDescriptorProto::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<GeneratedCodeInfo, 1> const GeneratedCodeInfo::MESSAGE_DESCRIPTOR{
    {
        {"annotation", ::tsdb2::proto::RepeatedSubMessageField<GeneratedCodeInfo>(
                           &GeneratedCodeInfo::annotation,
                           ::google::protobuf::GeneratedCodeInfo::Annotation::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<GeneratedCodeInfo::Annotation,
                                  5> const GeneratedCodeInfo::Annotation::MESSAGE_DESCRIPTOR{{
    {"begin", &GeneratedCodeInfo::Annotation::begin},
    {"end", &GeneratedCodeInfo::Annotation::end},
    {"path", &GeneratedCodeInfo::Annotation::path},
    {"semantic", ::tsdb2::proto::OptionalEnumField<GeneratedCodeInfo::Annotation>(
                     &GeneratedCodeInfo::Annotation::semantic,
                     ::google::protobuf::GeneratedCodeInfo::Annotation::Semantic_ENUM_DESCRIPTOR)},
    {"source_file", &GeneratedCodeInfo::Annotation::source_file},
}};

::tsdb2::proto::MessageDescriptor<MethodOptions, 4> const MethodOptions::MESSAGE_DESCRIPTOR{{
    {"deprecated", &MethodOptions::deprecated},
    {"features", ::tsdb2::proto::OptionalSubMessageField<MethodOptions>(
                     &MethodOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"idempotency_level", ::tsdb2::proto::RawEnumField<MethodOptions>(
                              &MethodOptions::idempotency_level,
                              ::google::protobuf::MethodOptions::IdempotencyLevel_ENUM_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<MethodOptions>(
                                 &MethodOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<MethodDescriptorProto, 6> const
    MethodDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"client_streaming", &MethodDescriptorProto::client_streaming},
        {"input_type", &MethodDescriptorProto::input_type},
        {"name", &MethodDescriptorProto::name},
        {"options", ::tsdb2::proto::OptionalSubMessageField<MethodDescriptorProto>(
                        &MethodDescriptorProto::options,
                        ::google::protobuf::MethodOptions::MESSAGE_DESCRIPTOR)},
        {"output_type", &MethodDescriptorProto::output_type},
        {"server_streaming", &MethodDescriptorProto::server_streaming},
    }};

::tsdb2::proto::MessageDescriptor<OneofOptions, 2> const OneofOptions::MESSAGE_DESCRIPTOR{{
    {"features", ::tsdb2::proto::OptionalSubMessageField<OneofOptions>(
                     &OneofOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<OneofOptions>(
                                 &OneofOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<OneofDescriptorProto, 2> const
    OneofDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"name", &OneofDescriptorProto::name},
        {"options",
         ::tsdb2::proto::OptionalSubMessageField<OneofDescriptorProto>(
             &OneofDescriptorProto::options, ::google::protobuf::OneofOptions::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<ServiceOptions, 3> const ServiceOptions::MESSAGE_DESCRIPTOR{{
    {"deprecated", &ServiceOptions::deprecated},
    {"features",
     ::tsdb2::proto::OptionalSubMessageField<ServiceOptions>(
         &ServiceOptions::features, ::google::protobuf::FeatureSet::MESSAGE_DESCRIPTOR)},
    {"uninterpreted_option", ::tsdb2::proto::RepeatedSubMessageField<ServiceOptions>(
                                 &ServiceOptions::uninterpreted_option,
                                 ::google::protobuf::UninterpretedOption::MESSAGE_DESCRIPTOR)},
}};

::tsdb2::proto::MessageDescriptor<ServiceDescriptorProto, 3> const
    ServiceDescriptorProto::MESSAGE_DESCRIPTOR{{
        {"method", ::tsdb2::proto::RepeatedSubMessageField<ServiceDescriptorProto>(
                       &ServiceDescriptorProto::method,
                       ::google::protobuf::MethodDescriptorProto::MESSAGE_DESCRIPTOR)},
        {"name", &ServiceDescriptorProto::name},
        {"options", ::tsdb2::proto::OptionalSubMessageField<ServiceDescriptorProto>(
                        &ServiceDescriptorProto::options,
                        ::google::protobuf::ServiceOptions::MESSAGE_DESCRIPTOR)},
    }};

::tsdb2::proto::MessageDescriptor<SourceCodeInfo::Location, 5> const
    SourceCodeInfo::Location::MESSAGE_DESCRIPTOR{{
        {"leading_comments", &SourceCodeInfo::Location::leading_comments},
        {"leading_detached_comments", &SourceCodeInfo::Location::leading_detached_comments},
        {"path", &SourceCodeInfo::Location::path},
        {"span", &SourceCodeInfo::Location::span},
        {"trailing_comments", &SourceCodeInfo::Location::trailing_comments},
    }};

::tsdb2::proto::MessageDescriptor<UninterpretedOption, 7> const
    UninterpretedOption::MESSAGE_DESCRIPTOR{{
        {"aggregate_value", &UninterpretedOption::aggregate_value},
        {"double_value", &UninterpretedOption::double_value},
        {"identifier_value", &UninterpretedOption::identifier_value},
        {"name", ::tsdb2::proto::RepeatedSubMessageField<UninterpretedOption>(
                     &UninterpretedOption::name,
                     ::google::protobuf::UninterpretedOption::NamePart::MESSAGE_DESCRIPTOR)},
        {"negative_int_value", &UninterpretedOption::negative_int_value},
        {"positive_int_value", &UninterpretedOption::positive_int_value},
        {"string_value", &UninterpretedOption::string_value},
    }};

::tsdb2::proto::MessageDescriptor<UninterpretedOption::NamePart, 2> const
    UninterpretedOption::NamePart::MESSAGE_DESCRIPTOR{{
        {"is_extension", &UninterpretedOption::NamePart::is_extension},
        {"name_part", &UninterpretedOption::NamePart::name_part},
    }};

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
