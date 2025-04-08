#ifndef __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
#define __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__

#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace tsdb2::proto::internal {

struct DependencyMapping;

struct DependencyMapping : public ::tsdb2::proto::Message {
  static ::tsdb2::proto::MessageDescriptor<DependencyMapping, 1> const MESSAGE_DESCRIPTOR;

  struct DependencyEntry;

  struct DependencyEntry : public ::tsdb2::proto::Message {
    static ::tsdb2::proto::MessageDescriptor<DependencyEntry, 2> const MESSAGE_DESCRIPTOR;

    static ::absl::StatusOr<DependencyEntry> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(DependencyEntry const& proto);

    static auto Tie(DependencyEntry const& proto) {
      return ::tsdb2::proto::Tie(proto.key, proto.value);
    }

    template <typename H>
    friend H AbslHashValue(H h, DependencyEntry const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, DependencyEntry const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool operator==(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<std::string> key;
    std::optional<std::string> value;
  };

  static ::absl::StatusOr<DependencyMapping> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(DependencyMapping const& proto);

  static auto Tie(DependencyMapping const& proto) { return ::tsdb2::proto::Tie(proto.dependency); }

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

  ::absl::flat_hash_map<std::string, std::string> dependency;
};

}  // namespace tsdb2::proto::internal

namespace tsdb2::proto {

template <>
inline auto const&
GetMessageDescriptor<::tsdb2::proto::internal::DependencyMapping::DependencyEntry>() {
  return ::tsdb2::proto::internal::DependencyMapping::DependencyEntry::MESSAGE_DESCRIPTOR;
};

template <>
inline auto const& GetMessageDescriptor<::tsdb2::proto::internal::DependencyMapping>() {
  return ::tsdb2::proto::internal::DependencyMapping::MESSAGE_DESCRIPTOR;
};

}  // namespace tsdb2::proto

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
