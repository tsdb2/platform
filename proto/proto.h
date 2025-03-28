#ifndef __TSDB2_PROTO_PROTO_H__
#define __TSDB2_PROTO_PROTO_H__

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace proto {

// Base class for all protobuf messages.
struct Message {};

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
// NOTE: the whole reflection API is NOT thread-safe. It's the user's responsibility to ensure
// proper synchronization. The same goes for the protobufs themselves.
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

}  // namespace internal

// Base class for all message descriptors (see the `MessageDescriptor` template below).
//
// You must not create instances of this class. Instances are already provided in every generated
// message type through the `MESSAGE_DESCRIPTOR` static property.
//
// NOTE: the whole reflection API is NOT thread-safe. It's the user's responsibility to ensure
// proper synchronization. The same goes for the protobufs themselves.
class BaseMessageDescriptor {
 public:
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
    kOneOfField = 13,
  };

  enum class FieldKind : int8_t {
    kRaw = 0,
    kOptional = 1,
    kRepeated = 2,
    kOneOf = 3,
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
    kOneOfField = 39,
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
        : impl_(std::make_shared<Impl<SubMessage>>(message, descriptor)) {}

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
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<SubMessage>* const message,
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

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about a message-typed optional field. This is essentially a pair of a pointer
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
    // leading monostate, meaning the variant is effectively empty.
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
      OptionalSubMessage, RepeatedSubMessage, OneOf>;

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
      RawSubMessage const, OptionalSubMessage const, RepeatedSubMessage const, OneOf const>;

  explicit constexpr BaseMessageDescriptor() = default;
  virtual ~BaseMessageDescriptor() = default;

  // Returns the list of field names of the described message.
  virtual absl::Span<std::string_view const> GetAllFieldNames() const = 0;

  // Returns the `LabeledFieldType` of a field from its name.
  virtual absl::StatusOr<LabeledFieldType> GetLabeledFieldType(
      std::string_view field_name) const = 0;

  // Returns the type and kind of a field from its name.
  absl::StatusOr<std::pair<FieldType, FieldKind>> GetFieldTypeAndKind(
      std::string_view const field_name) const {
    DEFINE_CONST_OR_RETURN(labeled_type, GetLabeledFieldType(field_name));
    if (labeled_type != LabeledFieldType::kOneOfField) {
      auto const index = tsdb2::util::to_underlying(labeled_type);
      return std::make_pair(static_cast<FieldType>(index / 3), static_cast<FieldKind>(index % 3));
    } else {
      return std::make_pair(FieldType::kOneOfField, FieldKind::kOneOf);
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
        : impl_(std::make_shared<Impl<SubMessage>>(message, descriptor)) {}

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
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<SubMessage> Message::*const message,
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
      RepeatedSubMessageField, OneOfField>;
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

template <typename Message>
using OneOfField = typename FieldTypes<Message>::OneOfField;

// Allows implementing message descriptors, used for reflection features and TextFormat parsing.
template <typename Message, size_t num_fields>
class MessageDescriptor final : public BaseMessageDescriptor, public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::LabeledFieldType;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor(
      std::pair<std::string_view, FieldPointer> const (&fields)[num_fields])
      : field_ptrs_(tsdb2::common::fixed_flat_map_of(fields)), field_names_(MakeNameArray()) {}

  absl::Span<std::string_view const> GetAllFieldNames() const override { return field_names_; }

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
};

// We need to specialize the version with 0 values because zero element arrays are not permitted in
// C++. The constructor of this specialization takes no arguments.
template <typename Message>
class MessageDescriptor<Message, 0> final : public BaseMessageDescriptor,
                                            public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::LabeledFieldType;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor() = default;

  absl::Span<std::string_view const> GetAllFieldNames() const override { return {}; }

  absl::StatusOr<LabeledFieldType> GetLabeledFieldType(
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

template <typename Value>
absl::StatusOr<Value const*> RequireField(std::optional<Value> const& field) {
  if (field.has_value()) {
    return absl::StatusOr<Value const*>(&field.value());
  } else {
    return absl::InvalidArgumentError("missing required field");
  }
}

#define REQUIRE_FIELD_OR_RETURN(name, object, field)                     \
  auto const& maybe_##name = (object).field;                             \
  if (!maybe_##name.has_value()) {                                       \
    return absl::InvalidArgumentError("missing required field " #field); \
  }                                                                      \
  auto const& name = maybe_##name.value();

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_PROTO_H__
