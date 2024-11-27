#ifndef __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
#define __TSDB2_TSZ_FIELD_DESCRIPTOR_H__

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common/fixed.h"
#include "common/type_string.h"
#include "tsz/coercion.h"
#include "tsz/types.h"

namespace tsz {

namespace internal {

template <typename... Args>
struct FieldImpl;

template <typename FieldType>
struct FieldImpl<FieldType> {
  using HasTypeName = std::false_type;
  static inline bool constexpr kHasTypeName = false;

  using Type = FieldType;
  using CanonicalType = CanonicalTypeT<Type>;
  using ParameterType = ParameterTypeT<Type>;
};

template <typename FieldType, char... field_name>
struct FieldImpl<FieldType, tsdb2::common::TypeStringMatcher<field_name...>> {
  using HasTypeName = std::true_type;
  static inline bool constexpr kHasTypeName = true;

  using Type = FieldType;
  using CanonicalType = CanonicalTypeT<Type>;
  using ParameterType = ParameterTypeT<Type>;

  static inline std::string_view constexpr name =
      tsdb2::common::TypeStringMatcher<field_name...>::value;
};

template <typename Type, char const name[]>
struct Field {
  using Impl = FieldImpl<Type, tsdb2::common::TypeStringT<name>>;
};

template <typename Type>
struct Field<Type, nullptr> {
  using Impl = FieldImpl<Type>;
};

template <typename Type, char const name[]>
using FieldT = typename Field<Type, name>::Impl;

}  // namespace internal

// Represents an entity label or metric field in a tsz metric definition.
template <typename Type, char const name[] = nullptr>
using Field = internal::FieldT<Type, name>;

namespace internal {

// Indicates whether one or more of the specified `Fields` have the specified `Name`.
//
// REQUIRES: all the provided fields must have type names, i.e. they must be in the form
// `FieldImpl<FieldType, FieldName>`.
template <typename Name, typename... Fields>
struct ContainsName;

template <char... name>
struct ContainsName<tsdb2::common::TypeStringMatcher<name...>> : public std::false_type {};

template <char... name, typename FirstFieldType, typename... OtherFields>
struct ContainsName<tsdb2::common::TypeStringMatcher<name...>,
                    FieldImpl<FirstFieldType, tsdb2::common::TypeStringMatcher<name...>>,
                    OtherFields...> : public std::true_type {};

template <char... name, typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct ContainsName<tsdb2::common::TypeStringMatcher<name...>,
                    FieldImpl<FirstFieldType, FirstFieldName>, OtherFields...>
    : public ContainsName<tsdb2::common::TypeStringMatcher<name...>, OtherFields...> {};

template <typename Name, typename... Fields>
inline bool constexpr ContainsNameV = ContainsName<Name, Fields...>::value;

// Indicates whether two or more among the provided fields have duplicate names.
//
// REQUIRES: all the provided fields must have type names, i.e. they must be in the form
// `FieldImpl<FieldType, FieldName>`.
template <typename... Fields>
struct HasDuplicateNames;

template <>
struct HasDuplicateNames<> : public std::false_type {};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct HasDuplicateNames<FieldImpl<FirstFieldType, FirstFieldName>, OtherFields...>
    : public std::disjunction<ContainsName<FirstFieldName, OtherFields...>,
                              HasDuplicateNames<OtherFields...>> {};

template <typename... Fields>
inline bool constexpr HasDuplicateNamesV = HasDuplicateNames<Fields...>::value;

// Checks that all fields are in the form `Field<Type, kName>` and that there are no duplicate
// names.
template <typename... Fields>
struct AllFieldsHaveTypeNames;

template <>
struct AllFieldsHaveTypeNames<> : public std::true_type {};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct AllFieldsHaveTypeNames<internal::FieldImpl<FirstFieldType, FirstFieldName>, OtherFields...>
    : public AllFieldsHaveTypeNames<OtherFields...> {
  static_assert(!internal::HasDuplicateNamesV<internal::FieldImpl<FirstFieldType, FirstFieldName>,
                                              OtherFields...>,
                "field descriptors must not have duplicate names");
};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveTypeNames<internal::FieldImpl<FirstFieldType>, OtherFields...>
    : public std::false_type {};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveTypeNames<FirstFieldType, OtherFields...> : public std::false_type {};

// Checks that all fields are in the form `Field<Type>`, with the name being specified at
// construction time.
template <typename... Fields>
struct AllFieldsHaveParameterNames;

template <>
struct AllFieldsHaveParameterNames<> {
  static inline bool constexpr value = true;
};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveParameterNames<internal::FieldImpl<FirstFieldType>, OtherFields...> {
  static inline bool constexpr value = AllFieldsHaveParameterNames<OtherFields...>::value;
};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct AllFieldsHaveParameterNames<internal::FieldImpl<FirstFieldType, FirstFieldName>,
                                   OtherFields...> {
  static inline bool constexpr value = false;
};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveParameterNames<FirstFieldType, OtherFields...> {
  static inline bool constexpr value = AllFieldsHaveParameterNames<OtherFields...>::value;
};

}  // namespace internal

// Infers the canonical type of a field, as per `tsz::CanonicalType`. The field can be a `FieldImpl`
// with a type name, a `FieldImpl` with parameter name, or just a field type such as `int` or
// `std::string`.
template <typename FieldType>
struct CanonicalFieldType {
  using Type = CanonicalTypeT<FieldType>;
};

template <typename... Args>
struct CanonicalFieldType<internal::FieldImpl<Args...>> {
  using Type = typename internal::FieldImpl<Args...>::CanonicalType;
};

template <typename Field>
using CanonicalFieldTypeT = typename CanonicalFieldType<Field>::Type;

// Infers the parameter type of a field, as per `tsz::ParameterType`. The field can be a `FieldImpl`
// with a type name, a `FieldImpl` with parameter name, or just a field type such as `int` or
// `std::string`.
template <typename FieldType>
struct ParameterFieldType {
  using Type = ParameterTypeT<FieldType>;
};

template <typename... Args>
struct ParameterFieldType<internal::FieldImpl<Args...>> {
  using Type = typename internal::FieldImpl<Args...>::ParameterType;
};

template <typename Field>
using ParameterFieldTypeT = typename ParameterFieldType<Field>::Type;

// Represents an entity label set or metric field set in a tsz metric definition. The fields must
// not have duplicate names.
//
// NOTE: avoid using `FieldDescriptor` directly. Use either `EntityLabels` (for entity labels) or
// `MetricFields` (for metric fields) instead. They both inherit `FieldDescriptor` and provide the
// same API.
//
// This template can be used in two different ways, or patterns, differing in the way field names
// are specified.
//
// In the first pattern field names are part of the type along with field types:
//
//   char constexpr kLoremName[] = "lorem";
//   char constexpr kFooName[] = "foo";
//
//   tsz::Counter<tsz::EntityLabels<tsz::Field<std::string, kLoremName>>,
//                tsz::MetricFields<tsz::Field<int, kFooName>>>
//       counter{"/lorem/ipsum"};
//
// In the second pattern field names are specified at construction time:
//
//   tsz::Counter<tsz::EntityLabels<tsz::Field<std::string>>, tsz::MetricFields<tsz::Field<int>>>
//       counter{"/lorem/ipsum", "lorem", "foo"};
//
// The second pattern can also be abbreviated by removing the `Field` tag altogether:
//
//   tsz::Counter<tsz::EntityLabels<std::string>, tsz::MetricFields<int>>
//       counter{"/lorem/ipsum", "lorem", "foo"};
//
// The first pattern provides better type-safety but can be much slower to compile, so if you need
// to instrument many metrics you may want to resort to the second pattern. Both patterns have the
// same runtime performance.
//
// Entity labels and metric fields can have the following types: bool, any integer type, or
// std::string. They cannot be floating point numbers (for those you need to resort to metric
// values).
template <typename... Fields>
class FieldDescriptor {
 public:
  using HasTypeNames = internal::AllFieldsHaveTypeNames<Fields...>;
  static inline bool constexpr kHasTypeNames = HasTypeNames::value;

  using HasParameterNames = internal::AllFieldsHaveParameterNames<Fields...>;
  static inline bool constexpr kHasParameterNames = HasParameterNames::value;

  static_assert(kHasTypeNames || kHasParameterNames);

  using Tuple = std::tuple<CanonicalFieldTypeT<Fields>...>;

  // Constructs a field descriptor using the type names pattern. The names of the fields are
  // specified as type strings in the template arguments.
  //
  // Example:
  //
  //   char constexpr kLoremName[] = "lorem";
  //   char constexpr kIpsumName[] = "ipsum";
  //
  //   FieldDescriptor<Field<int, kLoremName>, Field<bool, kIpsumName>> fd;
  //
  // In the above example, the first field is an int called "lorem" and the second is a bool called
  // "ipsum".
  template <
      typename HasTypeNamesAlias = HasTypeNames,
      typename HasParameterNamesAlias = HasParameterNames,
      std::enable_if_t<HasTypeNamesAlias::value && !HasParameterNamesAlias::value, bool> = true>
  explicit FieldDescriptor() : names_{std::string(Fields::name)...} {
    InitIndices();
  }

  // Constructs a field descriptor using the parameter names pattern. The names of the fields are
  // specified as constructor arguments.
  //
  // Example:
  //
  //   FieldDescriptor<Field<int>, Field<bool>> fd{"lorem", "ipsum"};
  //
  // In the above example, the first field is an int called "lorem" and the second is a bool called
  // "ipsum".
  template <typename HasParameterNamesAlias = HasParameterNames,
            std::enable_if_t<HasParameterNamesAlias::value, bool> = true>
  explicit FieldDescriptor(tsdb2::common::FixedT<std::string_view, Fields> const... names)
      : names_{std::string(names)...} {
    InitIndices();
  }

  FieldDescriptor(FieldDescriptor const&) = default;
  FieldDescriptor& operator=(FieldDescriptor const&) = default;
  FieldDescriptor(FieldDescriptor&&) noexcept = default;
  FieldDescriptor& operator=(FieldDescriptor&&) noexcept = default;

  // Returns the names of these fields.
  std::array<std::string, sizeof...(Fields)> const& names() const { return names_; }

  // Returns a `FieldMap` object mapping these fields' names to the provided values.
  FieldMap MakeFieldMap(ParameterFieldTypeT<Fields> const... values) const;

 private:
  // Called by the constructors to initialize `indices_`.
  //
  // REQUIRES: `names_` must have been initialized.
  void InitIndices();

  std::array<std::string, sizeof...(Fields)> names_;

  // `indices_` represents `names_` as if they were sorted. In other words, `indices_[x] == y` means
  // that `names_[x]` is the y-th smallest name.
  //
  // This permutation is used to speed up `MakeFieldMap` by avoiding sorting the entries.
  std::array<int, sizeof...(Fields)> indices_;
};

template <typename... Fields>
FieldMap FieldDescriptor<Fields...>::MakeFieldMap(
    ParameterFieldTypeT<Fields> const... values) const {
  std::array<FieldValue, sizeof...(Fields)> value_array{CanonicalFieldTypeT<Fields>(values)...};
  typename FieldMap::representation_type rep;
  rep.reserve(sizeof...(Fields));
  for (auto const index : indices_) {
    rep.emplace_back(names_[index], std::move(value_array[index]));
  }
  return FieldMap(tsdb2::common::kSortedDeduplicatedContainer, std::move(rep));
}

template <typename... Fields>
void FieldDescriptor<Fields...>::InitIndices() {
  std::iota(indices_.begin(), indices_.end(), 0);
  std::sort(indices_.begin(), indices_.end(),
            [this](int const lhs, int const rhs) { return names_[lhs] < names_[rhs]; });
}

// Used to specify entity labels in tsz metrics. See `FieldDescriptor` for more info.
template <typename... Fields>
class EntityLabels : public FieldDescriptor<Fields...> {
 public:
  template <typename... Args>
  explicit EntityLabels(Args&&... args) : FieldDescriptor<Fields...>(std::forward<Args>(args)...) {}

  EntityLabels(EntityLabels const&) = default;
  EntityLabels& operator=(EntityLabels const&) = default;
  EntityLabels(EntityLabels&&) noexcept = default;
  EntityLabels& operator=(EntityLabels&&) noexcept = default;

  using FieldDescriptor<Fields...>::names;
  using FieldDescriptor<Fields...>::MakeFieldMap;
};

// Used to specify metric fields in tsz metrics. See `FieldDescriptor` for more info.
template <typename... Fields>
class MetricFields : public FieldDescriptor<Fields...> {
 public:
  template <typename... Args>
  explicit MetricFields(Args&&... args) : FieldDescriptor<Fields...>(std::forward<Args>(args)...) {}

  MetricFields(MetricFields const&) = default;
  MetricFields& operator=(MetricFields const&) = default;
  MetricFields(MetricFields&&) noexcept = default;
  MetricFields& operator=(MetricFields&&) noexcept = default;

  using FieldDescriptor<Fields...>::names;
  using FieldDescriptor<Fields...>::MakeFieldMap;
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
