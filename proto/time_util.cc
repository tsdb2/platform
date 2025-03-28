#include "proto/time_util.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "proto/duration.pb.sync.h"
#include "proto/timestamp.pb.sync.h"

namespace tsdb2 {
namespace proto {

absl::StatusOr<absl::Time> DecodeGoogleApiProto(::google::protobuf::Timestamp const& proto) {
  int64_t const seconds = proto.seconds.value_or(0);
  int32_t const nanos = proto.nanos.value_or(0);
  if (nanos < 0) {
    return absl::InvalidArgumentError(
        "invalid timestamp encoding (nanoseconds must not be negative)");
  }
  static int32_t constexpr kMaxNanos = 999999999;
  if (nanos > kMaxNanos) {
    return absl::OutOfRangeError("invalid duration encoding (nanoseconds are out of range)");
  }
  return absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Duration> DecodeGoogleApiProto(::google::protobuf::Duration const& proto) {
  int64_t const seconds = proto.seconds.value_or(0);
  int32_t const nanos = proto.nanos.value_or(0);
  if (seconds < 0 != nanos < 0) {
    return absl::InvalidArgumentError("invalid duration encoding (sign conflict)");
  }
  static int64_t constexpr kMinSeconds = -315576000000;
  static int64_t constexpr kMaxSeconds = 315576000000;
  if (seconds < kMinSeconds || seconds > kMaxSeconds) {
    return absl::OutOfRangeError("invalid duration encoding (seconds are out of range)");
  }
  static int32_t constexpr kMinNanos = -999999999;
  static int32_t constexpr kMaxNanos = 999999999;
  if (nanos < kMinNanos || nanos > kMaxNanos) {
    return absl::OutOfRangeError("invalid duration encoding (nanoseconds are out of range)");
  }
  return absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

::google::protobuf::Timestamp EncodeGoogleApiProto(absl::Time const time) {
  auto const duration = time - absl::UnixEpoch();
  static absl::Duration constexpr kOneSecond = absl::Seconds(1);
  if (duration < absl::ZeroDuration()) {
    auto const lower_bound = absl::Floor(duration, kOneSecond);
    return ::google::protobuf::Timestamp{
        .seconds = absl::ToInt64Seconds(lower_bound),
        .nanos = absl::ToInt64Nanoseconds(duration - lower_bound),
    };
  } else {
    return ::google::protobuf::Timestamp{
        .seconds = absl::ToInt64Seconds(duration),
        .nanos = absl::ToInt64Nanoseconds(duration % kOneSecond),
    };
  }
}

::google::protobuf::Duration EncodeGoogleApiProto(absl::Duration const duration) {
  static absl::Duration constexpr kOneSecond = absl::Seconds(1);
  return ::google::protobuf::Duration{
      .seconds = absl::ToInt64Seconds(duration),
      .nanos = absl::ToInt64Nanoseconds(duration % kOneSecond),
  };
}

}  // namespace proto
}  // namespace tsdb2
