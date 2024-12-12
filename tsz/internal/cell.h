#ifndef __TSDB2_TSZ_INTERNAL_CELL_H__
#define __TSDB2_TSZ_INTERNAL_CELL_H__

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/time/time.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

// A single tsz value cell.
//
// NOTE: this class is not thread-safe, only thread-friendly.
//
// Each cell contains:
//
//   * the metric fields associated to that value within the metric,
//   * the tsz value,
//   * the cell creation timestamp,
//   * the last update timestamp.
//
// The metric fields are immutable and pre-hashed, so that the cell can be easily stored in an
// `absl::flat_hash_set`.
//
// The value can be one of: a boolean, a 64-bit signed integer, an `std::string`, a `double`, or a
// distribution.
class Cell {
 private:
  static size_t ToHash(Cell const &cell) { return cell.hash(); }
  static size_t ToHash(FieldMapView const &field_view) { return field_view.hash(); }

 public:
  // Custom hash functor to store `Cell`s in an `absl::flat_hash_set`. It's fast because the metric
  // fields are pre-hashed.
  struct Hash {
    using is_transparent = void;
    template <typename Arg>
    size_t operator()(Arg const &arg) const {
      return ToHash(arg);
    }
  };

  // Custom equals functor to store `Cell`s in an `absl::flat_hash_set`. It's fast because it
  // short-circuits if the two precalculated hashes are different, so it avoids a full comparison of
  // the field maps in that case.
  struct Eq {
    using is_transparent = void;

    static FieldMap const &ToFields(Cell const &cell) { return cell.metric_fields(); }
    static FieldMap const &ToFields(FieldMapView const &field_view) { return field_view.value(); }

    // NOTE: we can't use perfect forwarding here because we can't move our params, we need to use
    // them twice.
    template <typename LHS, typename RHS>
    bool operator()(LHS const &lhs, RHS const &rhs) const {
      if (ToHash(lhs) != ToHash(rhs)) {
        return false;
      } else {
        return ToFields(lhs) == ToFields(rhs);
      }
    }
  };

  struct DistributionCell {};
  static inline DistributionCell constexpr kDistributionCell;

  // REQUIRES: `hash` MUST be the hash of `metric_fields` obtained from `absl::HashOf`. We cannot
  // calculate it ourselves because this constructor is executed in a latency-sensitive context.
  template <typename MetricFields>
  explicit Cell(MetricFields &&metric_fields, size_t const hash, Value value, absl::Time const now)
      : metric_fields_(std::forward<MetricFields>(metric_fields)),
        hash_(hash),
        value_(std::move(value)),
        start_time_(now),
        last_update_time_(now) {}

  // REQUIRES: `hash` MUST be the hash of `metric_fields` obtained from `absl::HashOf`. We cannot
  // calculate it ourselves because this constructor is executed in a latency-sensitive context.
  template <typename MetricFields>
  explicit Cell(DistributionCell /*distribution_cell*/, MetricFields &&metric_fields,
                size_t const hash, Bucketer const *const bucketer, absl::Time const now)
      : metric_fields_(std::forward<MetricFields>(metric_fields)),
        hash_(hash),
        value_(Distribution(bucketer ? *bucketer : Bucketer::Default())),
        start_time_(now),
        last_update_time_(now) {}

  ~Cell() = default;

  Cell(Cell const &) = default;
  Cell &operator=(Cell const &) = default;
  Cell(Cell &&) noexcept = default;
  Cell &operator=(Cell &&) noexcept = default;

  void swap(Cell &other);
  friend void swap(Cell &lhs, Cell &rhs) { lhs.swap(rhs); }

  FieldMap const &metric_fields() const { return metric_fields_; }
  size_t hash() const { return hash_; }

  Value &value() { return value_; }
  Value const &value() const { return value_; }

  absl::Time start_time() const { return start_time_; }
  absl::Time last_update_time() const { return last_update_time_; }

  void SetValue(Value &&value, absl::Time const now) {
    value_ = std::move(value);
    last_update_time_ = now;
  }

  void AddToInt(int64_t const delta, absl::Time const now) {
    std::get<int64_t>(value_) += delta;
    last_update_time_ = now;
  }

  void AddToDistribution(double const sample, size_t const times, absl::Time const now) {
    std::get<Distribution>(value_).RecordMany(sample, times);
    last_update_time_ = now;
  }

  // Resets the cell to a "zero value", setting its timestamps to the provided `new_start_time`.
  //
  // This is used to reset all cumulative metrics when the default entity labels change.
  void Reset(absl::Time new_start_time);

 private:
  FieldMap metric_fields_;
  size_t hash_;
  Value value_;
  absl::Time start_time_;
  absl::Time last_update_time_;
};

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_CELL_H__
