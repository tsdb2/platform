#ifndef __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
#define __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__

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
#include "proto/proto.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace tsdb2::proto {

struct DependencyMapping;

struct DependencyMapping : public ::tsdb2::proto::Message {
  static ::tsdb2::proto::MessageDescriptor<DependencyMapping, 1> const MESSAGE_DESCRIPTOR;

  struct Dependency;

  struct Dependency : public ::tsdb2::proto::Message {
    static ::tsdb2::proto::MessageDescriptor<Dependency, 2> const MESSAGE_DESCRIPTOR;

    static ::absl::StatusOr<Dependency> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Dependency const& proto);

    static auto Tie(Dependency const& proto) { return std::tie(proto.key, proto.value); }

    template <typename H>
    friend H AbslHashValue(H h, Dependency const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Dependency const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool operator==(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<std::string> key;
    std::optional<std::string> value;
  };

  static ::absl::StatusOr<DependencyMapping> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(DependencyMapping const& proto);

  static auto Tie(DependencyMapping const& proto) { return std::tie(proto.dependency); }

  template <typename H>
  friend H AbslHashValue(H h, DependencyMapping const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, DependencyMapping const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::vector<::tsdb2::proto::DependencyMapping::Dependency> dependency;
};

}  // namespace tsdb2::proto

namespace tsdb2::proto {

template <>
inline auto const& GetMessageDescriptor<::tsdb2::proto::DependencyMapping::Dependency>() {
  return ::tsdb2::proto::DependencyMapping::Dependency::MESSAGE_DESCRIPTOR;
};

template <>
inline auto const& GetMessageDescriptor<::tsdb2::proto::DependencyMapping>() {
  return ::tsdb2::proto::DependencyMapping::MESSAGE_DESCRIPTOR;
};

}  // namespace tsdb2::proto

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
