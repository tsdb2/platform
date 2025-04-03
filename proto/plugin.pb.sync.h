#ifndef __TSDB2_PROTO_PLUGIN_PB_H__
#define __TSDB2_PROTO_PLUGIN_PB_H__

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "io/buffer.h"
#include "io/cord.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/proto.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace google::protobuf::compiler {

struct Version;
struct CodeGeneratorRequest;
struct CodeGeneratorResponse;

struct Version : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<Version> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(Version const& proto);

  static auto Tie(Version const& proto) {
    return std::tie(proto.major, proto.minor, proto.patch, proto.suffix);
  }

  template <typename H>
  friend H AbslHashValue(H h, Version const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Version const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(Version const& lhs, Version const& rhs) { return Tie(lhs) == Tie(rhs); }
  friend bool operator!=(Version const& lhs, Version const& rhs) { return Tie(lhs) != Tie(rhs); }
  friend bool operator<(Version const& lhs, Version const& rhs) { return Tie(lhs) < Tie(rhs); }
  friend bool operator<=(Version const& lhs, Version const& rhs) { return Tie(lhs) <= Tie(rhs); }
  friend bool operator>(Version const& lhs, Version const& rhs) { return Tie(lhs) > Tie(rhs); }
  friend bool operator>=(Version const& lhs, Version const& rhs) { return Tie(lhs) >= Tie(rhs); }

  std::optional<int32_t> major;
  std::optional<int32_t> minor;
  std::optional<int32_t> patch;
  std::optional<std::string> suffix;
};

struct CodeGeneratorRequest : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<CodeGeneratorRequest> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(CodeGeneratorRequest const& proto);

  static auto Tie(CodeGeneratorRequest const& proto) {
    return std::tie(proto.file_to_generate, proto.parameter, proto.proto_file,
                    proto.source_file_descriptors, proto.compiler_version);
  }

  template <typename H>
  friend H AbslHashValue(H h, CodeGeneratorRequest const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, CodeGeneratorRequest const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  template <typename H>
  friend H AbslHashValue(H h, Feature const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Feature const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  struct File : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<File> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(File const& proto);

    static auto Tie(File const& proto) {
      return std::tie(proto.name, proto.insertion_point, proto.content, proto.generated_code_info);
    }

    template <typename H>
    friend H AbslHashValue(H h, File const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, File const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool operator==(File const& lhs, File const& rhs) { return Tie(lhs) == Tie(rhs); }
    friend bool operator!=(File const& lhs, File const& rhs) { return Tie(lhs) != Tie(rhs); }
    friend bool operator<(File const& lhs, File const& rhs) { return Tie(lhs) < Tie(rhs); }
    friend bool operator<=(File const& lhs, File const& rhs) { return Tie(lhs) <= Tie(rhs); }
    friend bool operator>(File const& lhs, File const& rhs) { return Tie(lhs) > Tie(rhs); }
    friend bool operator>=(File const& lhs, File const& rhs) { return Tie(lhs) >= Tie(rhs); }

    std::optional<std::string> name;
    std::optional<std::string> insertion_point;
    std::optional<std::string> content;
    std::optional<::google::protobuf::GeneratedCodeInfo> generated_code_info;
  };

  static ::absl::StatusOr<CodeGeneratorResponse> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(CodeGeneratorResponse const& proto);

  static auto Tie(CodeGeneratorResponse const& proto) {
    return std::tie(proto.error, proto.supported_features, proto.minimum_edition,
                    proto.maximum_edition, proto.file);
  }

  template <typename H>
  friend H AbslHashValue(H h, CodeGeneratorResponse const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, CodeGeneratorResponse const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<std::string> error;
  std::optional<uint64_t> supported_features;
  std::optional<int32_t> minimum_edition;
  std::optional<int32_t> maximum_edition;
  std::vector<::google::protobuf::compiler::CodeGeneratorResponse::File> file;
};

}  // namespace google::protobuf::compiler

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_PLUGIN_PB_H__
