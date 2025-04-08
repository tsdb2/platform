#ifndef __TSDB2_PROTO_ANNOTATIONS_PB_H__
#define __TSDB2_PROTO_ANNOTATIONS_PB_H__

#include "proto/descriptor.pb.sync.h"
#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace tsdb2::proto {

enum class FieldIndirectionType {
  INDIRECTION_DIRECT = 0,
  INDIRECTION_UNIQUE = 1,
  INDIRECTION_SHARED = 2,
};

template <typename H>
inline H AbslHashValue(H h, FieldIndirectionType const& value) {
  return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
}

template <typename State>
inline State Tsdb2FingerprintValue(State state, FieldIndirectionType const& value) {
  return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
}

struct google_protobuf_FieldOptions_extension : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<google_protobuf_FieldOptions_extension> Decode(
      ::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(google_protobuf_FieldOptions_extension const& proto);

  static auto Tie(google_protobuf_FieldOptions_extension const& proto) {
    return ::tsdb2::proto::Tie(proto.indirect);
  }

  template <typename H>
  friend H AbslHashValue(H h, google_protobuf_FieldOptions_extension const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state,
                                     google_protobuf_FieldOptions_extension const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(google_protobuf_FieldOptions_extension const& lhs,
                        google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(google_protobuf_FieldOptions_extension const& lhs,
                        google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<::tsdb2::proto::FieldIndirectionType> indirect;
};

}  // namespace tsdb2::proto

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_ANNOTATIONS_PB_H__
