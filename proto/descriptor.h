#ifndef __TSDB2_PROTO_PLUGIN_H__
#define __TSDB2_PROTO_PLUGIN_H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "proto/object.h"

// Manually adapted from google/protobuf/compiler/plugin.proto and google/protobuf/descriptor.proto
// (see https://github.com/protocolbuffers/protobuf).

namespace google {
namespace protobuf {

enum class Edition {
  EDITION_UNKNOWN = 0,

  EDITION_LEGACY = 900,

  EDITION_PROTO2 = 998,
  EDITION_PROTO3 = 999,

  EDITION_2023 = 1000,
  EDITION_2024 = 1001,

  EDITION_1_TEST_ONLY = 1,
  EDITION_2_TEST_ONLY = 2,
  EDITION_99997_TEST_ONLY = 99997,
  EDITION_99998_TEST_ONLY = 99998,
  EDITION_99999_TEST_ONLY = 99999,

  EDITION_MAX = 0x7FFFFFFF,
};

enum class GeneratedCodeInfo_Annotation_Semantic {
  NONE = 0,
  SET = 1,
  ALIAS = 2,
};

char constexpr kGeneratedCodeInfoAnnotationPathField[] = "path";
char constexpr kGeneratedCodeInfoAnnotationSourceFileField[] = "source_file";
char constexpr kGeneratedCodeInfoAnnotationBeginField[] = "begin";
char constexpr kGeneratedCodeInfoAnnotationEndField[] = "end";
char constexpr kGeneratedCodeInfoAnnotationSemanticField[] = "semantic";

using GeneratedCodeInfo_Annotation = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<int32_t>, kGeneratedCodeInfoAnnotationPathField, 1>,
    tsdb2::proto::Field<std::optional<std::string>, kGeneratedCodeInfoAnnotationSourceFileField, 2>,
    tsdb2::proto::Field<std::optional<int32_t>, kGeneratedCodeInfoAnnotationBeginField, 3>,
    tsdb2::proto::Field<std::optional<int32_t>, kGeneratedCodeInfoAnnotationEndField, 4>,
    tsdb2::proto::Field<std::optional<GeneratedCodeInfo_Annotation_Semantic>,
                        kGeneratedCodeInfoAnnotationSemanticField, 5>>;

char constexpr kGeneratedCodeInfoAnnotationField[] = "annotation";

using GeneratedCodeInfo =
    tsdb2::proto::Object<tsdb2::proto::Field<std::vector<GeneratedCodeInfo_Annotation>,
                                             kGeneratedCodeInfoAnnotationField, 1>>;

char constexpr kSourceCodeInfoLocationPathField[] = "path";
char constexpr kSourceCodeInfoLocationSpanField[] = "span";
char constexpr kSourceCodeInfoLocationLeadingCommentsField[] = "leading_comments";
char constexpr kSourceCodeInfoLocationTrailingCommentsField[] = "trailing_comments";
char constexpr kSourceCodeInfoLocationLeadingDetachedCommentsField[] = "leading_detached_comments";

using SourceCodeInfo_Location = tsdb2::proto::Object<
    tsdb2::proto::Field<std::vector<int32_t>, kSourceCodeInfoLocationPathField, 1>,
    tsdb2::proto::Field<std::vector<int32_t>, kSourceCodeInfoLocationSpanField, 2>,
    tsdb2::proto::Field<std::optional<std::string>, kSourceCodeInfoLocationLeadingCommentsField, 3>,
    tsdb2::proto::Field<std::optional<std::string>, kSourceCodeInfoLocationTrailingCommentsField,
                        4>,
    tsdb2::proto::Field<std::vector<std::string>,
                        kSourceCodeInfoLocationLeadingDetachedCommentsField, 6>>;

char constexpr kSourceCodeInfoLocationField[] = "location";

using SourceCodeInfo = tsdb2::proto::Object<
    tsdb2::proto::Field<std::vector<SourceCodeInfo_Location>, kSourceCodeInfoLocationField, 1>>;

char constexpr kEnumValueOptionsDeprecatedField[] = "deprecated";

using EnumValueOptions = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<bool>, kEnumValueOptionsDeprecatedField, 1>>;

char constexpr kEnumValueDescriptorProtoNameField[] = "name";
char constexpr kEnumValueDescriptorProtoNumberField[] = "number";
char constexpr kEnumValueDescriptorProtoOptionsField[] = "options";

using EnumValueDescriptorProto = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kEnumValueDescriptorProtoNameField, 1>,
    tsdb2::proto::Field<std::optional<int32_t>, kEnumValueDescriptorProtoNumberField, 2>,
    tsdb2::proto::Field<std::optional<EnumValueOptions>, kEnumValueDescriptorProtoOptionsField, 3>>;

char constexpr kEnumDescriptorProtoNameField[] = "name";
char constexpr kEnumDescriptorProtoValueField[] = "value";

using EnumDescriptorProto = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kEnumDescriptorProtoNameField, 1>,
    tsdb2::proto::Field<std::vector<EnumValueDescriptorProto>, kEnumDescriptorProtoValueField, 2>>;

enum class FieldDescriptorProto_Type {
  TYPE_DOUBLE = 1,
  TYPE_FLOAT = 2,

  // Not ZigZag encoded.  Negative numbers take 10 bytes.  Use TYPE_SINT64 if
  // negative values are likely.
  TYPE_INT64 = 3,
  TYPE_UINT64 = 4,

  // Not ZigZag encoded.  Negative numbers take 10 bytes.  Use TYPE_SINT32 if
  // negative values are likely.
  TYPE_INT32 = 5,
  TYPE_FIXED64 = 6,
  TYPE_FIXED32 = 7,
  TYPE_BOOL = 8,
  TYPE_STRING = 9,

  // Tag-delimited aggregate.
  TYPE_GROUP = 10,
  TYPE_MESSAGE = 11,  // Length-delimited aggregate.

  // New in version 2.
  TYPE_BYTES = 12,
  TYPE_UINT32 = 13,
  TYPE_ENUM = 14,
  TYPE_SFIXED32 = 15,
  TYPE_SFIXED64 = 16,
  TYPE_SINT32 = 17,  // Uses ZigZag encoding.
  TYPE_SINT64 = 18,  // Uses ZigZag encoding.
};

enum class FieldDescriptorProto_Label {
  LABEL_OPTIONAL = 1,
  LABEL_REPEATED = 3,
  LABEL_REQUIRED = 2,
};

char constexpr kFieldDescriptorProtoNameField[] = "name";
char constexpr kFieldDescriptorProtoNumberField[] = "number";
char constexpr kFieldDescriptorProtoLabelField[] = "label";
char constexpr kFieldDescriptorProtoTypeField[] = "type";
char constexpr kFieldDescriptorProtoTypeNameField[] = "type_name";
char constexpr kFieldDescriptorProtoExtendeeField[] = "extendee";
char constexpr kFieldDescriptorProtoDefaultValueField[] = "default_value";
char constexpr kFieldDescriptorProtoOneOfIndexField[] = "oneof_index";
char constexpr kFieldDescriptorProtoJsonNameField[] = "json_name";
char constexpr kFieldDescriptorProtoOptionsField[] = "options";
char constexpr kFieldDescriptorProtoProto3OptionalField[] = "proto3_optional";

using FieldDescriptorProto = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kFieldDescriptorProtoNameField, 1>,
    tsdb2::proto::Field<std::optional<int32_t>, kFieldDescriptorProtoNumberField, 3>,
    tsdb2::proto::Field<std::optional<FieldDescriptorProto_Label>, kFieldDescriptorProtoLabelField,
                        4>,
    tsdb2::proto::Field<std::optional<FieldDescriptorProto_Type>, kFieldDescriptorProtoTypeField,
                        5>,
    tsdb2::proto::Field<std::optional<std::string>, kFieldDescriptorProtoTypeNameField, 6>,
    tsdb2::proto::Field<std::optional<std::string>, kFieldDescriptorProtoExtendeeField, 2>,
    tsdb2::proto::Field<std::optional<std::string>, kFieldDescriptorProtoDefaultValueField, 7>,
    tsdb2::proto::Field<std::optional<int32_t>, kFieldDescriptorProtoOneOfIndexField, 9>,
    tsdb2::proto::Field<std::optional<std::string>, kFieldDescriptorProtoJsonNameField, 10>,
    tsdb2::proto::Field<std::optional<bool>, kFieldDescriptorProtoProto3OptionalField, 17>>;

char constexpr kDescriptorProtoNameField[] = "name";
char constexpr kDescriptorProtoFieldField[] = "field";
char constexpr kDescriptorProtoExtensionField[] = "extension";

using DescriptorProto = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kDescriptorProtoNameField, 1>,
    tsdb2::proto::Field<std::vector<FieldDescriptorProto>, kDescriptorProtoFieldField, 2>,
    tsdb2::proto::Field<std::vector<FieldDescriptorProto>, kDescriptorProtoExtensionField, 6>>;

char constexpr kFileDescriptorProtoNameField[] = "name";
char constexpr kFileDescriptorProtoPackageField[] = "package";
char constexpr kFileDescriptorProtoDependencyField[] = "dependency";
char constexpr kFileDescriptorProtoPublicDependencyField[] = "public_dependency";
char constexpr kFileDescriptorProtoMessageTypeField[] = "message_type";
char constexpr kFileDescriptorProtoEnumTypeField[] = "enum_type";

using FileDescriptorProto = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kFileDescriptorProtoNameField, 1>,
    tsdb2::proto::Field<std::optional<std::string>, kFileDescriptorProtoPackageField, 2>,
    tsdb2::proto::Field<std::vector<std::string>, kFileDescriptorProtoDependencyField, 3>,
    tsdb2::proto::Field<std::vector<int32_t>, kFileDescriptorProtoPublicDependencyField, 10>,
    tsdb2::proto::Field<std::vector<DescriptorProto>, kFileDescriptorProtoMessageTypeField, 4>,
    tsdb2::proto::Field<std::vector<EnumDescriptorProto>, kFileDescriptorProtoEnumTypeField, 5>>;

char constexpr kFileDescriptorSetFileField[] = "file";

using FileDescriptorSet = tsdb2::proto::Object<
    tsdb2::proto::Field<std::vector<FileDescriptorProto>, kFileDescriptorSetFileField, 1>>;

namespace compiler {

char constexpr kVersionMajorField[] = "major";
char constexpr kVersionMinorField[] = "minor";
char constexpr kVersionPatchField[] = "patch";
char constexpr kVersionSuffixField[] = "suffix";

using Version =
    tsdb2::proto::Object<tsdb2::proto::Field<std::optional<int32_t>, kVersionMajorField, 1>,
                         tsdb2::proto::Field<std::optional<int32_t>, kVersionMinorField, 2>,
                         tsdb2::proto::Field<std::optional<int32_t>, kVersionPatchField, 3>,
                         tsdb2::proto::Field<std::optional<std::string>, kVersionSuffixField, 4>>;

char constexpr kCodeGeneratorRequestFileToGenerateField[] = "file_to_generate";
char constexpr kCodeGeneratorRequestParameterField[] = "parameter";
char constexpr kCodeGeneratorRequestProtoFileField[] = "proto_file";
char constexpr kCodeGeneratorRequestSourceFileDescriptorsField[] = "source_file_descriptors";
char constexpr kCodeGeneratorRequestCompilerVersionField[] = "compiler_version";

using CodeGeneratorRequest = tsdb2::proto::Object<
    tsdb2::proto::Field<std::vector<std::string>, kCodeGeneratorRequestFileToGenerateField, 1>,
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorRequestParameterField, 2>,
    tsdb2::proto::Field<std::vector<FileDescriptorProto>, kCodeGeneratorRequestProtoFileField, 15>,
    tsdb2::proto::Field<std::vector<FileDescriptorProto>,
                        kCodeGeneratorRequestSourceFileDescriptorsField, 17>,
    tsdb2::proto::Field<std::optional<Version>, kCodeGeneratorRequestCompilerVersionField, 3>>;

enum class CodeGeneratorResponse_Feature {
  FEATURE_NONE = 0,
  FEATURE_PROTO3_OPTIONAL = 1,
  FEATURE_SUPPORTS_EDITIONS = 2,
};

char constexpr kCodeGeneratorResponseFileNameField[] = "name";
char constexpr kCodeGeneratorResponseFileInsertionPointField[] = "insertion_point";
char constexpr kCodeGeneratorResponseFileContentField[] = "content";

using CodeGeneratorResponse_File = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorResponseFileNameField, 1>,
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorResponseFileInsertionPointField,
                        2>,
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorResponseFileContentField, 15>>;

char constexpr kCodeGeneratorResponseErrorField[] = "error";
char constexpr kCodeGeneratorResponseSupportedFeaturesField[] = "supported_features";
char constexpr kCodeGeneratorResponseMinimumEditionField[] = "minimum_edition";
char constexpr kCodeGeneratorResponseMaximumEditionField[] = "maximum_edition";
char constexpr kCodeGeneratorResponseFileField[] = "file";

using CodeGeneratorResponse = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorResponseErrorField, 1>,
    tsdb2::proto::Field<std::optional<uint64_t>, kCodeGeneratorResponseSupportedFeaturesField, 2>,
    tsdb2::proto::Field<std::optional<int32_t>, kCodeGeneratorResponseMinimumEditionField, 3>,
    tsdb2::proto::Field<std::optional<int32_t>, kCodeGeneratorResponseMaximumEditionField, 4>,
    tsdb2::proto::Field<std::vector<CodeGeneratorResponse_File>, kCodeGeneratorResponseFileField,
                        15>>;

// TODO

}  // namespace compiler

}  // namespace protobuf
}  // namespace google

#endif  // __TSDB2_PROTO_PLUGIN_H__
