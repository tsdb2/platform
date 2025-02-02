#ifndef __TSDB2_PROTO_PLUGIN_H__
#define __TSDB2_PROTO_PLUGIN_H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "proto/object.h"

// Manually translated from google/protobuf/compiler/plugin.proto and
// google/protobuf/descriptor.proto (see https://github.com/protocolbuffers/protobuf).

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

// TODO

namespace compiler {

char constexpr kVersionMajorField[] = "major";
char constexpr kVersionMinorField[] = "minor";
char constexpr kVersionPatchField[] = "patch";
char constexpr kVersionSuffixField[] = "suffix";

using Version = tsdb2::proto::Object<tsdb2::proto::Field<int32_t, kVersionMajorField, 1>,
                                     tsdb2::proto::Field<int32_t, kVersionMinorField, 2>,
                                     tsdb2::proto::Field<int32_t, kVersionPatchField, 3>,
                                     tsdb2::proto::Field<std::string, kVersionSuffixField, 4>>;

char constexpr kCodeGeneratorRequestFileToGenerateField[] = "file_to_generate";
char constexpr kCodeGeneratorRequestParameterField[] = "parameter";
char constexpr kCodeGeneratorRequestCompilerVersionField[] = "compiler_version";

using CodeGeneratorRequest = tsdb2::proto::Object<
    tsdb2::proto::Field<std::vector<std::string>, kCodeGeneratorRequestFileToGenerateField, 1>,
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorRequestParameterField, 2>,
    tsdb2::proto::Field<std::optional<Version>, kCodeGeneratorRequestCompilerVersionField, 3>>;

enum class CodeGeneratorResponse_Feature {
  FEATURE_NONE = 0,
  FEATURE_PROTO3_OPTIONAL = 1,
  FEATURE_SUPPORTS_EDITIONS = 2,
};

char constexpr kCodeGeneratorResponseFileNameField[] = "name";

using CodeGeneratorResponse_File = tsdb2::proto::Object<
    tsdb2::proto::Field<std::optional<std::string>, kCodeGeneratorResponseFileNameField, 1>>;

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
