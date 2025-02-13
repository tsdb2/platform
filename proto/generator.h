#ifndef __TSDB2_PROTO_GENERATOR_H__
#define __TSDB2_PROTO_GENERATOR_H__

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "proto/dependencies.h"
#include "proto/descriptor.h"

namespace tsdb2 {
namespace proto {

namespace generator {

absl::StatusOr<std::vector<uint8_t>> ReadFile(FILE* fp);
absl::Status WriteFile(FILE* fp, absl::Span<uint8_t const> data);

std::string MakeHeaderFileName(std::string_view proto_file_name);
std::string MakeSourceFileName(std::string_view proto_file_name);

}  // namespace generator

class Generator {
 public:
  static absl::StatusOr<Generator> Create(
      google::protobuf::FileDescriptorProto const& file_descriptor);

  absl::StatusOr<std::string> GenerateHeaderFileContent();
  absl::StatusOr<std::string> GenerateSourceFileContent();

  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateHeaderFile();
  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateSourceFile();

 private:
  explicit Generator(google::protobuf::FileDescriptorProto const& file_descriptor,
                     internal::DependencyManager dependencies)
      : file_descriptor_(file_descriptor), dependencies_(std::move(dependencies)) {}

  absl::StatusOr<std::string> GetHeaderPath();
  absl::StatusOr<std::string> GetHeaderGuardName();
  absl::StatusOr<std::string> GetCppPackage();

  google::protobuf::FileDescriptorProto const& file_descriptor_;
  internal::DependencyManager dependencies_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
