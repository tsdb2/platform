#ifndef __TSDB2_TSZ_INTERNAL_ENTITY_H__
#define __TSDB2_TSZ_INTERNAL_ENTITY_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/ref_count.h"
#include "tsz/internal/metric.h"
#include "tsz/internal/metric_config.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/internal/throw_away_metric_proxy.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class EntityManager {
 public:
  explicit EntityManager() = default;
  virtual ~EntityManager() = default;

  virtual absl::StatusOr<MetricConfig const *> GetConfigForMetric(
      std::string_view metric_name) const = 0;

  virtual bool DeleteEntityInternal(FieldMap const &labels) = 0;

 private:
  EntityManager(EntityManager const &) = delete;
  EntityManager &operator=(EntityManager const &) = delete;
  EntityManager(EntityManager &&) = delete;
  EntityManager &operator=(EntityManager &&) = delete;
};

class EntityContext;

class Entity : public MetricManager {
 public:
  explicit Entity(EntityManager *const manager, FieldMap labels)
      : manager_(manager), labels_(std::move(labels)), hash_(absl::HashOf(labels_)) {}

  ~Entity() override = default;

  FieldMap const &labels() const { return labels_; }
  size_t hash() const { return hash_; }

  bool is_pinned() const { return pin_count_.is_referenced(); }
  void Pin() { pin_count_.Ref(); }
  void Unpin() ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<Value> GetValue(std::string_view metric_name, FieldMap const &metric_fields);

  template <typename MetricFields>
  void SetValue(EntityContext const &context, std::string_view metric_name,
                MetricFields &&metric_fields, Value value);

  template <typename MetricFields>
  void AddToInt(EntityContext const &context, std::string_view metric_name,
                MetricFields &&metric_fields, int64_t delta);

  template <typename MetricFields>
  void AddToDistribution(EntityContext const &context, std::string_view metric_name,
                         MetricFields &&metric_fields, double sample, size_t times);

  bool DeleteValue(EntityContext const &context, std::string_view metric_name,
                   FieldMap const &metric_fields);

  bool DeleteMetric(EntityContext const &context, std::string_view metric_name);

  absl::StatusOr<ScopedMetricProxy> GetPinnedMetric(EntityContext const &context,
                                                    std::string_view metric_name);

 private:
  class MetricCell {
   public:
    template <typename... Args>
    explicit MetricCell(Args &&...args)
        : metric_(std::make_shared<Metric>(std::forward<Args>(args)...)) {}

    ~MetricCell() = default;

    MetricCell(MetricCell const &) = default;
    MetricCell &operator=(MetricCell const &) = default;
    MetricCell(MetricCell &&) noexcept = default;
    MetricCell &operator=(MetricCell &&) noexcept = default;

    void swap(MetricCell &other) noexcept { std::swap(metric_, other.metric_); }
    friend void swap(MetricCell &lhs, MetricCell &rhs) noexcept { lhs.swap(rhs); }

    std::shared_ptr<Metric> const &ptr() const { return metric_; }
    Metric *operator->() const { return metric_.get(); }
    Metric &operator*() const { return *metric_; }

    absl::StatusOr<ScopedMetricProxy> MakeScopedProxy(EntityContext const &context) const;
    absl::StatusOr<ThrowAwayMetricProxy> MakeThrowAwayProxy(EntityContext const &context) const;

   private:
    std::shared_ptr<Metric> metric_;
  };

  // Custom hash functor to index `Metric` objects by their name transparently while taking
  // advantage of their pre-calculated hash, therefore reducing the time spent in the critical
  // section.
  struct Hash {
    using is_transparent = void;

    size_t operator()(MetricCell const &metric) const { return metric->hash(); }

    size_t operator()(std::string_view const metric_name) const {
      return absl::HashOf(metric_name);
    }
  };

  // Custom equality functor to index `Metric` objects by their name transparently while taking
  // advantage of their pre-calculated hash, therefore reducing the time spent in the critical
  // section.
  struct Eq {
    using is_transparent = void;

    static std::string_view GetMetricName(MetricCell const &metric) { return metric->name(); }

    static std::string_view GetMetricName(std::string_view const metric_name) {
      return metric_name;
    }

    template <typename LHS, typename RHS>
    bool operator()(LHS &&lhs, RHS &&rhs) const {
      return GetMetricName(std::forward<LHS>(lhs)) == GetMetricName(std::forward<RHS>(rhs));
    }
  };

  using MetricSet = absl::flat_hash_set<MetricCell, Hash, Eq>;

  friend class EntityContext;

  Entity(Entity const &) = delete;
  Entity &operator=(Entity const &) = delete;
  Entity(Entity &&) = delete;
  Entity &operator=(Entity &&) = delete;

  void DeleteMetricInternal(std::string_view name) override ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<ThrowAwayMetricProxy> GetMetric(EntityContext const &context,
                                                 std::string_view metric_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<ThrowAwayMetricProxy> GetOrCreateMetric(EntityContext const &context,
                                                         std::string_view metric_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::shared_ptr<Metric> GetEphemeralMetric(std::string_view metric_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  EntityManager *const manager_;  // not owned

  FieldMap const labels_;
  size_t const hash_;

  absl::Mutex mutable mutex_;
  tsdb2::common::RefCount pin_count_;
  MetricSet metrics_ ABSL_GUARDED_BY(mutex_);
};

class EntityContext final {
 public:
  explicit EntityContext() : entity_(nullptr) {}

  explicit EntityContext(std::shared_ptr<Entity> entity, absl::Time const time)
      : entity_(std::move(entity)), time_(time) {
    if (entity_) {
      entity_->Pin();
    }
  }

  ~EntityContext() { MaybeUnpin(); }

  EntityContext(EntityContext &&other) noexcept = default;

  EntityContext &operator=(EntityContext &&other) noexcept {
    if (this != &other) {
      MaybeUnpin();
      entity_ = std::move(other.entity_);
      time_ = other.time_;
    }
    return *this;
  }

  std::shared_ptr<Entity> const &entity() const { return entity_; }
  absl::Time time() const { return time_; }

 private:
  // No copies because we don't want to pin/unpin more than once per context.
  EntityContext(EntityContext const &) = delete;
  EntityContext &operator=(EntityContext const &) = delete;

  void MaybeUnpin() {
    if (entity_) {
      entity_->Unpin();
    }
  }

  std::shared_ptr<Entity> entity_;
  absl::Time time_;
};

template <typename MetricFields>
void Entity::SetValue(EntityContext const &context, std::string_view const metric_name,
                      MetricFields &&metric_fields, Value value) {
  auto status_or_metric = GetOrCreateMetric(context, metric_name);
  if (status_or_metric.ok()) {
    status_or_metric->SetValue(std::forward<MetricFields>(metric_fields), std::move(value));
  }
}

template <typename MetricFields>
void Entity::AddToInt(EntityContext const &context, std::string_view const metric_name,
                      MetricFields &&metric_fields, int64_t const delta) {
  auto status_or_metric = GetOrCreateMetric(context, metric_name);
  if (status_or_metric.ok()) {
    status_or_metric->AddToInt(std::forward<MetricFields>(metric_fields), delta);
  }
}

template <typename MetricFields>
void Entity::AddToDistribution(EntityContext const &context, std::string_view const metric_name,
                               MetricFields &&metric_fields, double const sample,
                               size_t const times) {
  auto status_or_metric = GetOrCreateMetric(context, metric_name);
  if (status_or_metric.ok()) {
    status_or_metric->AddToDistribution(std::forward<MetricFields>(metric_fields), sample, times);
  }
}

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_ENTITY_H__
