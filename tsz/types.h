#ifndef __TSDB2_TSZ_TYPES_H__
#define __TSDB2_TSZ_TYPES_H__

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/time/time.h"
#include "common/flat_map.h"
#include "common/no_destructor.h"
#include "common/reffed_ptr.h"
#include "tsz/bucketer.h"
#include "tsz/distribution.h"
#include "tsz/realm.h"

namespace tsz {

using ::tsdb2::common::NoDestructor;  // re-export for convenience.

enum class TimeUnit { kNanosecond, kMicrosecond, kMillisecond, kSecond };

using FieldValue = std::variant<bool, int64_t, std::string>;
using FieldMap = tsdb2::common::flat_map<std::string, FieldValue>;
using Value = std::variant<bool, int64_t, double, std::string, Distribution>;

// `*Value` functions allow constructing `FieldValue` and `Value` objects unambiguously.
//
// Example:
//
//   FieldMap fields{
//     {"foo", IntValue(42)},
//     {"bar", BoolValue(false)},
//     {"baz", StringValue("hello")},
//   };
//
// These functions cast their input to the correct type so that the `std::variant` constructor
// overloads can resolve unambiguously. For example, the integer 42 of the example above has type
// `int` which is not one of the types of `FieldValue`, so overload resolution would fail if
// `IntValue` weren't used.

inline bool BoolValue(bool const value) { return value; }
inline int64_t IntValue(int64_t const value) { return value; }
inline double DoubleValue(double const value) { return value; }
inline std::string StringValue(std::string_view const value) { return std::string(value); }
inline Distribution DistributionValue(Distribution const &value) { return value; }
inline Distribution DistributionValue(Distribution &&value) { return std::move(value); }

// Immutable pre-hashed view of a `FieldMap` object, similar to what `std::string_view` is to
// `std::string`.
//
// NOTE: the referenced `FieldMap` MUST outlive all of its views.
class FieldMapView {
 public:
  // Constructs an empty view. The view doesn't refer to any `FieldMap` object.
  explicit FieldMapView() : value_(nullptr), hash_(0) {}

  // Constructs a view referring to the specified `FieldMap` object.
  explicit FieldMapView(FieldMap const &value) : value_(&value), hash_(absl::HashOf(*value_)) {}

  FieldMapView(FieldMapView const &) = default;
  FieldMapView &operator=(FieldMapView const &) = default;
  FieldMapView(FieldMapView &&) noexcept = default;
  FieldMapView &operator=(FieldMapView &&) noexcept = default;

  friend bool operator==(FieldMapView const &lhs, FieldMapView const &rhs) {
    if (lhs.hash() != rhs.hash()) {
      return false;
    } else {
      return lhs.value() == rhs.value();
    }
  }

  friend bool operator!=(FieldMapView const &lhs, FieldMapView const &rhs) {
    return !operator==(lhs, rhs);
  }

  [[nodiscard]] bool empty() const { return value_ == nullptr; }
  explicit operator bool() const { return value_ != nullptr; }

  FieldMap const &value() const { return *value_; }
  size_t hash() const { return hash_; }

 private:
  FieldMap const *value_;
  size_t hash_;
};

// Metric options.
struct Options {
  // This key is used to index certain settings per-(prefix,backend). The prefix component is the
  // root part of a metric name (e.g. "example.com" in "example.com/rpc/server/count"), while the
  // "backend" component is the address of the TSDB2 namespace (e.g. "tsdb2.io").
  struct BackendKey {
    explicit BackendKey(std::string_view const key_root_prefix,
                        std::string_view const key_backend_address)
        : root_prefix(key_root_prefix), backend_address(key_backend_address) {}

    BackendKey(BackendKey const &) = default;
    BackendKey &operator=(BackendKey const &) = default;
    BackendKey(BackendKey &&) noexcept = default;
    BackendKey &operator=(BackendKey &&) noexcept = default;

    auto tie() const { return std::tie(root_prefix, backend_address); }

    template <typename H>
    friend H AbslHashValue(H h, BackendKey const &key) {
      return H::combine(std::move(h), key.root_prefix, key.backend_address);
    }

    friend bool operator==(BackendKey const &lhs, BackendKey const &rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(BackendKey const &lhs, BackendKey const &rhs) {
      return lhs.tie() != rhs.tie();
    }

    std::string root_prefix;
    std::string backend_address;
  };

  // Per-(prefix,backend) settings.
  struct BackendSettings {
    // Default labels that are implicitly added to all entities.
    FieldMap default_entity_labels;

    // Sampling period for the metric. If a value is provided here the metric is exported in
    // entity-writing mode, while if this is set to `std::nullopt` the sampling period is obtained
    // from the applicable retention policies installed in the backend and the metric is exported in
    // target-writing mode.
    std::optional<absl::Duration> sampling_period;

    // Maximum time period between writes of this metric. If specified then `sampling_period` must
    // also be specified and the `max_write_period` must be either equal to or a multiple of the
    // `sampling_period`. When the `max_write_period` is a multiple, the tsz client buffers the
    // sampled points in memory and writes them in batches at least once every `max_write_period`
    // (aka "segmented writes"). That can result not only in a lower RPC rate but also a lower
    // overall byte load because the labels and fields of each time series are sent once per batch
    // rather than once per point.
    //
    // If the `sampling_period` is unspecified this field is ignored. Note that for target-writing
    // mode the max_write_period can be specified in the retention policy.
    std::optional<absl::Duration> max_write_period;

    // TODO: optional IP TOS.
  };

  // The realm a metric is associated to.
  //
  // Realms can "silo" the whole instrumentation stack: metrics associated to different realms are
  // managed by separate exporter shards, sampled by separate background threads, and written in
  // separate RPCs, even if they are in the same logical entity and are directed towards the same
  // backend(s).
  //
  // Realms are a client-side concept only, so aside from partitioning the RPCs they have no effect
  // on the backends: time series living in the same target are always stored in the same target
  // even if the exporting client or clients associate them to different realms.
  //
  // Realms are a reliability feature: partitioning the generated tsz traffic can help manage the
  // reliability of certain metric sets. For example, all predefined metamonitoring metrics are
  // associated to a separate realm called "meta" so that they are not impacted if one of the
  // metrics in the default realm becomes too large and causes the write RPCs to get dropped.
  tsdb2::common::reffed_ptr<Realm const> realm = Realm::Default();

  // A human-readable description for the metric.
  std::string description;

  // Time unit annotation, for metrics where it makes sense (e.g. `EventMetric`s tracking
  // latencies).
  std::optional<TimeUnit> time_unit = std::nullopt;

  // When enabled, skips a point at sampling time if its value didn't change since the last time it
  // was sampled. Ignored for non-cumulative metrics.
  //
  // TODO: explain further.
  //
  // `skip_stable_cells` is often used in conjunction with `delta_mode` (see below). Enabling both
  // flags achieves a behavior that is similar to the clear-on-push algorithm implemented in
  // `ClearOnPushCounter` and `ClearOnPushEventMetric`, although these two are much more efficient.
  //
  // NOTE: this flag works only if the metric is exported in entity-writing mode to all backends,
  // that is only if a single explicit sampling period is provided for all backends (in the
  // `default_backend_settings`), otherwise it's ignored. We can't keep track of the last update
  // time of each value on a per-backend basis.
  bool skip_stable_cells = false;

  // When enabled the metric is exported in DELTA form, meaning the value cell is reset at every
  // sampling. Ignored for non-cumulative metrics.
  //
  // TODO: explain further.
  //
  // NOTE: this flag works only if the metric is exported in entity-writing mode to all backends,
  // that is only if a single explicit sampling period is provided for all backends (in the
  // `default_backend_settings`), otherwise it's ignored. We can't keep track of whether a value
  // cell has been reset on a per-backend basis.
  bool delta_mode = false;

  bool user_timestamps = false;

  // The `Bucketer` used for a distribution metric. nullptr means using the default bucketer (as per
  // `Bucketer::Default()`). Ignored for non-distribution metrics.
  Bucketer const *bucketer = nullptr;

  std::optional<absl::Duration> max_entity_staleness = std::nullopt;
  std::optional<absl::Duration> max_value_staleness = std::nullopt;

  BackendSettings default_backend_settings;
  absl::flat_hash_map<BackendKey, BackendSettings> backend_settings;
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_TYPES_H__
