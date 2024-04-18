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

// Used for field names. Example:
//
//   char constexpr kFooName[] = "foo";
//   tsz::Counter<tsz::Field<int, tsz::Name<kFooName>>> foo_counter{"/foo/bar"};
//
template <char const str[]>
using Name = tsdb2::common::TypeStringT<str>;

// Used internally by tsz to implement template specializations matching the name of a field. Tsz
// users normally don't need this.
template <char... name>
using NameMatcher = tsdb2::common::TypeStringMatcher<name...>;

// Represents an entity label or metric field in a tsz metric definition.
template <typename... Args>
struct Field;

template <typename FieldType>
struct Field<FieldType> {
  using HasTypeName = std::false_type;
  static inline bool constexpr kHasTypeName = false;

  using Type = FieldType;
  using CanonicalType = CanonicalTypeT<Type>;
  using ParameterType = ParameterTypeT<Type>;

  static std::string_view GetName(std::string_view const name) { return name; }
};

template <typename FieldType, char... field_name>
struct Field<FieldType, NameMatcher<field_name...>> {
  using HasTypeName = std::true_type;
  static inline bool constexpr kHasTypeName = true;

  using Type = FieldType;
  using CanonicalType = CanonicalTypeT<Type>;
  using ParameterType = ParameterTypeT<Type>;

  static std::string_view GetName() { return NameMatcher<field_name...>::value; }
};

namespace internal {

// Checks that all fields are in the form `Field<Type, Name<kName>>`.
template <typename... Fields>
struct AllFieldsHaveTypeNames;

template <>
struct AllFieldsHaveTypeNames<> {
  static inline bool constexpr value = true;
};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct AllFieldsHaveTypeNames<Field<FirstFieldType, FirstFieldName>, OtherFields...> {
  static inline bool constexpr value = AllFieldsHaveTypeNames<OtherFields...>::value;
};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveTypeNames<Field<FirstFieldType>, OtherFields...> {
  static inline bool constexpr value = false;
};

// Checks that all fields are in the form `Field<Type>`, with the name being specified at
// construction time.
template <typename... Fields>
struct AllFieldsHaveParameterNames;

template <>
struct AllFieldsHaveParameterNames<> {
  static inline bool constexpr value = true;
};

template <typename FirstFieldType, typename... OtherFields>
struct AllFieldsHaveParameterNames<Field<FirstFieldType>, OtherFields...> {
  static inline bool constexpr value = AllFieldsHaveParameterNames<OtherFields...>::value;
};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct AllFieldsHaveParameterNames<Field<FirstFieldType, FirstFieldName>, OtherFields...> {
  static inline bool constexpr value = false;
};

template <typename... Fields>
std::array<std::string, sizeof...(Fields)> GetTypeNameArray() {
  return {std::string(Fields::GetName())...};
}

template <typename... Fields>
struct FieldDescriptorImpl;

template <>
struct FieldDescriptorImpl<> {
  template <size_t N>
  static void GetFieldNamesImpl(std::array<std::string, N>& names, size_t const index) {}
  static std::array<std::string, 0> GetFieldNames() { return {}; }
};

template <typename FirstFieldType, typename FirstFieldName, typename... OtherFields>
struct FieldDescriptorImpl<Field<FirstFieldType, FirstFieldName>, OtherFields...>
    : private FieldDescriptorImpl<OtherFields...> {
  template <size_t N>
  static void GetFieldNamesImpl(std::array<std::string, N>& names, size_t const index) {
    names[index] = Field<FirstFieldType, FirstFieldName>::name;
    FieldDescriptorImpl<OtherFields...>::GetFieldNamesImpl(names, index + 1);
  }

  static std::array<std::string, sizeof...(OtherFields) + 1> GetFieldNames() {
    std::array<std::string, sizeof...(OtherFields) + 1> names;
    GetFieldNamesImpl(names, 0);
    return names;
  }
};

}  // namespace internal

// Represents an entity label set or metric field set in a tsz metric definition. The fields must
// not have duplicate names.
//
// NOTE: avoid using `FieldDescriptor` directly. Use either `EntityLabels` (for entity labels) and
// `MetricFields` (for metric fields) instead. They both inherit `FieldDescriptor` and provide the
// same API.
//
// This template can be used in two different ways, or patterns, differing in the way field names
// are specified.
//
// In the first pattern field names are part of the type along with field types:
//
//   tsz::Counter<tsz::EntityLabels<tsz::Field<std::string, tsz::Name<kLoremName>>>,
//                tsz::MetricFields<tsz::Field<int, tsz::Name<kFooName>>>>
//       counter{"/lorem/ipsum"};
//
// In the second pattern field names are specified at construction time:
//
//   tsz::Counter<tsz::EntityLabels<tsz::Field<std::string>>, tsz::MetricFields<tsz::Field<int>>>
//       counter{"/lorem/ipsum", "lorem", "foo"};
//
// The first pattern provides better type-safety but can be much slower to compile, so if you need
// to instrument many metrics you may want to resort to the second pattern. Both patterns have the
// same runtime performance.
template <typename... Fields>
class FieldDescriptor {
 public:
  using HasTypeNames = internal::AllFieldsHaveTypeNames<Fields...>;
  static inline bool constexpr kHasTypeNames = HasTypeNames::value;

  using HasParameterNames = internal::AllFieldsHaveParameterNames<Fields...>;
  static inline bool constexpr kHasParameterNames = HasParameterNames::value;

  static_assert(kHasTypeNames || kHasParameterNames);

  using Tuple = std::tuple<CanonicalTypeT<typename Fields::Type>...>;

  // Constructs a field descriptor using the type names pattern. The names of the fields are
  // specified as type strings in the template arguments.
  //
  // Example:
  //
  //   char constexpr kLoremName[] = "lorem";
  //   char constexpr kIpsumName[] = "ipsum";
  //
  //   FieldDescriptor<Field<int, Name<kLoremName>>, Field<bool, Name<kIpsumName>>> fd;
  //
  // In the above example, the first field is an int called "lorem" and the second is a bool called
  // "ipsum".
  template <
      typename HasTypeNamesAlias = HasTypeNames,
      typename HasParameterNamesAlias = HasParameterNames,
      std::enable_if_t<HasTypeNamesAlias::value && !HasParameterNamesAlias::value, bool> = true>
  explicit FieldDescriptor() : names_(internal::GetTypeNameArray<Fields...>()) {
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
  FieldMap MakeFieldMap(typename Fields::ParameterType const... values) const;

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
    typename Fields::ParameterType const... values) const {
  std::array<FieldValue, sizeof...(Fields)> value_array{typename Fields::CanonicalType(values)...};
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
  using FieldDescriptor<Fields...>::operator=;
  using FieldDescriptor<Fields...>::names;
  using FieldDescriptor<Fields...>::MakeFieldMap;
};

// Used to specify metric fields in tsz metrics. See `FieldDescriptor` for more info.
template <typename... Fields>
class MetricFields : public FieldDescriptor<Fields...> {
 public:
  template <typename... Args>
  explicit MetricFields(Args&&... args) : FieldDescriptor<Fields...>(std::forward<Args>(args)...) {}
  using FieldDescriptor<Fields...>::operator=;
  using FieldDescriptor<Fields...>::names;
  using FieldDescriptor<Fields...>::MakeFieldMap;
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
