#ifndef __TSDB2_PROTO_REFLECTION_H__
#define __TSDB2_PROTO_REFLECTION_H__

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/to_array.h"
#include "common/trie_map.h"
#include "common/utilities.h"
#include "proto/proto.h"

namespace tsdb2 {
namespace proto {

namespace internal {

template <typename Type, typename Descriptor, typename Enable = void>
struct IsDescriptorForType {
  static inline bool constexpr value = std::is_same_v<Descriptor, std::monostate>;
};

template <typename Type, typename Descriptor>
inline bool constexpr IsDescriptorForTypeV = IsDescriptorForType<Type, Descriptor>::value;

template <typename... Types>
struct OneofTypes {};

template <typename... Descriptors>
struct OneofDescriptors {};

template <typename Types, typename Descriptors>
struct CheckDescriptorsForTypes;

template <>
struct CheckDescriptorsForTypes<OneofTypes<>, OneofDescriptors<>> {
  static inline bool constexpr value = true;
};

template <typename FirstType, typename... OtherTypes, typename FirstDescriptor,
          typename... OtherDescriptors>
struct CheckDescriptorsForTypes<OneofTypes<FirstType, OtherTypes...>,
                                OneofDescriptors<FirstDescriptor, OtherDescriptors...>> {
  static inline bool constexpr value =
      internal::IsDescriptorForTypeV<FirstType, FirstDescriptor> &&
      CheckDescriptorsForTypes<OneofTypes<OtherTypes...>,
                               OneofDescriptors<OtherDescriptors...>>::value;
};

template <typename Types, typename Descriptors>
static inline bool constexpr CheckDescriptorsForTypesV =
    CheckDescriptorsForTypes<Types, Descriptors>::value;

}  // namespace internal

// Base class for all enum descriptors (see the `EnumDescriptor` template below).
//
// You must not create instances of this class. Instances are already provided for every generated
// enum type through the generated `<EnumType>_ENUM_DESCRIPTOR` globals, where `<EnumType>` is the
// name of the enum type.
//
// NOTE: the whole reflection API is NOT thread-safe, only thread-friendly. It's the user's
// responsibility to ensure proper synchronization. The same goes for the protobufs themselves.
class BaseEnumDescriptor {
 public:
  explicit BaseEnumDescriptor() = default;
  virtual ~BaseEnumDescriptor() = default;

  virtual absl::Span<std::string_view const> GetValueNames() const = 0;
  virtual absl::StatusOr<int64_t> GetValueForName(std::string_view name) const = 0;
  virtual absl::StatusOr<std::string_view> GetNameForValue(int64_t value) const = 0;

 private:
  BaseEnumDescriptor(BaseEnumDescriptor const&) = delete;
  BaseEnumDescriptor& operator=(BaseEnumDescriptor const&) = delete;
  BaseEnumDescriptor(BaseEnumDescriptor&&) = delete;
  BaseEnumDescriptor& operator=(BaseEnumDescriptor&&) = delete;
};

template <typename Enum, size_t num_values>
class EnumDescriptor : public BaseEnumDescriptor {
 public:
  using UnderlyingType = std::underlying_type_t<Enum>;

  explicit constexpr EnumDescriptor(
      std::pair<std::string_view, UnderlyingType> const (&values)[num_values])
      : value_names_(MakeValueNames(values)),
        values_by_name_(tsdb2::common::fixed_flat_map_of(values)),
        names_by_value_(IndexNamesByValue(values, std::make_index_sequence<num_values>())) {}

  absl::Span<std::string_view const> GetValueNames() const override { return value_names_; }

  absl::StatusOr<int64_t> GetValueForName(std::string_view const name) const override {
    auto const it = values_by_name_.find(name);
    if (it == values_by_name_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid enum value name: \"", absl::CEscape(name), "\""));
    }
    return it->second;
  }

  absl::StatusOr<std::string_view> GetNameForValue(int64_t const value) const override {
    auto const it = names_by_value_.find(value);
    if (it == names_by_value_.end()) {
      return absl::InvalidArgumentError(absl::StrCat("unknown enum value: ", value));
    }
    return it->second;
  }

  absl::StatusOr<std::string_view> GetValueName(Enum const value) const {
    auto const it = names_by_value_.find(tsdb2::util::to_underlying(value));
    if (it == names_by_value_.end()) {
      return absl::InvalidArgumentError("invalid enum value");
    }
    return it->second;
  }

  absl::StatusOr<Enum> GetNameValue(std::string_view const name) const {
    auto const it = values_by_name_.find(name);
    if (it == values_by_name_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid enum value name: \"", absl::CEscape(name), "\""));
    }
    return static_cast<Enum>(it->second);
  }

  absl::Status SetValueByName(Enum* const ptr, std::string_view const name) const {
    DEFINE_CONST_OR_RETURN(value, GetNameValue(name));
    *ptr = value;
    return absl::OkStatus();
  }

 private:
  static constexpr std::array<std::string_view, num_values> MakeValueNames(
      std::pair<std::string_view, UnderlyingType> const (&values)[num_values]) {
    std::array<std::string_view, num_values> array;
    for (size_t i = 0; i < num_values; ++i) {
      array[i] = values[i].first;
    }
    return array;
  }

  template <size_t... Is>
  static constexpr tsdb2::common::fixed_flat_map<UnderlyingType, std::string_view, num_values>
  IndexNamesByValue(std::pair<std::string_view, UnderlyingType> const (&values)[num_values],
                    std::index_sequence<Is...> /*index_sequence*/) {
    return tsdb2::common::fixed_flat_map_of<UnderlyingType, std::string_view>(
        {{values[Is].second, values[Is].first}...});
  }

  std::array<std::string_view, num_values> value_names_;
  tsdb2::common::fixed_flat_map<std::string_view, UnderlyingType, num_values> values_by_name_;
  tsdb2::common::fixed_flat_map<UnderlyingType, std::string_view, num_values> names_by_value_;
};

// We need to specialize the version with 0 values because zero element arrays are not permitted in
// C++. The constructor of this specialization takes no arguments.
template <typename Enum>
class EnumDescriptor<Enum, 0> : public BaseEnumDescriptor {
 public:
  using UnderlyingType = std::underlying_type_t<Enum>;

  explicit constexpr EnumDescriptor() = default;

  absl::Span<std::string_view const> GetValueNames() const override { return {}; }
};

namespace internal {

template <typename Enum, typename Descriptor>
struct IsDescriptorForType<Enum, Descriptor, std::enable_if_t<std::is_enum_v<Enum>>> {
  static inline bool constexpr value =
      std::is_base_of_v<BaseEnumDescriptor, std::decay_t<Descriptor>>;
};

// TODO: can we remove the *Impl versions of these definitions in C++20, after migrating from SFINAE
// to concepts?

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using StdMapImpl = std::map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using StdMap = StdMapImpl<EntryMessage>;

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using StdUnorderedMapImpl =
    std::unordered_map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using StdUnorderedMap = StdUnorderedMapImpl<EntryMessage>;

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using FlatHashMapImpl = absl::flat_hash_map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using FlatHashMap = FlatHashMapImpl<EntryMessage>;

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using NodeHashMapImpl = absl::node_hash_map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using NodeHashMap = NodeHashMapImpl<EntryMessage>;

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using BtreeMapImpl = absl::btree_map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using BtreeMap = BtreeMapImpl<EntryMessage>;

template <typename EntryMessage, std::enable_if_t<internal::IsMapEntryV<EntryMessage>, bool> = true>
using FlatMapImpl = tsdb2::common::flat_map<MapKeyType<EntryMessage>, MapValueType<EntryMessage>>;

template <typename EntryMessage>
using FlatMap = FlatMapImpl<EntryMessage>;

template <typename EntryMessage,
          std::enable_if_t<internal::IsMapEntryV<EntryMessage> &&
                               std::is_same_v<MapKeyType<EntryMessage>, std::string>,
                           bool> = true>
using TrieMapImpl = tsdb2::common::trie_map<MapValueType<EntryMessage>>;

template <typename EntryMessage>
using TrieMap = TrieMapImpl<EntryMessage>;

}  // namespace internal

// Base class for all message descriptors (see the `MessageDescriptor` template below).
//
// You must not create instances of this class. Instances are already provided in every generated
// message type through the `MESSAGE_DESCRIPTOR` static property.
//
// NOTE: the whole reflection API is NOT thread-safe, only thread-friendly. It's the user's
// responsibility to ensure proper synchronization. The same goes for the protobufs themselves.
class BaseMessageDescriptor {
 public:
  // WARNING: don't change the order and numbering of the `FieldType`, `FieldKind`, and
  // `LabeledFieldType` enum values. Save for a few exceptions (e.g. oneof fields), the rest of the
  // code makes assumptions about the numbering for various purposes, for example when decomposing a
  // `LabeledFieldType` into the corresponding `FieldType` and `FieldKind`.

  enum class FieldType : int8_t {
    kInt32Field = 0,
    kUInt32Field = 1,
    kInt64Field = 2,
    kUInt64Field = 3,
    kBoolField = 4,
    kStringField = 5,
    kBytesField = 6,
    kDoubleField = 7,
    kFloatField = 8,
    kTimeField = 9,
    kDurationField = 10,
    kEnumField = 11,
    kSubMessageField = 12,
    kMapField = 13,
    kOneOfField = 14,
  };

  enum class FieldKind : int8_t {
    kRaw = 0,
    kOptional = 1,
    kRepeated = 2,
    kMap = 3,
    kOneOf = 4,
  };

  enum class LabeledFieldType : int8_t {
    kRawInt32Field = 0,
    kOptionalInt32Field = 1,
    kRepeatedInt32Field = 2,
    kRawUInt32Field = 3,
    kOptionalUInt32Field = 4,
    kRepeatedUInt32Field = 5,
    kRawInt64Field = 6,
    kOptionalInt64Field = 7,
    kRepeatedInt64Field = 8,
    kRawUInt64Field = 9,
    kOptionalUInt64Field = 10,
    kRepeatedUInt64Field = 11,
    kRawBoolField = 12,
    kOptionalBoolField = 13,
    kRepeatedBoolField = 14,
    kRawStringField = 15,
    kOptionalStringField = 16,
    kRepeatedStringField = 17,
    kRawBytesField = 18,
    kOptionalBytesField = 19,
    kRepeatedBytesField = 20,
    kRawDoubleField = 21,
    kOptionalDoubleField = 22,
    kRepeatedDoubleField = 23,
    kRawFloatField = 24,
    kOptionalFloatField = 25,
    kRepeatedFloatField = 26,
    kRawTimeField = 27,
    kOptionalTimeField = 28,
    kRepeatedTimeField = 29,
    kRawDurationField = 30,
    kOptionalDurationField = 31,
    kRepeatedDurationField = 32,
    kRawEnumField = 33,
    kOptionalEnumField = 34,
    kRepeatedEnumField = 35,
    kRawSubMessageField = 36,
    kOptionalSubMessageField = 37,
    kRepeatedSubMessageField = 38,
    kMapField = 39,
    kOneOfField = 40,
  };

  // Keeps information about an enum-typed field.
  class RawEnum final {
   public:
    template <typename Enum, typename Descriptor>
    explicit RawEnum(Enum* const field, Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(field, descriptor)) {}

    ~RawEnum() = default;

    RawEnum(RawEnum const&) = default;
    RawEnum& operator=(RawEnum const&) = default;

    RawEnum(RawEnum&&) noexcept = default;
    RawEnum& operator=(RawEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    bool HasKnownValue() const { return impl_->HasKnownValue(); }

    absl::StatusOr<std::string_view> GetValue() const { return impl_->GetValue(); }

    int64_t GetUnderlyingValue() const { return impl_->GetUnderlyingValue(); }

    absl::Status SetValue(std::string_view name) { return impl_->SetValue(name); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual bool HasKnownValue() const = 0;
      virtual absl::StatusOr<std::string_view> GetValue() const = 0;
      virtual int64_t GetUnderlyingValue() const = 0;
      virtual absl::Status SetValue(std::string_view name) = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(Enum* const field, Descriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasKnownValue() const override {
        auto const status_or_value = descriptor_.GetValueName(*field_);
        return status_or_value.ok();
      }

      absl::StatusOr<std::string_view> GetValue() const override {
        return descriptor_.GetValueName(*field_);
      }

      int64_t GetUnderlyingValue() const override { return tsdb2::util::to_underlying(*field_); }

      absl::Status SetValue(std::string_view const name) override {
        return descriptor_.SetValueByName(field_, name);
      }

     private:
      Enum* const field_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about an optional enum-typed field.
  class OptionalEnum final {
   public:
    template <typename Enum, typename Descriptor>
    explicit OptionalEnum(std::optional<Enum>* const field, Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(field, descriptor)) {}

    ~OptionalEnum() = default;

    OptionalEnum(OptionalEnum const&) = default;
    OptionalEnum& operator=(OptionalEnum const&) = default;

    OptionalEnum(OptionalEnum&&) noexcept = default;
    OptionalEnum& operator=(OptionalEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    bool HasValue() const { return impl_->HasValue(); }
    bool HasKnownValue() const { return impl_->HasKnownValue(); }

    absl::StatusOr<std::string_view> GetValue() const { return impl_->GetValue(); }

    int64_t GetUnderlyingValue() const { return impl_->GetUnderlyingValue(); }

    absl::Status SetValue(std::string_view name) { return impl_->SetValue(name); }

    bool EraseValue() { return impl_->EraseValue(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual bool HasValue() const = 0;
      virtual bool HasKnownValue() const = 0;
      virtual absl::StatusOr<std::string_view> GetValue() const = 0;
      virtual int64_t GetUnderlyingValue() const = 0;
      virtual absl::Status SetValue(std::string_view name) = 0;
      virtual bool EraseValue() = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<Enum>* const field, Descriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasValue() const override { return field_->has_value(); }

      bool HasKnownValue() const override {
        if (!field_->has_value()) {
          return false;
        }
        auto const status_or_value = descriptor_.GetValueName(field_->value());
        return status_or_value.ok();
      }

      absl::StatusOr<std::string_view> GetValue() const override {
        return descriptor_.GetValueName(field_->value());
      }

      int64_t GetUnderlyingValue() const override {
        return tsdb2::util::to_underlying(field_->value());
      }

      absl::Status SetValue(std::string_view const name) override {
        DEFINE_CONST_OR_RETURN(value, descriptor_.GetNameValue(name));
        field_->emplace(value);
        return absl::OkStatus();
      }

      bool EraseValue() override {
        bool const result = field_->has_value();
        field_->reset();
        return result;
      }

     private:
      std::optional<Enum>* const field_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about a repeated enum-typed field.
  class RepeatedEnum final {
   public:
    using value_type = std::string_view;

    class const_iterator final {
     public:
      explicit const_iterator() : parent_(nullptr), index_(0) {}
      ~const_iterator() = default;

      const_iterator(const_iterator const&) = default;
      const_iterator& operator=(const_iterator const&) = default;
      const_iterator(const_iterator&&) noexcept = default;
      const_iterator& operator=(const_iterator&&) noexcept = default;

      auto tie() const { return std::tie(parent_, index_); }

      friend bool operator==(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() == rhs.tie();
      }

      friend bool operator!=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() != rhs.tie();
      }

      friend bool operator<(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() < rhs.tie();
      }

      friend bool operator<=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() <= rhs.tie();
      }

      friend bool operator>(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() > rhs.tie();
      }

      friend bool operator>=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() >= rhs.tie();
      }

      explicit operator bool() const { return parent_ != nullptr; }

      std::string_view operator*() const { return parent_->operator[](index_); }

      const_iterator& operator++() {
        ++index_;
        return *this;
      }

      const_iterator operator++(int) { return const_iterator(*parent_, index_++); }

     private:
      friend class RepeatedEnum;

      explicit const_iterator(RepeatedEnum const& parent, size_t const index)
          : parent_(&parent), index_(index) {}

      RepeatedEnum const* parent_;
      size_t index_;
    };

    template <typename Enum, typename Descriptor>
    explicit RepeatedEnum(std::vector<Enum>* const values, Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(values, descriptor)) {}

    ~RepeatedEnum() = default;

    RepeatedEnum(RepeatedEnum const&) = default;
    RepeatedEnum& operator=(RepeatedEnum const&) = default;

    RepeatedEnum(RepeatedEnum&&) noexcept = default;
    RepeatedEnum& operator=(RepeatedEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    size_t size() const { return impl_->GetSize(); }
    [[nodiscard]] bool empty() const { return impl_->GetSize() == 0; }

    bool HasKnownValueAt(size_t const index) const { return impl_->HasKnownValueAt(index); }

    absl::StatusOr<std::string_view> GetValueAt(size_t const index) const {
      return impl_->GetValueAt(index);
    }

    int64_t GetUnderlyingValueAt(size_t const index) const {
      return impl_->GetUnderlyingValueAt(index);
    }

    bool AllValuesAreKnown() const {
      size_t const size = impl_->GetSize();
      for (size_t i = 0; i < size; ++i) {
        if (!impl_->HasKnownValueAt(i)) {
          return false;
        }
      }
      return true;
    }

    std::string_view operator[](size_t const index) const {
      return impl_->GetValueAt(index).value_or("");
    }

    const_iterator begin() const { return const_iterator(*this, 0); }
    const_iterator cbegin() const { return const_iterator(*this, 0); }

    const_iterator end() const { return const_iterator(*this, impl_->GetSize()); }
    const_iterator cend() const { return const_iterator(*this, impl_->GetSize()); }

    absl::Status AppendValue(std::string_view const name) { return impl_->AppendValue(name); }

    absl::Status SetAllValues(absl::Span<std::string_view const> const names) {
      return impl_->SetAllValues(names);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;

      virtual size_t GetSize() const = 0;

      virtual bool HasKnownValueAt(size_t index) const = 0;

      virtual absl::StatusOr<std::string_view> GetValueAt(size_t index) const = 0;

      virtual int64_t GetUnderlyingValueAt(size_t index) const = 0;

      virtual absl::Status AppendValue(std::string_view name) = 0;

      virtual absl::Status SetAllValues(absl::Span<std::string_view const> names) = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<Enum>* const values, Descriptor const& descriptor)
          : values_(values), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      size_t GetSize() const override { return values_->size(); }

      bool HasKnownValueAt(size_t const index) const override {
        auto const status_or_value = descriptor_.GetValueName(values_->at(index));
        return status_or_value.ok();
      }

      absl::StatusOr<std::string_view> GetValueAt(size_t const index) const override {
        return descriptor_.GetValueName(values_->at(index));
      }

      int64_t GetUnderlyingValueAt(size_t const index) const override {
        return tsdb2::util::to_underlying(values_->at(index));
      }

      absl::Status AppendValue(std::string_view name) override {
        DEFINE_CONST_OR_RETURN(value, descriptor_.GetNameValue(name));
        values_->emplace_back(value);
        return absl::OkStatus();
      }

      absl::Status SetAllValues(absl::Span<std::string_view const> names) override {
        values_->resize(names.size());
        for (size_t i = 0; i < names.size(); ++i) {
          RETURN_IF_ERROR(descriptor_.SetValueByName(&((*values_)[i]), names[i]));
        }
        return absl::OkStatus();
      }

     private:
      std::vector<Enum>* const values_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about a message-typed field. This is essentially a pair of a pointer to the
  // message and its `BaseMessageDescriptor`. This type is cheap to copy and can be passed by value.
  class RawSubMessage final {
   public:
    explicit RawSubMessage(Message* const message, BaseMessageDescriptor const& descriptor)
        : message_(message), descriptor_(&descriptor) {}

    ~RawSubMessage() = default;

    RawSubMessage(RawSubMessage const&) = default;
    RawSubMessage& operator=(RawSubMessage const&) = default;

    RawSubMessage(RawSubMessage&&) noexcept = default;
    RawSubMessage& operator=(RawSubMessage&&) noexcept = default;

    Message const& message() const { return *message_; }
    Message* mutable_message() { return message_; }

    BaseMessageDescriptor const& descriptor() const { return *descriptor_; }

   private:
    Message* message_;
    BaseMessageDescriptor const* descriptor_;
  };

  // Keeps information about a message-typed optional field. This is essentially a pair of a pointer
  // to the (optional) message and its `BaseMessageDescriptor`.
  class OptionalSubMessage final {
   public:
    template <typename SubMessage>
    explicit OptionalSubMessage(std::optional<SubMessage>* const message,
                                BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<OptionalImpl<SubMessage>>(message, descriptor)) {}

    template <typename SubMessage>
    explicit OptionalSubMessage(std::unique_ptr<SubMessage>* const message,
                                BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<UniqueImpl<SubMessage>>(message, descriptor)) {}

    template <typename SubMessage>
    explicit OptionalSubMessage(std::shared_ptr<SubMessage>* const message,
                                BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<SharedImpl<SubMessage>>(message, descriptor)) {}

    ~OptionalSubMessage() = default;

    OptionalSubMessage(OptionalSubMessage const&) = default;
    OptionalSubMessage& operator=(OptionalSubMessage const&) = default;

    OptionalSubMessage(OptionalSubMessage&&) noexcept = default;
    OptionalSubMessage& operator=(OptionalSubMessage&&) noexcept = default;

    bool has_value() const { return impl_->HasValue(); }

    Message const& message() const { return impl_->GetValue(); }
    Message* mutable_message() { return impl_->GetMutableValue(); }

    bool Erase() { return impl_->Erase(); }
    Message* Reset() { return impl_->Reset(); }

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;
      virtual bool HasValue() const = 0;
      virtual Message const& GetValue() const = 0;
      virtual Message* GetMutableValue() = 0;
      virtual bool Erase() = 0;
      virtual Message* Reset() = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename SubMessage>
    class OptionalImpl : public BaseImpl {
     public:
      explicit OptionalImpl(std::optional<SubMessage>* const message,
                            BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasValue() const override { return message_->has_value(); }
      Message const& GetValue() const override { return message_->value(); }
      Message* GetMutableValue() override { return &(message_->value()); }

      bool Erase() override {
        bool const result = message_->has_value();
        message_->reset();
        return result;
      }

      Message* Reset() override { return &(message_->emplace()); }

     private:
      std::optional<SubMessage>* const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    template <typename SubMessage>
    class UniqueImpl : public BaseImpl {
     public:
      explicit UniqueImpl(std::unique_ptr<SubMessage>* const message,
                          BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasValue() const override { return message_->operator bool(); }
      Message const& GetValue() const override { return *(message_->get()); }
      Message* GetMutableValue() override { return message_->get(); }

      bool Erase() override {
        bool const result = message_->operator bool();
        message_->reset();
        return result;
      }

      Message* Reset() override {
        *message_ = std::make_unique<SubMessage>();
        return message_->get();
      }

     private:
      std::unique_ptr<SubMessage>* const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    template <typename SubMessage>
    class SharedImpl : public BaseImpl {
     public:
      explicit SharedImpl(std::shared_ptr<SubMessage>* const message,
                          BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasValue() const override { return message_->operator bool(); }
      Message const& GetValue() const override { return *(message_->get()); }
      Message* GetMutableValue() override { return message_->get(); }

      bool Erase() override {
        bool const result = message_->operator bool();
        message_->reset();
        return result;
      }

      Message* Reset() override {
        *message_ = std::make_shared<SubMessage>();
        return message_->get();
      }

     private:
      std::shared_ptr<SubMessage>* const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about a message-typed repeated field. This is essentially a pair of a pointer
  // to the message array and its `BaseMessageDescriptor`.
  class RepeatedSubMessage final {
   public:
    class iterator final {
     public:
      explicit iterator() : parent_(nullptr), index_(0) {}
      ~iterator() = default;

      iterator(iterator const&) = default;
      iterator& operator=(iterator const&) = default;
      iterator(iterator&&) noexcept = default;
      iterator& operator=(iterator&&) noexcept = default;

      explicit operator bool() const { return parent_ != nullptr; }

      Message& operator*() const { return parent_->operator[](index_); }
      Message* operator->() const { return &(parent_->operator[](index_)); }

      iterator& operator++() {
        ++index_;
        return *this;
      }

      iterator operator++(int) { return iterator(parent_, index_++); }

     private:
      friend class RepeatedSubMessage;

      explicit iterator(RepeatedSubMessage* const parent, size_t const index)
          : parent_(parent), index_(index) {}

      RepeatedSubMessage* parent_;
      size_t index_;
    };

    class const_iterator final {
     public:
      explicit const_iterator() : parent_(nullptr), index_(0) {}
      ~const_iterator() = default;

      const_iterator(const_iterator const&) = default;
      const_iterator& operator=(const_iterator const&) = default;
      const_iterator(const_iterator&&) noexcept = default;
      const_iterator& operator=(const_iterator&&) noexcept = default;

      auto tie() const { return std::tie(parent_, index_); }

      friend bool operator==(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() == rhs.tie();
      }

      friend bool operator!=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() != rhs.tie();
      }

      friend bool operator<(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() < rhs.tie();
      }

      friend bool operator<=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() <= rhs.tie();
      }

      friend bool operator>(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() > rhs.tie();
      }

      friend bool operator>=(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.tie() >= rhs.tie();
      }

      explicit operator bool() const { return parent_ != nullptr; }

      Message const& operator*() const { return parent_->operator[](index_); }
      Message const* operator->() const { return &(parent_->operator[](index_)); }

      const_iterator& operator++() {
        ++index_;
        return *this;
      }

      const_iterator operator++(int) { return const_iterator(*parent_, index_++); }

     private:
      friend class RepeatedSubMessage;

      explicit const_iterator(RepeatedSubMessage const& parent, size_t const index)
          : parent_(&parent), index_(index) {}

      RepeatedSubMessage const* parent_;
      size_t index_;
    };

    template <typename SubMessage>
    explicit RepeatedSubMessage(std::vector<SubMessage>* const messages,
                                BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<SubMessage>>(messages, descriptor)) {}

    ~RepeatedSubMessage() = default;

    RepeatedSubMessage(RepeatedSubMessage const&) = default;
    RepeatedSubMessage& operator=(RepeatedSubMessage const&) = default;

    RepeatedSubMessage(RepeatedSubMessage&&) noexcept = default;
    RepeatedSubMessage& operator=(RepeatedSubMessage&&) noexcept = default;

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    size_t size() const { return impl_->GetSize(); }
    [[nodiscard]] bool empty() const { return impl_->GetSize() == 0; }

    void Clear() { impl_->Clear(); }
    void Reserve(size_t const size) { impl_->Reserve(size); }

    Message* Append() { return impl_->Append(); }

    Message& operator[](size_t const index) { return *(impl_->GetAt(index)); }
    Message const& operator[](size_t const index) const { return *(impl_->GetAt(index)); }

    iterator begin() { return iterator(this, 0); }
    const_iterator begin() const { return const_iterator(*this, 0); }
    const_iterator cbegin() const { return const_iterator(*this, 0); }

    iterator end() { return iterator(this, impl_->GetSize()); }
    const_iterator end() const { return const_iterator(*this, impl_->GetSize()); }
    const_iterator cend() const { return const_iterator(*this, impl_->GetSize()); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;

      virtual size_t GetSize() const = 0;
      virtual void Clear() = 0;
      virtual void Reserve(size_t size) = 0;

      virtual Message* GetAt(size_t index) const = 0;

      virtual Message* Append() = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename SubMessage>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<SubMessage>* const messages,
                    BaseMessageDescriptor const& descriptor)
          : messages_(messages), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      size_t GetSize() const override { return messages_->size(); }

      void Clear() override { messages_->clear(); }
      void Reserve(size_t const size) override { messages_->reserve(size); }

      Message* GetAt(size_t const index) const override { return &(messages_->at(index)); }

      Message* Append() override { return &(messages_->emplace_back()); }

     private:
      std::vector<SubMessage>* const messages_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  using MapKey = std::variant<int32_t, uint32_t, int64_t, uint64_t, bool, std::string>;

  using MapValue = std::variant<int32_t, uint32_t, int64_t, uint64_t, bool, std::string_view,
                                absl::Span<uint8_t const>, double, float, absl::Time,
                                absl::Duration, RawEnum const, RawSubMessage const>;

  using MapValueRef = std::variant<int32_t*, uint32_t*, int64_t*, uint64_t*, bool*, std::string*,
                                   std::vector<uint8_t>*, double*, float*, absl::Time*,
                                   absl::Duration*, RawEnum, RawSubMessage>;

  class Map final {
   private:
    class BaseImpl {
     public:
      class Iterator {
       public:
        explicit Iterator() = default;
        virtual ~Iterator() = default;

        virtual bool operator==(Iterator const& other) const = 0;

        virtual std::shared_ptr<Iterator> Clone() const = 0;

        virtual std::pair<MapKey, MapValueRef> Dereference() const = 0;
        virtual std::pair<MapKey, MapValue> DereferenceConst() const = 0;

        virtual void Advance() = 0;

       private:
        Iterator(Iterator const&) = delete;
        Iterator& operator=(Iterator const&) = delete;
        Iterator(Iterator&&) = delete;
        Iterator& operator=(Iterator&&) = delete;
      };

      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual bool IsOrdered() const = 0;
      virtual BaseMessageDescriptor const& GetEntryDescriptor() const = 0;

      virtual size_t GetSize() const = 0;
      virtual bool IsEmpty() const = 0;

      virtual std::shared_ptr<Iterator> MakeBeginIterator() const = 0;
      virtual std::shared_ptr<Iterator> MakeEndIterator() const = 0;

      virtual void Clear() = 0;
      virtual void Reserve(size_t size) = 0;

      virtual absl::StatusOr<bool> Contains(MapKey const& key) const = 0;
      virtual absl::StatusOr<std::shared_ptr<Iterator>> Find(MapKey const& key) const = 0;

      virtual absl::StatusOr<bool> Erase(MapKey const& key) = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

   public:
    class iterator final {
     public:
      iterator() = default;
      ~iterator() = default;

      iterator(iterator const&) = default;
      iterator& operator=(iterator const&) = default;

      iterator(iterator&&) noexcept = default;
      iterator& operator=(iterator&&) noexcept = default;

      void swap(iterator& other) noexcept { std::swap(it_, other.it_); }
      friend void swap(iterator& lhs, iterator& rhs) noexcept { lhs.swap(rhs); }

      friend bool operator==(iterator const& lhs, iterator const& rhs) {
        return lhs.it_ == rhs.it_ || *(lhs.it_) == *(rhs.it_);
      }

      friend bool operator!=(iterator const& lhs, iterator const& rhs) {
        return !operator==(lhs, rhs);
      }

      std::pair<MapKey, MapValueRef> operator*() const { return it_->Dereference(); }

     private:
      friend class Map;

      explicit iterator(std::shared_ptr<BaseImpl::Iterator> it) : it_(std::move(it)) {}

      std::shared_ptr<BaseImpl::Iterator> it_;
    };

    class const_iterator final {
     public:
      const_iterator() = default;

      explicit const_iterator(iterator const& other) : it_(other.it_) {}

      ~const_iterator() = default;

      const_iterator(const_iterator const&) = default;
      const_iterator& operator=(const_iterator const&) = default;

      const_iterator(const_iterator&&) noexcept = default;
      const_iterator& operator=(const_iterator&&) noexcept = default;

      void swap(const_iterator& other) noexcept { std::swap(it_, other.it_); }
      friend void swap(const_iterator& lhs, const_iterator& rhs) noexcept { lhs.swap(rhs); }

      friend bool operator==(const_iterator const& lhs, const_iterator const& rhs) {
        return lhs.it_ == rhs.it_ || *(lhs.it_) == *(rhs.it_);
      }

      friend bool operator!=(const_iterator const& lhs, const_iterator const& rhs) {
        return !operator==(lhs, rhs);
      }

      std::pair<MapKey, MapValue> operator*() const { return it_->DereferenceConst(); }

     private:
      friend class Map;

      explicit const_iterator(std::shared_ptr<BaseImpl::Iterator> it) : it_(std::move(it)) {}

      std::shared_ptr<BaseImpl::Iterator> it_;
    };

    template <template <typename> class MapType, typename EntryMessage, typename ValueDescriptor>
    static Map Create(MapType<EntryMessage>* const map,
                      BaseMessageDescriptor const& entry_descriptor,
                      ValueDescriptor const& value_descriptor) {
      return Map(std::make_shared<Impl<MapType, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    ~Map() = default;

    Map(Map const&) = default;
    Map& operator=(Map const&) = default;

    Map(Map&&) noexcept = default;
    Map& operator=(Map&&) noexcept = default;

    bool is_ordered() const { return impl_->IsOrdered(); }
    BaseMessageDescriptor const& entry_descriptor() const { return impl_->GetEntryDescriptor(); }

    size_t size() const { return impl_->GetSize(); }
    [[nodiscard]] bool empty() const { return impl_->IsEmpty(); }

    iterator begin() { return iterator(impl_->MakeBeginIterator()); }
    const_iterator begin() const { return const_iterator(impl_->MakeBeginIterator()); }
    const_iterator cbegin() const { return const_iterator(impl_->MakeBeginIterator()); }

    iterator end() { return iterator(impl_->MakeEndIterator()); }
    const_iterator end() const { return const_iterator(impl_->MakeEndIterator()); }
    const_iterator cend() const { return const_iterator(impl_->MakeEndIterator()); }

    void Clear() { impl_->Clear(); }
    void Reserve(size_t const size) { impl_->Reserve(size); }

    absl::StatusOr<bool> Contains(MapKey const& key) const { return impl_->Contains(key); }

    absl::StatusOr<iterator> Find(MapKey const& key) {
      DEFINE_VAR_OR_RETURN(it, impl_->Find(key));
      return iterator(std::move(it));
    }

    absl::StatusOr<const_iterator> Find(MapKey const& key) const {
      DEFINE_VAR_OR_RETURN(it, impl_->Find(key));
      return const_iterator(std::move(it));
    }

    absl::StatusOr<bool> Erase(MapKey const& key) { return impl_->Erase(key); }

   private:
    struct WrapKey final {
      MapKey operator()(int32_t const key) const { return key; }
      MapKey operator()(int64_t const key) const { return key; }
      MapKey operator()(uint32_t const key) const { return key; }
      MapKey operator()(uint64_t const key) const { return key; }
      MapKey operator()(bool const key) const { return key; }
      MapKey operator()(std::string_view const key) const { return std::string(key); }
    };

    template <typename ValueDescriptor>
    class WrapValueRef final {
     public:
      explicit WrapValueRef(ValueDescriptor const& value_descriptor)
          : value_descriptor_(value_descriptor) {}

      MapValueRef operator()(int32_t& value) const { return &value; }
      MapValueRef operator()(int64_t& value) const { return &value; }
      MapValueRef operator()(uint32_t& value) const { return &value; }
      MapValueRef operator()(uint64_t& value) const { return &value; }
      MapValueRef operator()(bool& value) const { return &value; }
      MapValueRef operator()(std::string& value) const { return &value; }
      MapValueRef operator()(std::vector<uint8_t>& value) const { return &value; }
      MapValueRef operator()(double& value) const { return &value; }
      MapValueRef operator()(float& value) const { return &value; }
      MapValueRef operator()(absl::Time& value) const { return &value; }
      MapValueRef operator()(absl::Duration& value) const { return &value; }

      template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
      MapValueRef operator()(Enum& value) const {
        return RawEnum(&value, value_descriptor_);
      }

      template <typename SubMessage,
                std::enable_if_t<std::is_base_of_v<Message, SubMessage>, bool> = true>
      MapValueRef operator()(SubMessage& value) const {
        return RawSubMessage(&value, value_descriptor_);
      }

     private:
      ValueDescriptor const& value_descriptor_;
    };

    template <typename ValueDescriptor>
    class WrapValue final {
     public:
      explicit WrapValue(ValueDescriptor const& value_descriptor)
          : value_descriptor_(value_descriptor) {}

      MapValue operator()(int32_t const value) const { return value; }
      MapValue operator()(int64_t const value) const { return value; }
      MapValue operator()(uint32_t const value) const { return value; }
      MapValue operator()(uint64_t const value) const { return value; }
      MapValue operator()(bool const value) const { return value; }
      MapValue operator()(std::string_view const value) const { return value; }
      MapValue operator()(double const value) const { return value; }
      MapValue operator()(float const value) const { return value; }
      MapValue operator()(absl::Time const value) const { return value; }
      MapValue operator()(absl::Duration const value) const { return value; }

      MapValue operator()(std::vector<uint8_t> const& value) const {
        return absl::Span<uint8_t const>(value);
      }

      template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
      MapValue operator()(Enum& value) const {
        return RawEnum(&value, value_descriptor_);
      }

      template <typename SubMessage,
                std::enable_if_t<std::is_base_of_v<Message, SubMessage>, bool> = true>
      MapValue operator()(SubMessage& value) const {
        return RawSubMessage(&value, value_descriptor_);
      }

     private:
      ValueDescriptor const& value_descriptor_;
    };

    template <typename Key>
    struct UnwrapKey final {
      template <typename Alias = Key, std::enable_if_t<std::is_same_v<Alias, int32_t>, bool> = true>
      absl::StatusOr<Key> operator()(int32_t const key) const {
        return key;
      }

      template <typename Alias = Key,
                std::enable_if_t<std::is_same_v<Alias, uint32_t>, bool> = true>
      absl::StatusOr<Key> operator()(uint32_t const key) const {
        return key;
      }

      template <typename Alias = Key, std::enable_if_t<std::is_same_v<Alias, int64_t>, bool> = true>
      absl::StatusOr<Key> operator()(int64_t const key) const {
        return key;
      }

      template <typename Alias = Key,
                std::enable_if_t<std::is_same_v<Alias, uint64_t>, bool> = true>
      absl::StatusOr<Key> operator()(uint64_t const key) const {
        return key;
      }

      template <typename Alias = Key, std::enable_if_t<std::is_same_v<Alias, bool>, bool> = true>
      absl::StatusOr<Key> operator()(bool const key) const {
        return key;
      }

      template <typename Alias = Key,
                std::enable_if_t<std::is_same_v<Alias, std::string>, bool> = true>
      absl::StatusOr<Key> operator()(std::string_view const key) const {
        return std::string(key);
      }

      template <typename Arg, typename Alias = Key,
                std::enable_if_t<!std::is_same_v<Alias, Arg>, bool> = true>
      absl::StatusOr<Key> operator()(Arg&& arg) const {
        return absl::FailedPreconditionError("invalid key type");
      }
    };

    template <template <typename> class MapType, typename EntryMessage, typename ValueDescriptor,
              std::enable_if_t<internal::IsMapEntryV<EntryMessage> &&
                                   internal::IsDescriptorForTypeV<
                                       internal::MapValueType<EntryMessage>, ValueDescriptor>,
                               bool> = true>
    class Impl final : public BaseImpl {
     public:
      class Iterator final : public BaseImpl::Iterator {
       public:
        explicit Iterator(BaseMessageDescriptor const& entry_descriptor,
                          ValueDescriptor const& value_descriptor)
            : entry_descriptor_(entry_descriptor), value_descriptor_(value_descriptor) {}

        explicit Iterator(BaseMessageDescriptor const& entry_descriptor,
                          ValueDescriptor const& value_descriptor,
                          typename MapType<EntryMessage>::iterator it)
            : entry_descriptor_(entry_descriptor),
              value_descriptor_(value_descriptor),
              it_(std::move(it)) {}

        bool operator==(BaseImpl::Iterator const& other) const override {
          // Since exceptions are disabled, dynamic reference casts are unsafe because they can't
          // throw `std::bad_cast` when they receive the incorrect type. Dynamic pointer casts are
          // safe, we just need to check for nullptr.
          Iterator const* const downcast = dynamic_cast<Iterator const*>(&other);
          if (downcast != nullptr) {
            return it_ == downcast->it_;
          } else {
            return false;
          }
        }

        std::shared_ptr<BaseImpl::Iterator> Clone() const override {
          return std::make_shared<Iterator>(entry_descriptor_, value_descriptor_, it_);
        }

        std::pair<MapKey, MapValueRef> Dereference() const override {
          return std::make_pair(WrapKey{}(it_->first),
                                WrapValueRef{value_descriptor_}(it_->second));
        }

        std::pair<MapKey, MapValue> DereferenceConst() const override {
          return std::make_pair(WrapKey{}(it_->first), WrapValue{value_descriptor_}(it_->second));
        }

        void Advance() override { ++it_; }

       private:
        BaseMessageDescriptor const& entry_descriptor_;
        ValueDescriptor const& value_descriptor_;
        typename MapType<EntryMessage>::iterator it_;
      };

      explicit Impl(MapType<EntryMessage>* const map, BaseMessageDescriptor const& entry_descriptor,
                    ValueDescriptor const& value_descriptor)
          : map_(map), entry_descriptor_(entry_descriptor), value_descriptor_(value_descriptor) {}

      bool IsOrdered() const override { return false; }
      BaseMessageDescriptor const& GetEntryDescriptor() const override { return entry_descriptor_; }

      size_t GetSize() const override { return map_->size(); }
      bool IsEmpty() const override { return map_->empty(); }

      std::shared_ptr<BaseImpl::Iterator> MakeBeginIterator() const override {
        return std::make_shared<Iterator>(entry_descriptor_, value_descriptor_, map_->begin());
      }

      std::shared_ptr<BaseImpl::Iterator> MakeEndIterator() const override {
        return std::make_shared<Iterator>(entry_descriptor_, value_descriptor_, map_->end());
      }

      void Clear() override { map_->clear(); }
      void Reserve(size_t const size) override { CapacityReserver{map_}(size); }

      absl::StatusOr<bool> Contains(MapKey const& key) const override {
        DEFINE_CONST_OR_RETURN(raw_key,
                               std::visit(UnwrapKey<internal::MapKeyType<EntryMessage>>{}, key));
        // TODO: switch to `contains` in C++20.
        return map_->count(raw_key) != 0;
      }

      absl::StatusOr<std::shared_ptr<BaseImpl::Iterator>> Find(MapKey const& key) const override {
        DEFINE_CONST_OR_RETURN(raw_key,
                               std::visit(UnwrapKey<internal::MapKeyType<EntryMessage>>{}, key));
        return std::make_shared<Iterator>(entry_descriptor_, value_descriptor_,
                                          map_->find(raw_key));
      }

      absl::StatusOr<bool> Erase(MapKey const& key) override {
        DEFINE_CONST_OR_RETURN(raw_key,
                               std::visit(UnwrapKey<internal::MapKeyType<EntryMessage>>{}, key));
        auto const it = map_->find(raw_key);
        if (it != map_->end()) {
          map_->erase(raw_key);
          return true;
        } else {
          return false;
        }
      }

     private:
      // Calls `reserve` for map types supporting that operations, otherwise it's a no-op.
      template <typename Map = MapType<EntryMessage>, typename Enable = void>
      class CapacityReserver {
       public:
        explicit CapacityReserver(Map* const map) : map_(map) {}

        void operator()(size_t const size) const {}

       private:
        Map* const map_;
      };

      template <typename Map>
      class CapacityReserver<Map, std::void_t<decltype(&Map::reserve)>> {
       public:
        explicit CapacityReserver(Map* const map) : map_(map) {}

        void operator()(size_t const size) const { map_->reserve(size); }

       private:
        Map* const map_;
      };

      MapType<EntryMessage>* const map_;
      BaseMessageDescriptor const& entry_descriptor_;
      ValueDescriptor const& value_descriptor_;
    };

    explicit Map(std::shared_ptr<BaseImpl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<BaseImpl> impl_;
  };

  using OneofFieldValue = std::variant<int32_t*, uint32_t*, int64_t*, uint64_t*, bool*,
                                       std::string*, std::vector<uint8_t>*, double*, float*,
                                       absl::Time*, absl::Duration*, RawEnum, RawSubMessage>;

  using ConstOneofFieldValue =
      std::variant<int32_t const*, uint32_t const*, int64_t const*, uint64_t const*, bool const*,
                   std::string const*, std::vector<uint8_t> const*, double const*, float const*,
                   absl::Time const*, absl::Duration const*, RawEnum const, RawSubMessage const>;

  class OneOf final {
   public:
    using SetValueArg =
        std::variant<int32_t, uint32_t, int64_t, uint64_t, bool, std::string, std::vector<uint8_t>,
                     double, float, absl::Time, absl::Duration>;

    template <typename Variant, typename Descriptors>
    explicit OneOf(Variant* const variant, Descriptors&& descriptors)
        : impl_(new Impl(variant, std::forward<Descriptors>(descriptors))) {}

    ~OneOf() = default;

    OneOf(OneOf const&) = default;
    OneOf& operator=(OneOf const&) = default;

    OneOf(OneOf&&) noexcept = default;
    OneOf& operator=(OneOf&&) noexcept = default;

    // Returns the number of types in this variant, including the leading monostate.
    size_t size() const { return impl_->GetSize(); }

    // Returns the index of the currently held variant. 0 means the currently held variant is the
    // leading monostate, meaning the variant is empty.
    size_t index() const { return impl_->GetIndex(); }

    absl::StatusOr<FieldType> GetTypeAt(size_t const index) const {
      return impl_->GetTypeAt(index);
    }

    std::optional<FieldType> GetType() const { return impl_->GetType(); }
    std::optional<OneofFieldValue> GetValue() { return impl_->GetValue(); }
    std::optional<ConstOneofFieldValue> GetValue() const { return impl_->GetConstValue(); }
    std::optional<ConstOneofFieldValue> GetConstValue() const { return impl_->GetConstValue(); }

    absl::Status SetValue(size_t const index, SetValueArg value) {
      return impl_->SetValue(index, std::move(value));
    }

    // TODO: add setters for enums and sub-messages.

    void Clear() { impl_->Clear(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual size_t GetSize() const = 0;
      virtual size_t GetIndex() const = 0;

      virtual absl::StatusOr<FieldType> GetTypeAt(size_t index) const = 0;
      virtual std::optional<FieldType> GetType() const = 0;
      virtual std::optional<OneofFieldValue> GetValue() = 0;
      virtual std::optional<ConstOneofFieldValue> GetConstValue() const = 0;

      virtual absl::Status SetValue(size_t index, SetValueArg&& value) = 0;

      virtual void Clear() = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename Variant, typename Descriptors, typename Enable = void>
    class Impl;

    template <typename... Types, typename... Descriptors>
    class Impl<std::variant<std::monostate, Types...>, std::tuple<Descriptors...>,
               std::enable_if_t<internal::CheckDescriptorsForTypesV<
                   internal::OneofTypes<Types...>, internal::OneofDescriptors<Descriptors...>>>>
        final : public BaseImpl {
     public:
      using Variant = std::variant<std::monostate, Types...>;

      explicit Impl(Variant* const variant, std::tuple<Descriptors...> descriptors)
          : variant_(variant),
            descriptors_(std::tuple_cat(std::tuple<std::monostate>(), std::move(descriptors))) {}

      size_t GetSize() const override { return sizeof...(Types) + 1; }
      size_t GetIndex() const override { return variant_->index(); }

      absl::StatusOr<FieldType> GetTypeAt(size_t const index) const override {
        if (index > 0 && index < sizeof...(Types)) {
          return std::visit(OneOfFieldTypeSelector(), *variant_);
        } else {
          return absl::OutOfRangeError("invalid oneof variant index");
        }
      }

      std::optional<FieldType> GetType() const override {
        if (variant_->index() > 0) {
          return std::visit(OneOfFieldTypeSelector(), *variant_);
        } else {
          return std::nullopt;
        }
      }

      std::optional<OneofFieldValue> GetValue() override {
        if (variant_->index() > 0) {
          return GetValueImpl(std::make_index_sequence<sizeof...(Types) + 1>());
        } else {
          return std::nullopt;
        }
      }

      std::optional<ConstOneofFieldValue> GetConstValue() const override {
        if (variant_->index() > 0) {
          return GetConstValueImpl();
        } else {
          return std::nullopt;
        }
      }

      absl::Status SetValue(size_t const index, SetValueArg&& value) override {
        return SetValueImpl(index, std::move(value),
                            std::make_index_sequence<sizeof...(Types) + 1>());
      }

      void Clear() override { variant_->template emplace<0>(); }

     private:
      template <size_t index, typename Type, typename Enable = void>
      struct ValueSetter {
        absl::Status operator()(Variant* const variant, SetValueArg&& value) const {
          variant->template emplace<index>(std::get<Type>(std::move(value)));
          return absl::OkStatus();
        }
      };

      template <size_t index>
      struct ValueSetter<index, std::monostate> {
        absl::Status operator()(Variant* const variant, SetValueArg&& value) const {
          return absl::OutOfRangeError("invalid oneof variant index");
        }
      };

      template <size_t index, typename Enum>
      struct ValueSetter<index, Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
        absl::Status operator()(Variant* const variant, SetValueArg&& value) const {
          return absl::InvalidArgumentError("use `SetEnumValue()` for enums");
        }
      };

      template <size_t index, typename Message>
      struct ValueSetter<index, Message,
                         std::enable_if_t<std::is_base_of_v<tsdb2::proto::Message, Message>>> {
        absl::Status operator()(Variant* const variant, SetValueArg&& value) const {
          return absl::InvalidArgumentError("use `SetSubMessage()` for sub-messages");
        }
      };

      template <size_t... Is>
      std::optional<OneofFieldValue> GetValueImpl(std::index_sequence<Is...> /*unused*/) const {
        size_t const index = variant_->index();
        std::optional<OneofFieldValue> result;
        (void)((index == Is && (result = OneOfFieldValueSelector{}(std::get<Is>(*variant_),
                                                                   std::get<Is>(descriptors_)),
                                true)) ||
               ...);
        return result;
      }

      std::optional<ConstOneofFieldValue> GetConstValueImpl() const {
        auto result = GetValueImpl(std::make_index_sequence<sizeof...(Types) + 1>());
        if (result.has_value()) {
          return std::visit(MakeConstOneOfFieldValue(), std::move(result).value());
        } else {
          return std::nullopt;
        }
      }

      template <size_t... Is>
      absl::Status SetValueImpl(size_t const target_index, SetValueArg&& value,
                                std::index_sequence<Is...> /*unused*/) const {
        if (target_index > sizeof...(Types)) {
          return absl::OutOfRangeError("invalid oneof variant index");
        }
        absl::Status result = absl::OkStatus();
        (void)((target_index == Is &&
                (result.Update(ValueSetter<Is, std::variant_alternative_t<Is, Variant>>{}(
                     variant_, std::move(value))),
                 true)) ||
               ...);
        return result;
      }

      Variant* const variant_;
      std::tuple<std::monostate, Descriptors...> const descriptors_;
    };

    template <typename... Types, typename... Descriptors>
    explicit Impl(std::variant<std::monostate, Types...>*, std::tuple<Descriptors...>)
        -> Impl<std::variant<std::monostate, Types...>, std::tuple<Descriptors...>>;

    std::shared_ptr<BaseImpl> impl_;
  };

  using FieldValue = std::variant<
      int32_t*, std::optional<int32_t>*, std::vector<int32_t>*, uint32_t*, std::optional<uint32_t>*,
      std::vector<uint32_t>*, int64_t*, std::optional<int64_t>*, std::vector<int64_t>*, uint64_t*,
      std::optional<uint64_t>*, std::vector<uint64_t>*, bool*, std::optional<bool>*,
      std::vector<bool>*, std::string*, std::optional<std::string>*, std::vector<std::string>*,
      std::vector<uint8_t>*, std::optional<std::vector<uint8_t>>*,
      std::vector<std::vector<uint8_t>>*, double*, std::optional<double>*, std::vector<double>*,
      float*, std::optional<float>*, std::vector<float>*, absl::Time*, std::optional<absl::Time>*,
      std::vector<absl::Time>*, absl::Duration*, std::optional<absl::Duration>*,
      std::vector<absl::Duration>*, RawEnum, OptionalEnum, RepeatedEnum, RawSubMessage,
      OptionalSubMessage, RepeatedSubMessage, Map, OneOf>;

  using ConstFieldValue = std::variant<
      int32_t const*, std::optional<int32_t> const*, std::vector<int32_t> const*, uint32_t const*,
      std::optional<uint32_t> const*, std::vector<uint32_t> const*, int64_t const*,
      std::optional<int64_t> const*, std::vector<int64_t> const*, uint64_t const*,
      std::optional<uint64_t> const*, std::vector<uint64_t> const*, bool const*,
      std::optional<bool> const*, std::vector<bool> const*, std::string const*,
      std::optional<std::string> const*, std::vector<std::string> const*,
      std::vector<uint8_t> const*, std::optional<std::vector<uint8_t>> const*,
      std::vector<std::vector<uint8_t>> const*, double const*, std::optional<double> const*,
      std::vector<double> const*, float const*, std::optional<float> const*,
      std::vector<float> const*, absl::Time const*, std::optional<absl::Time> const*,
      std::vector<absl::Time> const*, absl::Duration const*, std::optional<absl::Duration> const*,
      std::vector<absl::Duration> const*, RawEnum const, OptionalEnum const, RepeatedEnum const,
      RawSubMessage const, OptionalSubMessage const, RepeatedSubMessage const, Map const,
      OneOf const>;

  explicit constexpr BaseMessageDescriptor() = default;
  virtual ~BaseMessageDescriptor() = default;

  // Returns the list of field names of the described message.
  virtual absl::Span<std::string_view const> GetAllFieldNames() const = 0;

  // Returns the list of field names of the described message.
  virtual absl::Span<std::string_view const> GetRequiredFieldNames() const = 0;

  // Returns the `LabeledFieldType` of a field from its name.
  virtual absl::StatusOr<LabeledFieldType> GetLabeledFieldType(
      std::string_view field_name) const = 0;

  // Returns the type and kind of a field from its name.
  absl::StatusOr<std::pair<FieldType, FieldKind>> GetFieldTypeAndKind(
      std::string_view const field_name) const {
    DEFINE_CONST_OR_RETURN(labeled_type, GetLabeledFieldType(field_name));
    switch (labeled_type) {
      case LabeledFieldType::kMapField:
        return std::make_pair(FieldType::kMapField, FieldKind::kMap);
      case LabeledFieldType::kOneOfField:
        return std::make_pair(FieldType::kOneOfField, FieldKind::kOneOf);
      default: {
        auto const index = tsdb2::util::to_underlying(labeled_type);
        return std::make_pair(static_cast<FieldType>(index / 3), static_cast<FieldKind>(index % 3));
      }
    }
  }

  // Returns the type of a field from its name.
  absl::StatusOr<FieldType> GetFieldType(std::string_view const field_name) const {
    DEFINE_CONST_OR_RETURN(type_and_kind, GetFieldTypeAndKind(field_name));
    return type_and_kind.first;
  }

  // Returns the kind of a field from its name.
  absl::StatusOr<FieldKind> GetFieldKind(std::string_view const field_name) const {
    DEFINE_CONST_OR_RETURN(type_and_kind, GetFieldTypeAndKind(field_name));
    return type_and_kind.second;
  }

  // Creates a new (default-initialized) instance of the described message.
  virtual std::unique_ptr<Message> CreateInstance() const = 0;

  // Returns the descriptor of the specified field if it's an enum, or an error status otherwise.
  virtual absl::StatusOr<BaseEnumDescriptor const*> GetEnumFieldDescriptor(
      std::string_view field_name) const = 0;

  // Returns the descriptor of the specified field if it's a sub-message, or an error status
  // otherwise.
  virtual absl::StatusOr<BaseMessageDescriptor const*> GetSubMessageFieldDescriptor(
      std::string_view field_name) const = 0;

  // Returns a const pointer to the value of a field from its name. The returned type is an
  // `std::variant` that wraps all possible types. Sub-message types are further wrapped in a proxy
  // object (see `RawSubMessage`, `OptionalSubMessage`, and `RepeatedSubMessage`) allowing access to
  // the field and to the `BaseMessageDescriptor` of its type.
  virtual absl::StatusOr<ConstFieldValue> GetConstFieldValue(Message const& message,
                                                             std::string_view field_name) const = 0;

  // Returns a pointer to a (mutable) field from its name. The returned type is an `std::variant`
  // that wraps all possible types. Sub-message types are further wrapped in a proxy object (see
  // `RawSubMessage`, `OptionalSubMessage`, and `RepeatedSubMessage`) allowing access to the field
  // and to the `BaseMessageDescriptor` of its type.
  virtual absl::StatusOr<FieldValue> GetFieldValue(Message* message,
                                                   std::string_view field_name) const = 0;

 private:
  struct OneOfFieldTypeSelector {
    // NOLINTBEGIN(readability-named-parameter)

    FieldType operator()(std::monostate) const {
      // This return value is bogus, we should never get here because empty oneofs are handled by
      // the caller.
      return FieldType::kOneOfField;
    }

    FieldType operator()(int32_t) const { return FieldType::kInt32Field; }
    FieldType operator()(uint32_t) const { return FieldType::kUInt32Field; }
    FieldType operator()(int64_t) const { return FieldType::kInt64Field; }
    FieldType operator()(uint64_t) const { return FieldType::kUInt64Field; }
    FieldType operator()(bool) const { return FieldType::kBoolField; }
    FieldType operator()(std::string_view) const { return FieldType::kStringField; }
    FieldType operator()(absl::Span<uint8_t const>) const { return FieldType::kBytesField; }
    FieldType operator()(double) const { return FieldType::kDoubleField; }
    FieldType operator()(float) const { return FieldType::kFloatField; }
    FieldType operator()(absl::Time) const { return FieldType::kTimeField; }
    FieldType operator()(absl::Duration) const { return FieldType::kDurationField; }

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    FieldType operator()(Enum) const {
      return FieldType::kEnumField;
    }

    FieldType operator()(tsdb2::proto::Message const&) const { return FieldType::kSubMessageField; }

    // NOLINTEND(readability-named-parameter)
  };

  struct OneOfFieldValueSelector {
    std::optional<OneofFieldValue> operator()(std::monostate /*unused*/,
                                              std::monostate /*unused*/) const {
      return std::nullopt;
    }

    std::optional<OneofFieldValue> operator()(int32_t& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<int32_t*>, &value);
    }

    std::optional<OneofFieldValue> operator()(uint32_t& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<uint32_t*>, &value);
    }

    std::optional<OneofFieldValue> operator()(int64_t& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<int64_t*>, &value);
    }

    std::optional<OneofFieldValue> operator()(uint64_t& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<uint64_t*>, &value);
    }

    std::optional<OneofFieldValue> operator()(bool& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<bool*>, &value);
    }

    std::optional<OneofFieldValue> operator()(std::string& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<std::string*>, &value);
    }

    std::optional<OneofFieldValue> operator()(std::vector<uint8_t>& value,
                                              std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<std::vector<uint8_t>*>, &value);
    }

    std::optional<OneofFieldValue> operator()(double& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<double*>, &value);
    }

    std::optional<OneofFieldValue> operator()(float& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<float*>, &value);
    }

    std::optional<OneofFieldValue> operator()(absl::Time& value, std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<absl::Time*>, &value);
    }

    std::optional<OneofFieldValue> operator()(absl::Duration& value,
                                              std::monostate /*unused*/) const {
      return OneofFieldValue(std::in_place_type<absl::Duration*>, &value);
    }

    template <typename Enum, typename Descriptor,
              std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    std::optional<OneofFieldValue> operator()(Enum& value, Descriptor const& descriptor) const {
      return OneofFieldValue(std::in_place_type<RawEnum>, RawEnum(&value, descriptor));
    }

    template <typename Descriptor>
    std::optional<OneofFieldValue> operator()(Message& value, Descriptor const& descriptor) const {
      return OneofFieldValue(std::in_place_type<RawSubMessage>, RawSubMessage(&value, descriptor));
    }
  };

  struct MakeConstOneOfFieldValue {
    std::optional<ConstOneofFieldValue> operator()(int32_t* const value) const {
      return ConstOneofFieldValue(std::in_place_type<int32_t const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(uint32_t* const value) const {
      return ConstOneofFieldValue(std::in_place_type<uint32_t const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(int64_t* const value) const {
      return ConstOneofFieldValue(std::in_place_type<int64_t const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(uint64_t* const value) const {
      return ConstOneofFieldValue(std::in_place_type<uint64_t const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(bool* const value) const {
      return ConstOneofFieldValue(std::in_place_type<bool const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(std::string* const value) const {
      return ConstOneofFieldValue(std::in_place_type<std::string const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(std::vector<uint8_t>* const value) const {
      return ConstOneofFieldValue(std::in_place_type<std::vector<uint8_t> const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(double* const value) const {
      return ConstOneofFieldValue(std::in_place_type<double const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(float* const value) const {
      return ConstOneofFieldValue(std::in_place_type<float const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(absl::Time* const value) const {
      return ConstOneofFieldValue(std::in_place_type<absl::Time const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(absl::Duration* const value) const {
      return ConstOneofFieldValue(std::in_place_type<absl::Duration const*>, value);
    }

    std::optional<ConstOneofFieldValue> operator()(RawEnum&& value) const {
      return ConstOneofFieldValue(std::in_place_type<RawEnum const>, std::move(value));
    }

    std::optional<ConstOneofFieldValue> operator()(RawSubMessage&& value) const {
      return ConstOneofFieldValue(std::in_place_type<RawSubMessage const>, std::move(value));
    }
  };

  BaseMessageDescriptor(BaseMessageDescriptor const&) = delete;
  BaseMessageDescriptor& operator=(BaseMessageDescriptor const&) = delete;
  BaseMessageDescriptor(BaseMessageDescriptor&&) = delete;
  BaseMessageDescriptor& operator=(BaseMessageDescriptor&&) = delete;
};

template <typename Message>
struct FieldTypes {
  using RawInt32Field = int32_t Message::*;
  using OptionalInt32Field = std::optional<int32_t> Message::*;
  using RepeatedInt32Field = std::vector<int32_t> Message::*;

  using RawUInt32Field = uint32_t Message::*;
  using OptionalUInt32Field = std::optional<uint32_t> Message::*;
  using RepeatedUInt32Field = std::vector<uint32_t> Message::*;

  using RawInt64Field = int64_t Message::*;
  using OptionalInt64Field = std::optional<int64_t> Message::*;
  using RepeatedInt64Field = std::vector<int64_t> Message::*;

  using RawUInt64Field = uint64_t Message::*;
  using OptionalUInt64Field = std::optional<uint64_t> Message::*;
  using RepeatedUInt64Field = std::vector<uint64_t> Message::*;

  using RawBoolField = bool Message::*;
  using OptionalBoolField = std::optional<bool> Message::*;
  using RepeatedBoolField = std::vector<bool> Message::*;

  using RawStringField = std::string Message::*;
  using OptionalStringField = std::optional<std::string> Message::*;
  using RepeatedStringField = std::vector<std::string> Message::*;

  using RawBytesField = std::vector<uint8_t> Message::*;
  using OptionalBytesField = std::optional<std::vector<uint8_t>> Message::*;
  using RepeatedBytesField = std::vector<std::vector<uint8_t>> Message::*;

  using RawDoubleField = double Message::*;
  using OptionalDoubleField = std::optional<double> Message::*;
  using RepeatedDoubleField = std::vector<double> Message::*;

  using RawFloatField = float Message::*;
  using OptionalFloatField = std::optional<float> Message::*;
  using RepeatedFloatField = std::vector<float> Message::*;

  using RawTimeField = absl::Time Message::*;
  using OptionalTimeField = std::optional<absl::Time> Message::*;
  using RepeatedTimeField = std::vector<absl::Time> Message::*;

  using RawDurationField = absl::Duration Message::*;
  using OptionalDurationField = std::optional<absl::Duration> Message::*;
  using RepeatedDurationField = std::vector<absl::Duration> Message::*;

  class RawEnumField final {
   public:
    template <typename Enum, typename Descriptor>
    explicit RawEnumField(Enum Message::*const field, Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(field, descriptor)) {}

    ~RawEnumField() = default;

    RawEnumField(RawEnumField const&) = default;
    RawEnumField& operator=(RawEnumField const&) = default;

    RawEnumField(RawEnumField&&) noexcept = default;
    RawEnumField& operator=(RawEnumField&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::RawEnum MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::RawEnum MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(Enum Message::*const field, Descriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RawEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RawEnum(&(parent->*field_), descriptor_);
      }

     private:
      Enum Message::*const field_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class OptionalEnumField final {
   public:
    template <typename Enum, typename Descriptor>
    explicit OptionalEnumField(std::optional<Enum> Message::*const field,
                               Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(field, descriptor)) {}

    ~OptionalEnumField() = default;

    OptionalEnumField(OptionalEnumField const&) = default;
    OptionalEnumField& operator=(OptionalEnumField const&) = default;

    OptionalEnumField(OptionalEnumField&&) noexcept = default;
    OptionalEnumField& operator=(OptionalEnumField&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::OptionalEnum MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::OptionalEnum MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<Enum> Message::*const field, Descriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::OptionalEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OptionalEnum(&(parent->*field_), descriptor_);
      }

     private:
      std::optional<Enum> Message::*const field_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class RepeatedEnumField final {
   public:
    template <typename Enum, typename Descriptor>
    explicit RepeatedEnumField(std::vector<Enum> Message::*const field,
                               Descriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum, Descriptor>>(field, descriptor)) {}

    ~RepeatedEnumField() = default;

    RepeatedEnumField(RepeatedEnumField const&) = default;
    RepeatedEnumField& operator=(RepeatedEnumField const&) = default;

    RepeatedEnumField(RepeatedEnumField&&) noexcept = default;
    RepeatedEnumField& operator=(RepeatedEnumField&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::RepeatedEnum MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::RepeatedEnum MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <
        typename Enum, typename Descriptor,
        std::enable_if_t<std::is_enum_v<Enum> && std::is_base_of_v<BaseEnumDescriptor, Descriptor>,
                         bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<Enum> Message::*const field, Descriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      Descriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RepeatedEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RepeatedEnum(&(parent->*field_), descriptor_);
      }

     private:
      std::vector<Enum> Message::*const field_;
      Descriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class RawSubMessageField final {
   public:
    template <typename SubMessage>
    explicit RawSubMessageField(SubMessage Message::*const message,
                                BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<SubMessage>>(message, descriptor)) {}

    ~RawSubMessageField() = default;

    RawSubMessageField(RawSubMessageField const&) = default;
    RawSubMessageField& operator=(RawSubMessageField const&) = default;

    RawSubMessageField(RawSubMessageField&&) noexcept = default;
    RawSubMessageField& operator=(RawSubMessageField&&) noexcept = default;

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::RawSubMessage MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::RawSubMessage MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename SubMessage>
    class Impl : public BaseImpl {
     public:
      explicit Impl(SubMessage Message::*const message, BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RawSubMessage MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RawSubMessage(&(parent->*message_), descriptor_);
      }

     private:
      SubMessage Message::*const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class OptionalSubMessageField final {
   public:
    template <typename SubMessage>
    explicit OptionalSubMessageField(std::optional<SubMessage> Message::*const message,
                                     BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<OptionalImpl<SubMessage>>(message, descriptor)) {}

    template <typename SubMessage>
    explicit OptionalSubMessageField(std::unique_ptr<SubMessage> Message::*const message,
                                     BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<UniqueImpl<SubMessage>>(message, descriptor)) {}

    template <typename SubMessage>
    explicit OptionalSubMessageField(std::shared_ptr<SubMessage> Message::*const message,
                                     BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<SharedImpl<SubMessage>>(message, descriptor)) {}

    ~OptionalSubMessageField() = default;

    OptionalSubMessageField(OptionalSubMessageField const&) = default;
    OptionalSubMessageField& operator=(OptionalSubMessageField const&) = default;

    OptionalSubMessageField(OptionalSubMessageField&&) noexcept = default;
    OptionalSubMessageField& operator=(OptionalSubMessageField&&) noexcept = default;

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::OptionalSubMessage MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::OptionalSubMessage MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename SubMessage>
    class OptionalImpl : public BaseImpl {
     public:
      explicit OptionalImpl(std::optional<SubMessage> Message::*const message,
                            BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::OptionalSubMessage MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OptionalSubMessage(&(parent->*message_), descriptor_);
      }

     private:
      std::optional<SubMessage> Message::*const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    template <typename SubMessage>
    class UniqueImpl : public BaseImpl {
     public:
      explicit UniqueImpl(std::unique_ptr<SubMessage> Message::*const message,
                          BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::OptionalSubMessage MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OptionalSubMessage(&(parent->*message_), descriptor_);
      }

     private:
      std::unique_ptr<SubMessage> Message::*const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    template <typename SubMessage>
    class SharedImpl : public BaseImpl {
     public:
      explicit SharedImpl(std::shared_ptr<SubMessage> Message::*const message,
                          BaseMessageDescriptor const& descriptor)
          : message_(message), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::OptionalSubMessage MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OptionalSubMessage(&(parent->*message_), descriptor_);
      }

     private:
      std::shared_ptr<SubMessage> Message::*const message_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class RepeatedSubMessageField final {
   public:
    template <typename SubMessage>
    explicit RepeatedSubMessageField(std::vector<SubMessage> Message::*const message,
                                     BaseMessageDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<SubMessage>>(message, descriptor)) {}

    ~RepeatedSubMessageField() = default;

    RepeatedSubMessageField(RepeatedSubMessageField const&) = default;
    RepeatedSubMessageField& operator=(RepeatedSubMessageField const&) = default;

    RepeatedSubMessageField(RepeatedSubMessageField&&) noexcept = default;
    RepeatedSubMessageField& operator=(RepeatedSubMessageField&&) noexcept = default;

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    BaseMessageDescriptor::RepeatedSubMessage MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;
      virtual BaseMessageDescriptor::RepeatedSubMessage MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename SubMessage>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<SubMessage> Message::*const messages,
                    BaseMessageDescriptor const& descriptor)
          : messages_(messages), descriptor_(descriptor) {}

      BaseMessageDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RepeatedSubMessage MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RepeatedSubMessage(&(parent->*messages_), descriptor_);
      }

     private:
      std::vector<SubMessage> Message::*const messages_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class MapField final {
   public:
    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromStdMap(internal::StdMap<EntryMessage> Message::*const map,
                               BaseMessageDescriptor const& entry_descriptor,
                               ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::StdMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromStdUnorderedMap(internal::StdUnorderedMap<EntryMessage> Message::*const map,
                                        BaseMessageDescriptor const& entry_descriptor,
                                        ValueDescriptor const& value_descriptor) {
      return MapField(
          std::make_shared<Impl<internal::StdUnorderedMap, EntryMessage, ValueDescriptor>>(
              map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromFlatHashMap(internal::FlatHashMap<EntryMessage> Message::*const map,
                                    BaseMessageDescriptor const& entry_descriptor,
                                    ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::FlatHashMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromNodeHashMap(internal::NodeHashMap<EntryMessage> Message::*const map,
                                    BaseMessageDescriptor const& entry_descriptor,
                                    ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::NodeHashMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromBtreeMap(internal::BtreeMap<EntryMessage> Message::*const map,
                                 BaseMessageDescriptor const& entry_descriptor,
                                 ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::BtreeMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromFlatMap(internal::FlatMap<EntryMessage> Message::*const map,
                                BaseMessageDescriptor const& entry_descriptor,
                                ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::FlatMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    template <typename EntryMessage, typename ValueDescriptor>
    static MapField FromTrieMap(internal::TrieMap<EntryMessage> Message::*const map,
                                BaseMessageDescriptor const& entry_descriptor,
                                ValueDescriptor const& value_descriptor) {
      return MapField(std::make_shared<Impl<internal::TrieMap, EntryMessage, ValueDescriptor>>(
          map, entry_descriptor, value_descriptor));
    }

    ~MapField() = default;

    MapField(MapField const&) = default;
    MapField& operator=(MapField const&) = default;

    MapField(MapField&&) noexcept = default;
    MapField& operator=(MapField&&) noexcept = default;

    BaseMessageDescriptor const& entry_descriptor() const { return impl_->GetEntryDescriptor(); }

    BaseMessageDescriptor::Map MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetEntryDescriptor() const = 0;

      virtual BaseMessageDescriptor::Map MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <template <typename> class MapType, typename EntryMessage, typename ValueDescriptor,
              std::enable_if_t<internal::IsMapEntryV<EntryMessage> &&
                                   internal::IsDescriptorForTypeV<
                                       internal::MapValueType<EntryMessage>, ValueDescriptor>,
                               bool> = true>
    class Impl final : public BaseImpl {
     public:
      explicit Impl(MapType<EntryMessage> Message::*const map,
                    BaseMessageDescriptor const& entry_descriptor,
                    ValueDescriptor const& value_descriptor)
          : map_(map), entry_descriptor_(entry_descriptor), value_descriptor_(value_descriptor) {}

      BaseMessageDescriptor const& GetEntryDescriptor() const override { return entry_descriptor_; }

      BaseMessageDescriptor::Map MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::Map::Create<MapType, EntryMessage, ValueDescriptor>(
            &(parent->*map_), entry_descriptor_, value_descriptor_);
      }

     private:
      MapType<EntryMessage> Message::*const map_;
      BaseMessageDescriptor const& entry_descriptor_;
      ValueDescriptor const& value_descriptor_;
    };

    explicit MapField(std::shared_ptr<BaseImpl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<BaseImpl> impl_;
  };

  class OneOfField final {
   public:
    template <typename Variant, typename Descriptors>
    explicit OneOfField(Variant Message::*const variant, Descriptors&& descriptors)
        : impl_(new Impl(variant, std::forward<Descriptors>(descriptors))) {}

    ~OneOfField() = default;

    OneOfField(OneOfField const&) = default;
    OneOfField& operator=(OneOfField const&) = default;

    OneOfField(OneOfField&&) noexcept = default;
    OneOfField& operator=(OneOfField&&) noexcept = default;

    BaseMessageDescriptor::OneOf MakeValue(Message* const parent) const {
      return impl_->MakeValue(parent);
    }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor::OneOf MakeValue(Message* parent) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename Variant, typename Descriptors>
    class Impl;

    template <typename... Types, typename... Descriptors>
    class Impl<std::variant<std::monostate, Types...>, std::tuple<Descriptors...>> final
        : public BaseImpl {
     public:
      explicit Impl(std::variant<std::monostate, Types...> Message::*const variant,
                    std::tuple<Descriptors...> descriptors)
          : variant_(variant), descriptors_(std::move(descriptors)) {}

      BaseMessageDescriptor::OneOf MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OneOf(&(parent->*variant_), descriptors_);
      }

     private:
      static_assert(internal::CheckDescriptorsForTypesV<internal::OneofTypes<Types...>,
                                                        internal::OneofDescriptors<Descriptors...>>,
                    "invalid type descriptors");

      std::variant<std::monostate, Types...> Message::*const variant_;
      std::tuple<Descriptors...> const descriptors_;
    };

    template <typename... Types, typename... Descriptors>
    explicit Impl(std::variant<std::monostate, Types...> Message::*, std::tuple<Descriptors...>)
        -> Impl<std::variant<std::monostate, Types...>, std::tuple<Descriptors...>>;

    std::shared_ptr<BaseImpl> impl_;
  };

  using FieldPointer = std::variant<
      RawInt32Field, OptionalInt32Field, RepeatedInt32Field, RawUInt32Field, OptionalUInt32Field,
      RepeatedUInt32Field, RawInt64Field, OptionalInt64Field, RepeatedInt64Field, RawUInt64Field,
      OptionalUInt64Field, RepeatedUInt64Field, RawBoolField, OptionalBoolField, RepeatedBoolField,
      RawStringField, OptionalStringField, RepeatedStringField, RawBytesField, OptionalBytesField,
      RepeatedBytesField, RawDoubleField, OptionalDoubleField, RepeatedDoubleField, RawFloatField,
      OptionalFloatField, RepeatedFloatField, RawTimeField, OptionalTimeField, RepeatedTimeField,
      RawDurationField, OptionalDurationField, RepeatedDurationField, RawEnumField,
      OptionalEnumField, RepeatedEnumField, RawSubMessageField, OptionalSubMessageField,
      RepeatedSubMessageField, MapField, OneOfField>;
};

template <typename Message>
using RawEnumField = typename FieldTypes<Message>::RawEnumField;

template <typename Message>
using OptionalEnumField = typename FieldTypes<Message>::OptionalEnumField;

template <typename Message>
using RepeatedEnumField = typename FieldTypes<Message>::RepeatedEnumField;

template <typename Message>
using RawSubMessageField = typename FieldTypes<Message>::RawSubMessageField;

template <typename Message>
using OptionalSubMessageField = typename FieldTypes<Message>::OptionalSubMessageField;

template <typename Message>
using RepeatedSubMessageField = typename FieldTypes<Message>::RepeatedSubMessageField;

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField StdMapField(
    internal::StdMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromStdMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField StdUnorderedMapField(
    internal::StdUnorderedMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromStdUnorderedMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField FlatHashMapField(
    internal::FlatHashMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromFlatHashMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField NodeHashMapField(
    internal::NodeHashMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromNodeHashMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField BtreeMapField(
    internal::BtreeMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromBtreeMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField FlatMapField(
    internal::FlatMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromFlatMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message, typename EntryMessage, typename ValueDescriptor>
inline typename FieldTypes<Message>::MapField TrieMapField(
    internal::TrieMap<EntryMessage> Message::*const field,
    BaseMessageDescriptor const& entry_descriptor, ValueDescriptor const& value_descriptor) {
  return FieldTypes<Message>::MapField::template FromTrieMap<EntryMessage, ValueDescriptor>(
      field, entry_descriptor, value_descriptor);
}

template <typename Message>
using OneOfField = typename FieldTypes<Message>::OneOfField;

// Allows implementing message descriptors, used for reflection features and TextFormat parsing.
template <typename Message, size_t num_fields, size_t num_required_fields>
class MessageDescriptor final : public BaseMessageDescriptor, public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::LabeledFieldType;
  using BaseMessageDescriptor::Map;
  using BaseMessageDescriptor::OneOf;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor(
      std::pair<std::string_view, FieldPointer> const (&fields)[num_fields])
      : field_ptrs_(tsdb2::common::fixed_flat_map_of(fields)), field_names_(MakeNameArray()) {}

  explicit constexpr MessageDescriptor(
      std::pair<std::string_view, FieldPointer> const (&fields)[num_fields],
      std::string_view const (&required_field_names)[num_required_fields])
      : field_ptrs_(tsdb2::common::fixed_flat_map_of(fields)),
        field_names_(MakeNameArray()),
        required_field_names_(tsdb2::common::to_array(required_field_names)) {}

  absl::Span<std::string_view const> GetAllFieldNames() const override { return field_names_; }

  absl::Span<std::string_view const> GetRequiredFieldNames() const override {
    return required_field_names_;
  }

  absl::StatusOr<LabeledFieldType> GetLabeledFieldType(
      std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it != field_ptrs_.end()) {
      return static_cast<LabeledFieldType>(it->second.index());
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
    }
  }

  std::unique_ptr<tsdb2::proto::Message> CreateInstance() const override {
    return std::make_unique<Message>();
  }

  absl::StatusOr<BaseEnumDescriptor const*> GetEnumFieldDescriptor(
      std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it == field_ptrs_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
    }
    return std::visit(ExtractEnumDescriptor(), it->second);
  }

  absl::StatusOr<BaseMessageDescriptor const*> GetSubMessageFieldDescriptor(
      std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it == field_ptrs_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
    }
    return std::visit(ExtractSubMessageDescriptor(), it->second);
  }

  absl::StatusOr<ConstFieldValue> GetConstFieldValue(
      tsdb2::proto::Message const& message, std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it == field_ptrs_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
    }
    return std::visit(ConstFieldPointerVisitor(*static_cast<Message const*>(&message)), it->second);
  }

  absl::StatusOr<FieldValue> GetFieldValue(tsdb2::proto::Message* const message,
                                           std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it == field_ptrs_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
    }
    return std::visit(FieldPointerVisitor(static_cast<Message*>(message)), it->second);
  }

 private:
  struct ExtractEnumDescriptor {
    absl::StatusOr<BaseEnumDescriptor const*> operator()(RawEnum const& field) const {
      return &(field.descriptor());
    }

    absl::StatusOr<BaseEnumDescriptor const*> operator()(OptionalEnum const& field) const {
      return &(field.descriptor());
    }

    absl::StatusOr<BaseEnumDescriptor const*> operator()(RepeatedEnum const& field) const {
      return &(field.descriptor());
    }

    template <typename Arg>
    absl::StatusOr<BaseEnumDescriptor const*> operator()(Arg&& arg) const {
      return absl::FailedPreconditionError("not an enum field");
    }
  };

  struct ExtractSubMessageDescriptor {
    absl::StatusOr<BaseMessageDescriptor const*> operator()(RawSubMessage const& field) const {
      return &(field.descriptor());
    }

    absl::StatusOr<BaseMessageDescriptor const*> operator()(OptionalSubMessage const& field) const {
      return &(field.descriptor());
    }

    absl::StatusOr<BaseMessageDescriptor const*> operator()(RepeatedSubMessage const& field) const {
      return &(field.descriptor());
    }

    template <typename Arg>
    absl::StatusOr<BaseMessageDescriptor const*> operator()(Arg&& arg) const {
      return absl::FailedPreconditionError("not a sub-message field");
    }
  };

  class ConstFieldPointerVisitor {
   public:
    explicit ConstFieldPointerVisitor(Message const& message) : message_(message) {}

    ConstFieldValue operator()(int32_t Message::*const value) const {
      return ConstFieldValue(std::in_place_type<int32_t const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<int32_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<int32_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<int32_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<int32_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(uint32_t Message::*const value) const {
      return ConstFieldValue(std::in_place_type<uint32_t const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<uint32_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<uint32_t> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<uint32_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<uint32_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(int64_t Message::*const value) const {
      return ConstFieldValue(std::in_place_type<int64_t const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<int64_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<int64_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<int64_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<int64_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(uint64_t Message::*const value) const {
      return ConstFieldValue(std::in_place_type<uint64_t const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<uint64_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<uint64_t> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<uint64_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<uint64_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(bool Message::*const value) const {
      return ConstFieldValue(std::in_place_type<bool const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<bool> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<bool> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<bool> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<bool> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::string Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::string const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<std::string> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<std::string> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<std::string> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<std::string> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<uint8_t> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<uint8_t> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<std::vector<uint8_t>> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<std::vector<uint8_t>> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<std::vector<uint8_t>> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<std::vector<uint8_t>> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(double Message::*const value) const {
      return ConstFieldValue(std::in_place_type<double const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<double> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<double> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<double> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<double> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(float Message::*const value) const {
      return ConstFieldValue(std::in_place_type<float const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<float> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<float> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<float> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<float> const*>, &(message_.*value));
    }

    ConstFieldValue operator()(absl::Time Message::*const value) const {
      return ConstFieldValue(std::in_place_type<absl::Time const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<absl::Time> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<absl::Time> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<absl::Time> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<absl::Time> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(absl::Duration Message::*const value) const {
      return ConstFieldValue(std::in_place_type<absl::Duration const*>, &(message_.*value));
    }

    ConstFieldValue operator()(std::optional<absl::Duration> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::optional<absl::Duration> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(std::vector<absl::Duration> Message::*const value) const {
      return ConstFieldValue(std::in_place_type<std::vector<absl::Duration> const*>,
                             &(message_.*value));
    }

    ConstFieldValue operator()(typename FieldTypes::RawEnumField const field) const {
      return ConstFieldValue(std::in_place_type<RawEnum const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::OptionalEnumField const field) const {
      return ConstFieldValue(std::in_place_type<OptionalEnum const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::RepeatedEnumField const field) const {
      return ConstFieldValue(std::in_place_type<RepeatedEnum const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::RawSubMessageField const field) const {
      return ConstFieldValue(std::in_place_type<RawSubMessage const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::OptionalSubMessageField const& field) const {
      return ConstFieldValue(std::in_place_type<OptionalSubMessage const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::RepeatedSubMessageField const& field) const {
      return ConstFieldValue(std::in_place_type<RepeatedSubMessage const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::MapField const& field) const {
      return ConstFieldValue(std::in_place_type<Map const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

    ConstFieldValue operator()(typename FieldTypes::OneOfField const& field) const {
      return ConstFieldValue(std::in_place_type<OneOf const>,
                             field.MakeValue(const_cast<Message*>(&message_)));
    }

   private:
    Message const& message_;
  };

  class FieldPointerVisitor {
   public:
    explicit FieldPointerVisitor(Message* const message) : message_(message) {}

    FieldValue operator()(int32_t Message::*const value) const {
      return FieldValue(std::in_place_type<int32_t*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<int32_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<int32_t>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<int32_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<int32_t>*>, &(message_->*value));
    }

    FieldValue operator()(uint32_t Message::*const value) const {
      return FieldValue(std::in_place_type<uint32_t*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<uint32_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<uint32_t>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<uint32_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<uint32_t>*>, &(message_->*value));
    }

    FieldValue operator()(int64_t Message::*const value) const {
      return FieldValue(std::in_place_type<int64_t*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<int64_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<int64_t>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<int64_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<int64_t>*>, &(message_->*value));
    }

    FieldValue operator()(uint64_t Message::*const value) const {
      return FieldValue(std::in_place_type<uint64_t*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<uint64_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<uint64_t>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<uint64_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<uint64_t>*>, &(message_->*value));
    }

    FieldValue operator()(bool Message::*const value) const {
      return FieldValue(std::in_place_type<bool*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<bool> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<bool>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<bool> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<bool>*>, &(message_->*value));
    }

    FieldValue operator()(std::string Message::*const value) const {
      return FieldValue(std::in_place_type<std::string*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<std::string> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<std::string>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<std::string> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<std::string>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<uint8_t> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<uint8_t>*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<std::vector<uint8_t>> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<std::vector<uint8_t>>*>,
                        &(message_->*value));
    }

    FieldValue operator()(std::vector<std::vector<uint8_t>> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<std::vector<uint8_t>>*>,
                        &(message_->*value));
    }

    FieldValue operator()(double Message::*const value) const {
      return FieldValue(std::in_place_type<double*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<double> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<double>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<double> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<double>*>, &(message_->*value));
    }

    FieldValue operator()(float Message::*const value) const {
      return FieldValue(std::in_place_type<float*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<float> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<float>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<float> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<float>*>, &(message_->*value));
    }

    FieldValue operator()(absl::Time Message::*const value) const {
      return FieldValue(std::in_place_type<absl::Time*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<absl::Time> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<absl::Time>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<absl::Time> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<absl::Time>*>, &(message_->*value));
    }

    FieldValue operator()(absl::Duration Message::*const value) const {
      return FieldValue(std::in_place_type<absl::Duration*>, &(message_->*value));
    }

    FieldValue operator()(std::optional<absl::Duration> Message::*const value) const {
      return FieldValue(std::in_place_type<std::optional<absl::Duration>*>, &(message_->*value));
    }

    FieldValue operator()(std::vector<absl::Duration> Message::*const value) const {
      return FieldValue(std::in_place_type<std::vector<absl::Duration>*>, &(message_->*value));
    }

    FieldValue operator()(typename FieldTypes::RawEnumField const field) const {
      return FieldValue(std::in_place_type<RawEnum>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::OptionalEnumField const field) const {
      return FieldValue(std::in_place_type<OptionalEnum>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::RepeatedEnumField const field) const {
      return FieldValue(std::in_place_type<RepeatedEnum>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::RawSubMessageField const field) const {
      return FieldValue(std::in_place_type<RawSubMessage>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::OptionalSubMessageField const& field) const {
      return FieldValue(std::in_place_type<OptionalSubMessage>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::RepeatedSubMessageField const& field) const {
      return FieldValue(std::in_place_type<RepeatedSubMessage>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::MapField const& field) const {
      return FieldValue(std::in_place_type<Map>, field.MakeValue(message_));
    }

    FieldValue operator()(typename FieldTypes::OneOfField const& field) const {
      return FieldValue(std::in_place_type<OneOf>, field.MakeValue(message_));
    }

   private:
    Message* const message_;
  };

  constexpr std::array<std::string_view, num_fields> MakeNameArray() const {
    std::array<std::string_view, num_fields> array;
    for (size_t i = 0; i < field_ptrs_.size(); ++i) {
      array[i] = field_ptrs_.rep()[i].first;
    }
    return array;
  }

  tsdb2::common::fixed_flat_map<std::string_view, FieldPointer, num_fields> const field_ptrs_;
  std::array<std::string_view, num_fields> const field_names_;
  std::array<std::string_view, num_required_fields> const required_field_names_;
};

// We need to specialize the version with 0 values because zero element arrays are not permitted in
// C++. The constructor of this specialization takes no arguments.
template <typename Message>
class MessageDescriptor<Message, 0, 0> final : public BaseMessageDescriptor,
                                               public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::LabeledFieldType;
  using BaseMessageDescriptor::Map;
  using BaseMessageDescriptor::OneOf;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor() = default;

  absl::Span<std::string_view const> GetAllFieldNames() const override { return {}; }

  absl::Span<std::string_view const> GetRequiredFieldNames() const override { return {}; }

  absl::StatusOr<LabeledFieldType> GetLabeledFieldType(
      std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }

  std::unique_ptr<tsdb2::proto::Message> CreateInstance() const override {
    return std::make_unique<Message>();
  }

  absl::StatusOr<BaseEnumDescriptor const*> GetEnumFieldDescriptor(
      std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }

  absl::StatusOr<BaseMessageDescriptor const*> GetSubMessageFieldDescriptor(
      std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }

  absl::StatusOr<ConstFieldValue> GetConstFieldValue(
      tsdb2::proto::Message const& message, std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }

  absl::StatusOr<FieldValue> GetFieldValue(tsdb2::proto::Message* const message,
                                           std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }
};

namespace internal {

template <typename Message, typename Descriptor>
struct IsDescriptorForType<Message, Descriptor,
                           std::enable_if_t<std::is_base_of_v<tsdb2::proto::Message, Message>>> {
  static inline bool constexpr value =
      std::is_base_of_v<BaseMessageDescriptor, std::decay_t<Descriptor>>;
};

}  // namespace internal

// Allows retrieving the descriptor of a proto enum without knowing its exact type. Can be used with
// templates, e.g.:
//
//   template <typename Enum>
//   void Foo(Enum const value) {
//     auto const& descriptor = tsdb2::proto::GetEnumDescriptor<Enum>();
//     for (auto const name : descriptor.GetValueNames()) {
//       // ...
//     }
//   }
//
template <typename Enum>
auto const& GetEnumDescriptor();

// Allows retrieving the descriptor of a proto message without knowing its exact type. Can be used
// with templates, e.g.:
//
//   template <typename Message>
//   void Foo(Message const& proto) {
//     auto const& descriptor = tsdb2::proto::GetMessageDescriptor<Message>();
//     for (auto const name : descriptor.GetAllFieldNames()) {
//       // ...
//     }
//   }
//
template <typename Message>
auto const& GetMessageDescriptor();

// Empty descriptor used as a placeholder in those contexts where a descriptor is required but the
// described value is neither an enum nor a proto message.
//
// We use `std::monostate` as its type because that's the same descriptor type we use in primitive
// variants of oneof fields.
extern std::monostate const kVoidDescriptor;

template <typename Type, typename Enable = void>
struct HasProtoReflection : public std::false_type {};

template <typename Type>
struct HasProtoReflection<
    Type, std::enable_if_t<IsProtoMessageV<Type> &&
                           std::is_base_of_v<BaseMessageDescriptor,
                                             std::decay_t<decltype(Type::MESSAGE_DESCRIPTOR)>>>>
    : public std::true_type {};

template <typename Type>
struct HasProtoReflection<
    Type, std::enable_if_t<std::is_enum_v<Type> &&
                           std::is_base_of_v<BaseEnumDescriptor,
                                             std::decay_t<decltype(GetEnumDescriptor<Type>())>>>>
    : public std::true_type {};

template <typename Type>
inline bool constexpr HasProtoReflectionV = HasProtoReflection<Type>::value;

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_REFLECTION_H__
