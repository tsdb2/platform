#include "proto/plugin.h"

#include <cstdint>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "proto/wire.h"

namespace google {
namespace protobuf {

// TODO

namespace compiler {

absl::StatusOr<CodeGeneratorRequest> CodeGeneratorRequest::Decode(
    absl::Span<uint8_t const> const buffer) {
  tsdb2::proto::Decoder decoder{buffer};
  CodeGeneratorRequest proto;
  while (!decoder.at_end()) {
    DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
    switch (tag.field_number) {
      case kFileToGenerateTagNumber: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeString());
        proto.file_to_generate.emplace_back(std::move(value));
      } break;
      case kParameterTagNumber: {
        DEFINE_VAR_OR_RETURN(value, decoder.DecodeString());
        proto.parameter = std::move(value);
      } break;
      case kProtoFileTagNumber: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan());
        DEFINE_VAR_OR_RETURN(value, FileDescriptorProto::Decode(child_span));
        proto.proto_file.emplace_back(std::move(value));
      } break;
      case kSourceFileDescriptorsTagNumber: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan());
        DEFINE_VAR_OR_RETURN(value, FileDescriptorProto::Decode(child_span));
        proto.source_file_descriptors.emplace_back(std::move(value));
      } break;
      case kCompilerVersionTagNumber: {
        DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan());
        DEFINE_VAR_OR_RETURN(value, Version::Decode(child_span));
        proto.compiler_version = std::move(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

}  // namespace compiler

}  // namespace protobuf
}  // namespace google
