#ifndef __TSDB2_TSZ_INTERNAL_SHARD_H__
#define __TSDB2_TSZ_INTERNAL_SHARD_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/clock.h"
#include "common/lock_free_hash_map.h"
#include "tsz/internal/entity.h"
#include "tsz/internal/entity_proxy.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class Shard : private EntityManager {
 public:
  explicit Shard() : Shard(*tsdb2::common::RealClock::GetInstance()) {}

  explicit Shard(tsdb2::common::Clock const &clock) : clock_(clock) {}

  ~Shard() override = default;

  absl::Status DefineMetric(std::string_view metric_name, MetricConfig metric_config);

  absl::Status DefineMetricRedundant(std::string_view metric_name, MetricConfig metric_config);

  absl::StatusOr<Value> GetValue(FieldMap const &entity_labels, std::string_view metric_name,
                                 FieldMap const &metric_fields) const;

  template <typename EntityLabels, typename MetricFields>
  void SetValue(EntityLabels &&entity_labels, std::string_view const metric_name,
                MetricFields &&metric_fields, Value value) {
    GetOrCreateEntity(std::forward<EntityLabels>(entity_labels))
        .SetValue(metric_name, std::forward<MetricFields>(metric_fields), std::move(value));
  }

  template <typename EntityLabels, typename MetricFields>
  void AddToInt(EntityLabels &&entity_labels, std::string_view const metric_name,
                MetricFields &&metric_fields, int64_t const delta) {
    GetOrCreateEntity(std::forward<EntityLabels>(entity_labels))
        .AddToInt(metric_name, std::forward<MetricFields>(metric_fields), delta);
  }

  template <typename EntityLabels, typename MetricFields>
  void AddToDistribution(EntityLabels &&entity_labels, std::string_view const metric_name,
                         MetricFields &&metric_fields, double const sample, size_t const times) {
    GetOrCreateEntity(std::forward<EntityLabels>(entity_labels))
        .AddToDistribution(metric_name, std::forward<MetricFields>(metric_fields), sample, times);
  }

  bool DeleteValue(FieldMap const &entity_labels, std::string_view metric_name,
                   FieldMap const &metric_fields);

  // Deletes a metric from a specific entity. Returns a boolean indicating whether the metric was
  // actually found and deleted.
  bool DeleteMetric(FieldMap const &entity_labels, std::string_view metric_name);

  // Deletes a metric across all entities.
  //
  // WARNING: this method is VERY SLOW, as it needs to acquire the shard mutex and scan all entities
  // multiple times. It's meant mostly for test code, where there are few entities and it's
  // important to reset all global state after each test.
  void DeleteMetric(std::string_view metric_name);

  template <typename EntityLabels>
  absl::StatusOr<ScopedMetricProxy> GetPinnedMetric(EntityLabels &&entity_labels,
                                                    std::string_view const metric_name) {
    return GetOrCreateEntity(std::forward<EntityLabels>(entity_labels))
        .GetPinnedMetric(metric_name);
  }

 private:
  // Encapsulates a `MetricConfig` object guaranteeing pointer stability. We use these holders as
  // values in the `metric_configs_` map below, and they future-proof against moves and copies of
  // the embedded `MetricConfig` so that `Metric` objects can safely reference them throughout the
  // lifetime of the `Shard`.
  //
  // NOTE: we cannot delete moves & copies for `MetricConfig` itself because we want it to be
  // movable and copiable.
  class MetricConfigHolder {
   public:
    explicit MetricConfigHolder(MetricConfig value) : value_(std::move(value)) {}
    ~MetricConfigHolder() = default;

    MetricConfig const &value() const { return value_; }

   private:
    MetricConfigHolder(MetricConfigHolder const &) = delete;
    MetricConfigHolder &operator=(MetricConfigHolder const &) = delete;
    MetricConfigHolder(MetricConfigHolder &&) = delete;
    MetricConfigHolder &operator=(MetricConfigHolder &&) = delete;

    MetricConfig const value_;
  };

  // Embeds an `Entity` managed by `std::shared_ptr`. We use these cells as values in the main
  // `EntitySet`.
  class EntityCell {
   private:
    static size_t HashOf(EntityCell const &entity_cell) { return entity_cell.hash(); }
    static size_t HashOf(FieldMapView const &entity_label_view) { return entity_label_view.hash(); }

   public:
    // Custom hash functor to index entity cells by their labels transparently while taking
    // advantage of the pre-calculated hash, therefore reducing the time spent in the critical
    // section.
    struct Hash {
      using is_transparent = void;
      template <typename Arg>
      size_t operator()(Arg const &arg) const {
        return HashOf(arg);
      }
    };

    // Custom equality functor to index entity cells by their labels transparently while taking
    // advantage of the pre-calculated hash, therefore reducing the time spent in the critical
    // section.
    struct Eq {
      using is_transparent = void;

      static FieldMap const &ToLabels(EntityCell const &entity_cell) {
        return entity_cell->labels();
      }

      static FieldMap const &ToLabels(FieldMapView const &entity_label_view) {
        return entity_label_view.value();
      }

      // NOTE: we can't use perfect forwarding here because we can't move our params, we need to use
      // them twice.
      template <typename LHS, typename RHS>
      bool operator()(LHS const &lhs, RHS const &rhs) const {
        if (HashOf(lhs) != HashOf(rhs)) {
          return false;
        } else {
          return ToLabels(lhs) == ToLabels(rhs);
        }
      }
    };

    template <typename... Args>
    explicit EntityCell(Args &&...args)
        : entity_(std::make_shared<Entity>(std::forward<Args>(args)...)) {}

    ~EntityCell() = default;

    EntityCell(EntityCell const &) = default;
    EntityCell &operator=(EntityCell const &) = default;
    EntityCell(EntityCell &&) noexcept = default;
    EntityCell &operator=(EntityCell &&) noexcept = default;

    void swap(EntityCell &other) noexcept { std::swap(entity_, other.entity_); }
    friend void swap(EntityCell &lhs, EntityCell &rhs) noexcept { lhs.swap(rhs); }

    std::shared_ptr<Entity> const &ptr() const { return entity_; }
    size_t hash() const { return entity_->hash(); }
    Entity *operator->() const { return entity_.get(); }
    Entity &operator*() const { return *entity_; }

    // Makes a proxy for the entity, effectively pinning it.
    EntityProxy MakeProxy(absl::Time const time) const { return EntityProxy(entity_, time); }

   private:
    std::shared_ptr<Entity> entity_;
  };

  using EntitySet = absl::flat_hash_set<EntityCell, EntityCell::Hash, EntityCell::Eq>;

  Shard(Shard const &) = delete;
  Shard &operator=(Shard const &) = delete;
  Shard(Shard &&) = delete;
  Shard &operator=(Shard &&) = delete;

  absl::StatusOr<MetricConfig const *> GetConfigForMetric(
      std::string_view metric_name) const override;

  bool DeleteEntityInternal(FieldMap const &labels) override ABSL_LOCKS_EXCLUDED(mutex_);

  EntityProxy GetEntity(FieldMap const &entity_labels) const ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename EntityLabels>
  EntityProxy GetOrCreateEntity(EntityLabels &&entity_labels) ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<Entity> GetEphemeralEntity(FieldMap const &entity_labels) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::vector<EntityProxy> GetAllProxies() const ABSL_LOCKS_EXCLUDED(mutex_);

  tsdb2::common::Clock const &clock_;

  tsdb2::common::lock_free_hash_map<std::string, MetricConfigHolder> metric_configs_;

  absl::Mutex mutable mutex_;
  EntitySet entities_ ABSL_GUARDED_BY(mutex_);
};

template <typename EntityLabels>
EntityProxy Shard::GetOrCreateEntity(EntityLabels &&entity_labels) {
  absl::Time const now = clock_.TimeNow();
  FieldMapView const entity_label_view{entity_labels};
  absl::WriterMutexLock lock{&mutex_};
  auto it = entities_.find(entity_label_view);
  if (it == entities_.end()) {
    bool unused;
    std::tie(it, unused) = entities_.emplace(static_cast<EntityManager *>(this),
                                             std::forward<EntityLabels>(entity_labels));
  }
  return it->MakeProxy(now);
}

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_SHARD_H__
