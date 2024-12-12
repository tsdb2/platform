#ifndef __TSDB2_TSZ_ENTITY_H__
#define __TSDB2_TSZ_ENTITY_H__

#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "tsz/base.h"

namespace tsz {

// Abstract interface for `tsz::Entity`. Suitable for use with `reffed_ptr`.
class EntityInterface {
 public:
  explicit EntityInterface() = default;
  virtual ~EntityInterface() = default;
  virtual FieldMap const &labels() const = 0;
  virtual void Ref() const = 0;
  virtual bool Unref() const = 0;

 private:
  EntityInterface(EntityInterface const &) = delete;
  EntityInterface &operator=(EntityInterface const &) = delete;
  EntityInterface(EntityInterface &&) = delete;
  EntityInterface &operator=(EntityInterface &&) = delete;
};

// Represents an entity to which you can bind one or more tsz metrics. This pattern is known as
// "bound-entity metrics".
//
// The entity is uniquely identified by a set of labels and the corresponding values. Label names
// and types are part of the type specification of the `Entity` object, while label values must be
// provided to the `Entity` constructor.
//
// NOTE: entities defined this way do not support parametric label names, so the label names must be
// provided as type strings (see the example below).
//
// NOTE: it is possible to instantiate two or more `Entity` object with the same type (i.e. label
// names and types) and label values, in which case the two entities will compare equal using the
// provided comparison operators.
//
// Example usage:
//
//   char constexpr kLoremLabel[] = "lorem";
//   char constexpr kIpsumLabel[] = "ipsum";
//   char constexpr kDolorLabel[] = "dolor";
//
//   tsz::NoDestructor<tsz::Entity<
//       tsz::Field<std::string, kLoremLabel>,
//       tsz::Field<int, kIpsumLabel>,
//       tsz::Field<bool, kDolorLabel>>> const entity{
//     /*lorem=*/ tsz::StringValue("blah"),
//     /*ipsum=*/ tsz::IntValue("123"),
//     /*dolor=*/ tsz::BoolValue(true),
//   };
//
//   tsz::NoDestructor<tsz::Counter<
//       tsz::Field<int, kFooField>,
//       tsz::Field<bool, kBarField>>> counter{*entity, "/foo/bar/count"};
//
//   tsz::NoDestructor<tsz::Metric<int, bool>> metric{*entity, "/bar/foo/count", "bar", "foo"};
//
// The above example uses `NoDestructor` because both the entity and the metrics are defined in
// global scope.
//
// Both `counter` and `metric` will export all their values in the entity identified by the labels
// `lorem=blah`, `ipsum=123`, and `dolor=true`.
//
// WARNING: destruction of the `Entity` blocks until there are no more metrics bound to it. To avoid
// potential deadlocks, make sure that is the case before trying to destroy an `Entity`.
//
// Bound-entity metrics are faster than the unbound counterparts because under the hood the bound
// implementation hooks directly into an internal component and performs one less hash & lookup as
// well as two less mutex locks. On the other hand the bound-entity API requires manual management
// of each entity and its lifetime, and therefore entails higher complexity on the user side.
template <typename... LabelArgs>
class Entity : public EntityInterface {
 public:
  using LabelDescriptor = EntityLabels<LabelArgs...>;

  template <typename Other>
  struct CompareEqual;

  template <typename... OtherLabelArgs>
  struct CompareEqual<Entity<OtherLabelArgs...>> {
    bool operator()(Entity<LabelArgs...> const &lhs, Entity<OtherLabelArgs...> const &rhs) const {
      return lhs.labels_ == rhs.labels_;
    }
  };

  template <typename Other>
  struct CompareLess;

  template <typename... OtherLabelArgs>
  struct CompareLess<Entity<OtherLabelArgs...>> {
    bool operator()(Entity<LabelArgs...> const &lhs, Entity<OtherLabelArgs...> const &rhs) const {
      return lhs.labels_ < rhs.labels_;
    }
  };

  template <typename EntityLabelsAlias = LabelDescriptor,
            std::enable_if_t<EntityLabelsAlias::kHasTypeNames, bool> = true>
  explicit Entity(ParameterFieldTypeT<LabelArgs> const... label_values)
      : labels_(descriptor_.MakeFieldMap(label_values...)) {}

  ~Entity() override {
    absl::MutexLock(&mutex_, absl::Condition(this, &Entity::ref_count_is_zero));
  }

  LabelDescriptor const &descriptor() const { return descriptor_; }

  FieldMap const &labels() const override { return labels_; }

  template <typename H>
  friend H AbslHashValue(H h, Entity const &entity) {
    return H::combine(std::move(h), entity.labels_);
  }

  template <typename H>
  friend H Tsdb2FingerprintValue(H h, Entity const &entity) {
    return H::Combine(std::move(h), entity.labels_);
  }

  template <typename Other>
  bool operator==(Other const &other) const {
    return CompareEqual<Other>{}(*this, other);
  }

  template <typename Other>
  bool operator!=(Other const &other) const {
    return !CompareEqual<Other>{}(*this, other);
  }

  template <typename Other>
  bool operator<(Other const &other) const {
    return CompareLess<Other>{}(*this, other);
  }

  template <typename Other>
  bool operator<=(Other const &other) const {
    return !CompareLess<Other>{}(other, *this);
  }

  template <typename Other>
  bool operator>(Other const &other) const {
    return CompareLess<Other>{}(other, *this);
  }

  template <typename Other>
  bool operator>=(Other const &other) const {
    return !CompareLess<Other>{}(*this, other);
  }

  intptr_t ref_count() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    return ref_count_;
  }

  void Ref() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    ++ref_count_;
  }

  bool Unref() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    DCHECK_GT(ref_count_, 0);
    return --ref_count_ == 0;
  }

 private:
  Entity(Entity const &) = delete;
  Entity &operator=(Entity const &) = delete;
  Entity(Entity &&) = delete;
  Entity &operator=(Entity &&) = delete;

  bool ref_count_is_zero() const ABSL_SHARED_LOCKS_REQUIRED(mutex_) { return ref_count_ == 0; }

  LabelDescriptor const descriptor_;
  FieldMap labels_;

  absl::Mutex mutable mutex_;
  intptr_t mutable ref_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

// Returns the default entity, i.e. the one with no labels except the default ones.
//
// Note that default entity labels can be configured on a per-metric basis, so it's possible that
// different metrics attached to this entity get different entity labels.
Entity<> const &GetDefaultEntity();

}  // namespace tsz

#endif  // __TSDB2_TSZ_ENTITY_H__
