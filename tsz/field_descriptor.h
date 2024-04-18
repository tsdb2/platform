#ifndef __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
#define __TSDB2_TSZ_FIELD_DESCRIPTOR_H__

#include <array>
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
class FieldDescriptor;

template <>
class FieldDescriptor<> {
 public:
  using HasTypeNames = internal::AllFieldsHaveTypeNames<>;
  static inline bool constexpr kHasTypeNames = HasTypeNames::value;

  using HasParameterNames = internal::AllFieldsHaveParameterNames<>;
  static inline bool constexpr kHasParameterNames = HasParameterNames::value;

  using Tuple = std::tuple<>;

  explicit FieldDescriptor() = default;
  FieldDescriptor(FieldDescriptor const&) = default;
  FieldDescriptor& operator=(FieldDescriptor const&) = default;
  FieldDescriptor(FieldDescriptor&&) noexcept = default;
  FieldDescriptor& operator=(FieldDescriptor&&) noexcept = default;

  std::array<std::string, 0> names() const { return {}; }

  FieldMap MakeFieldMap() const { return {}; }
};

template <typename... FirstFieldArgs, typename... OtherFields>
class FieldDescriptor<Field<FirstFieldArgs...>, OtherFields...> {
 public:
  using HasTypeNames = internal::AllFieldsHaveTypeNames<Field<FirstFieldArgs...>, OtherFields...>;
  static inline bool constexpr kHasTypeNames = HasTypeNames::value;

  using HasParameterNames =
      internal::AllFieldsHaveParameterNames<Field<FirstFieldArgs...>, OtherFields...>;
  static inline bool constexpr kHasParameterNames = HasParameterNames::value;

  static_assert(kHasTypeNames || kHasParameterNames);

  using Tuple = util::CatTupleT<std::tuple<CanonicalTypeT<typename Field<FirstFieldArgs...>::Type>>,
                                typename FieldDescriptor<OtherFields...>::Tuple>;

  template <typename HasTypeNamesAlias = HasTypeNames,
            std::enable_if_t<HasTypeNamesAlias::value, bool> = true>
  explicit FieldDescriptor()
      : names_(internal::GetTypeNameArray<Field<FirstFieldArgs...>, OtherFields...>()) {}

  template <typename HasParameterNamesAlias = HasParameterNames,
            std::enable_if_t<HasParameterNamesAlias::value, bool> = true>
  explicit FieldDescriptor(
      std::string_view const first_name,
      tsdb2::common::FixedT<std::string_view, OtherFields> const... other_names)
      : names_{std::string(first_name), std::string(other_names)...} {}

  FieldDescriptor(FieldDescriptor const&) = default;
  FieldDescriptor& operator=(FieldDescriptor const&) = default;
  FieldDescriptor(FieldDescriptor&&) noexcept = default;
  FieldDescriptor& operator=(FieldDescriptor&&) noexcept = default;

  std::array<std::string, sizeof...(OtherFields) + 1> names() const { return names_; }

  FieldMap MakeFieldMap(typename Field<FirstFieldArgs...>::ParameterType first,
                        typename OtherFields::ParameterType... others) const;

 private:
  std::array<std::string, sizeof...(OtherFields) + 1> names_;
};

template <typename... Fields>
class EntityLabels : public FieldDescriptor<Fields...> {
 public:
  template <typename... Args>
  explicit EntityLabels(Args&&... args) : FieldDescriptor<Fields...>(std::forward<Args>(args)...) {}
};

template <typename... Fields>
class MetricFields : public FieldDescriptor<Fields...> {
 public:
  template <typename... Args>
  explicit MetricFields(Args&&... args) : FieldDescriptor<Fields...>(std::forward<Args>(args)...) {}
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
