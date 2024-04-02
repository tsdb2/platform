#ifndef __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
#define __TSDB2_TSZ_FIELD_DESCRIPTOR_H__

#include <tuple>

#include "common/type_string.h"
#include "tsz/coercion.h"

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
  using Type = FieldType;
  explicit Field(std::string_view const name) : name(name) {}
  std::string const name;
};

template <typename FieldType, char... field_name>
struct Field<FieldType, NameMatcher<field_name...>> {
  using Type = FieldType;
  std::string const name = NameMatcher<field_name...>::value;
};

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
  using Tuple = std::tuple<>;
};

template <typename... FirstFieldArgs, typename... OtherFields>
class FieldDescriptor<Field<FirstFieldArgs...>, OtherFields...>
    : private FieldDescriptor<OtherFields...> {
 public:
  using Tuple = util::CatTupleT<std::tuple<CanonicalTypeT<typename Field<FirstFieldArgs...>::Type>>,
                                typename FieldDescriptor<OtherFields...>::Tuple>;
};

template <typename... Fields>
class EntityLabels : public FieldDescriptor<Fields...> {
 public:
  using Tuple = typename FieldDescriptor<Fields...>::Tuple;
};

template <typename... Fields>
class MetricFields : public FieldDescriptor<Fields...> {
 public:
  using Tuple = typename FieldDescriptor<Fields...>::Tuple;
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_FIELD_DESCRIPTOR_H__
