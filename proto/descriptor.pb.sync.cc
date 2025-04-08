#include "proto/descriptor.pb.sync.h"

#include "proto/runtime.h"

namespace google::protobuf {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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
  return std::move(encoder).Finish();
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

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace google::protobuf
