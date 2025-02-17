#ifndef __TSDB2_PROTO_PLUGIN_PB_H__
#define __TSDB2_PROTO_PLUGIN_PB_H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "io/cord.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/proto.h"

namespace google::protobuf::compiler {

struct Version;
struct CodeGeneratorRequest;
struct CodeGeneratorResponse;

struct Version : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<Version> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(Version const& proto);

  std::optional<int32_t> major;
  std::optional<int32_t> minor;
  std::optional<int32_t> patch;
  std::optional<std::string> suffix;
};

struct CodeGeneratorRequest : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<CodeGeneratorRequest> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(CodeGeneratorRequest const& proto);

  std::vector<std::string> file_to_generate;
  std::optional<std::string> parameter;
  std::vector<::google::protobuf::FileDescriptorProto> proto_file;
  std::vector<::google::protobuf::FileDescriptorProto> source_file_descriptors;
  std::optional<::google::protobuf::compiler::Version> compiler_version;
};

struct CodeGeneratorResponse : public ::tsdb2::proto::Message {
  struct File;

  enum class Feature {
    FEATURE_NONE = 0,
    FEATURE_PROTO3_OPTIONAL = 1,
    FEATURE_SUPPORTS_EDITIONS = 2,
  };

  struct File : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<File> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(File const& proto);

    std::optional<std::string> name;
    std::optional<std::string> insertion_point;
    std::optional<std::string> content;
    std::optional<::google::protobuf::GeneratedCodeInfo> generated_code_info;
  };

  static ::absl::StatusOr<CodeGeneratorResponse> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(CodeGeneratorResponse const& proto);

  std::optional<std::string> error;
  std::optional<uint64_t> supported_features;
  std::optional<int32_t> minimum_edition;
  std::optional<int32_t> maximum_edition;
  std::vector<::google::protobuf::compiler::CodeGeneratorResponse::File> file;
};

}  // namespace google::protobuf::compiler

#endif  // __TSDB2_PROTO_PLUGIN_PB_H__
