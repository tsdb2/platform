#ifndef __TSDB2_PROTO_GENERATOR_H__
#define __TSDB2_PROTO_GENERATOR_H__

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "proto/plugin.h"

namespace tsdb2 {
namespace proto {
namespace generator {

absl::StatusOr<std::vector<uint8_t>> ReadFile(FILE* fp);
absl::Status WriteFile(FILE* fp, absl::Span<uint8_t const> data);

absl::StatusOr<std::string> GenerateHeaderFileContent(
    google::protobuf::FileDescriptorProto const& file_descriptor);

absl::StatusOr<std::string> GenerateSourceFileContent(
    google::protobuf::FileDescriptorProto const& file_descriptor);

absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateHeaderFile(
    google::protobuf::FileDescriptorProto const& file_descriptor);

absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateSourceFile(
    google::protobuf::FileDescriptorProto const& file_descriptor);

}  // namespace generator
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
