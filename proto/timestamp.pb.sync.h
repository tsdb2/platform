#ifndef __TSDB2_PROTO_TIMESTAMP_PB_SYNC_H__
#define __TSDB2_PROTO_TIMESTAMP_PB_SYNC_H__

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "io/cord.h"
#include "proto/proto.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace google::protobuf {

struct Timestamp;

struct Timestamp : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<Timestamp> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(Timestamp const& proto);

  static auto Tie(Timestamp const& proto) { return std::tie(proto.seconds, proto.nanos); }

  template <typename H>
  friend H AbslHashValue(H h, Timestamp const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Timestamp const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(Timestamp const& lhs, Timestamp const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(Timestamp const& lhs, Timestamp const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(Timestamp const& lhs, Timestamp const& rhs) { return Tie(lhs) < Tie(rhs); }
  friend bool operator<=(Timestamp const& lhs, Timestamp const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(Timestamp const& lhs, Timestamp const& rhs) { return Tie(lhs) > Tie(rhs); }
  friend bool operator>=(Timestamp const& lhs, Timestamp const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<int64_t> seconds;
  std::optional<int32_t> nanos;
};

}  // namespace google::protobuf

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_TIMESTAMP_PB_SYNC_H__
