#ifndef __TSDB2_PROTO_PROTO_H__
#define __TSDB2_PROTO_PROTO_H__

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace proto {

// Base class for all protobuf messages.
struct Message {};

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
    if (it != values_by_name_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid enum value name: \"", absl::CEscape(name), "\""));
    }
    return it->second;
  }

  absl::StatusOr<std::string_view> GetNameForValue(int64_t const value) const override {
    auto const it = names_by_value_.find(value);
    if (it != names_by_value_.end()) {
      return absl::InvalidArgumentError(absl::StrCat("unknown enum value: ", value));
    }
    return it->second;
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

// Base class for all message descriptors (see the `MessageDescriptor` template below).
//
// You must not create instances of this class. Instances are already provided in every generated
// message type through the `MESSAGE_DESCRIPTOR` static property.
//
// NOTE: the whole reflection API is NOT thread-safe. It's the user's responsibility to ensure
// proper synchronization. The same goes for the protobufs themselves.
class BaseMessageDescriptor {
 public:
  enum class FieldType {
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
    kRawEnumField = 27,
    kOptionalEnumField = 28,
    kRepeatedEnumField = 29,
    kRawSubMessageField = 30,
    kOptionalSubMessageField = 31,
    kRepeatedSubMessageField = 32,
  };

  // Keeps information about an enum-typed field.
  class RawEnum final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit RawEnum(Enum* const field, BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(field, descriptor)) {}

    ~RawEnum() = default;

    RawEnum(RawEnum const&) = default;
    RawEnum& operator=(RawEnum const&) = default;

    RawEnum(RawEnum&&) noexcept = default;
    RawEnum& operator=(RawEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    int64_t value() const { return impl_->GetValue(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual int64_t GetValue() const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(Enum* const field, BaseEnumDescriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      int64_t GetValue() const override { return tsdb2::util::to_underlying(*field_); }

     private:
      Enum* const field_;
      BaseEnumDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about an optional enum-typed field.
  class OptionalEnum final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit OptionalEnum(std::optional<Enum>* const field, BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(field, descriptor)) {}

    ~OptionalEnum() = default;

    OptionalEnum(OptionalEnum const&) = default;
    OptionalEnum& operator=(OptionalEnum const&) = default;

    OptionalEnum(OptionalEnum&&) noexcept = default;
    OptionalEnum& operator=(OptionalEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    bool has_value() const { return impl_->HasValue(); }
    int64_t value() const { return impl_->GetValue(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;
      virtual bool HasValue() const = 0;
      virtual int64_t GetValue() const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<Enum>* const field, BaseEnumDescriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      bool HasValue() const override { return field_->has_value(); }
      int64_t GetValue() const override { return tsdb2::util::to_underlying(field_->value()); }

     private:
      std::optional<Enum>* const field_;
      BaseEnumDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  // Keeps information about a repeated enum-typed field.
  class RepeatedEnum final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit RepeatedEnum(std::vector<Enum>* const values, BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(values, descriptor)) {}

    ~RepeatedEnum() = default;

    RepeatedEnum(RepeatedEnum const&) = default;
    RepeatedEnum& operator=(RepeatedEnum const&) = default;

    RepeatedEnum(RepeatedEnum&&) noexcept = default;
    RepeatedEnum& operator=(RepeatedEnum&&) noexcept = default;

    BaseEnumDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

    size_t size() const { return impl_->GetSize(); }
    [[nodiscard]] bool empty() const { return impl_->GetSize() == 0; }

    int64_t operator[](size_t const index) const { return impl_->GetAt(index); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseEnumDescriptor const& GetDescriptor() const = 0;

      virtual size_t GetSize() const = 0;

      virtual int64_t GetAt(size_t index) const = 0;

     private:
      BaseImpl(BaseImpl const&) = delete;
      BaseImpl& operator=(BaseImpl const&) = delete;
      BaseImpl(BaseImpl&&) = delete;
      BaseImpl& operator=(BaseImpl&&) = delete;
    };

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<Enum>* const values, BaseEnumDescriptor const& descriptor)
          : values_(values), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      size_t GetSize() const override { return values_->size(); }

      int64_t GetAt(size_t const index) const override {
        return tsdb2::util::to_underlying(values_->operator[](index));
      }

     private:
      std::vector<Enum>* const values_;
      BaseEnumDescriptor const& descriptor_;
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

    BaseMessageDescriptor const& descriptor() const { return impl_->GetDescriptor(); }

   private:
    class BaseImpl {
     public:
      explicit BaseImpl() = default;
      virtual ~BaseImpl() = default;

      virtual BaseMessageDescriptor const& GetDescriptor() const = 0;
      virtual bool HasValue() const = 0;
      virtual Message const& GetValue() const = 0;
      virtual Message* GetMutableValue() const = 0;

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
      Message* GetMutableValue() const override { return &(message_->value()); }

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

      virtual Message* GetAt(size_t index) const = 0;

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

      Message* GetAt(size_t const index) const override { return &(messages_->at(index)); }

     private:
      std::vector<SubMessage>* const messages_;
      BaseMessageDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  using FieldValue = std::variant<
      int32_t*, std::optional<int32_t>*, std::vector<int32_t>*, uint32_t*, std::optional<uint32_t>*,
      std::vector<uint32_t>*, int64_t*, std::optional<int64_t>*, std::vector<int64_t>*, uint64_t*,
      std::optional<uint64_t>*, std::vector<uint64_t>*, bool*, std::optional<bool>*,
      std::vector<bool>*, std::string*, std::optional<std::string>*, std::vector<std::string>*,
      std::vector<uint8_t>*, std::optional<std::vector<uint8_t>>*,
      std::vector<std::vector<uint8_t>>*, double*, std::optional<double>*, std::vector<double>*,
      float*, std::optional<float>*, std::vector<float>*, RawEnum, OptionalEnum, RepeatedEnum,
      RawSubMessage, OptionalSubMessage, RepeatedSubMessage>;

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
      std::vector<float> const*, RawEnum const, OptionalEnum const, RepeatedEnum const,
      RawSubMessage const, OptionalSubMessage const, RepeatedSubMessage const>;

  explicit constexpr BaseMessageDescriptor() = default;
  virtual ~BaseMessageDescriptor() = default;

  // Returns the list of field names of the described message.
  virtual absl::Span<std::string_view const> GetAllFieldNames() const = 0;

  // Returns the type of a field from its name.
  virtual absl::StatusOr<FieldType> GetFieldType(std::string_view field_name) const = 0;

  // Returns a const pointer to the value of a field from its name. The returned type is an
  // `std::variant` that wraps all possible types. Sub-message types are also wrapped in a proxy
  // object (see `RawSubMessage`, `OptionalSubMessage`, and `RepeatedSubMessage`) allowing access to
  // the field and to the `BaseMessageDescriptor` of its type.
  virtual absl::StatusOr<ConstFieldValue> GetConstFieldValue(Message const& message,
                                                             std::string_view field_name) const = 0;

  // virtual absl::StatusOr<FieldValue> GetMutableFieldValue(Message const& message,
  //                                                         std::string_view field_name) const = 0;

  // virtual absl::Status SetFieldValue(Message const& message, std::string_view field_name,
  //                                    FieldValue new_value) const = 0;

 private:
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

  class RawEnumField final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit RawEnumField(Enum Message::*const field, BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(field, descriptor)) {}

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

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(Enum Message::*const field, BaseEnumDescriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RawEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RawEnum(&(parent->*field_), descriptor_);
      }

     private:
      Enum Message::*const field_;
      BaseEnumDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class OptionalEnumField final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit OptionalEnumField(std::optional<Enum> Message::*const field,
                               BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(field, descriptor)) {}

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

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::optional<Enum> Message::*const field, BaseEnumDescriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::OptionalEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::OptionalEnum(&(parent->*field_), descriptor_);
      }

     private:
      std::optional<Enum> Message::*const field_;
      BaseEnumDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class RepeatedEnumField final {
   public:
    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    explicit RepeatedEnumField(std::vector<Enum> Message::*const field,
                               BaseEnumDescriptor const& descriptor)
        : impl_(std::make_shared<Impl<Enum>>(field, descriptor)) {}

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

    template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
    class Impl : public BaseImpl {
     public:
      explicit Impl(std::vector<Enum> Message::*const field, BaseEnumDescriptor const& descriptor)
          : field_(field), descriptor_(descriptor) {}

      BaseEnumDescriptor const& GetDescriptor() const override { return descriptor_; }

      BaseMessageDescriptor::RepeatedEnum MakeValue(Message* const parent) const override {
        return BaseMessageDescriptor::RepeatedEnum(&(parent->*field_), descriptor_);
      }

     private:
      std::vector<Enum> Message::*const field_;
      BaseEnumDescriptor const& descriptor_;
    };

    std::shared_ptr<BaseImpl> impl_;
  };

  class RawSubMessageField final {
   public:
    explicit RawSubMessageField(tsdb2::proto::Message Message::*const message,
                                BaseMessageDescriptor const& descriptor)
        : message_(message), descriptor_(&descriptor) {}

    ~RawSubMessageField() = default;

    RawSubMessageField(RawSubMessageField const&) = default;
    RawSubMessageField& operator=(RawSubMessageField const&) = default;

    RawSubMessageField(RawSubMessageField&&) noexcept = default;
    RawSubMessageField& operator=(RawSubMessageField&&) noexcept = default;

    BaseMessageDescriptor const& descriptor() const { return *descriptor_; }

    BaseMessageDescriptor::RawSubMessage MakeValue(Message* const parent) const {
      return BaseMessageDescriptor::RawSubMessage(&(parent->*message_), *descriptor_);
    }

   private:
    tsdb2::proto::Message Message::*message_;
    BaseMessageDescriptor const* descriptor_;
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

  using FieldPointer = std::variant<
      RawInt32Field, OptionalInt32Field, RepeatedInt32Field, RawUInt32Field, OptionalUInt32Field,
      RepeatedUInt32Field, RawInt64Field, OptionalInt64Field, RepeatedInt64Field, RawUInt64Field,
      OptionalUInt64Field, RepeatedUInt64Field, RawBoolField, OptionalBoolField, RepeatedBoolField,
      RawStringField, OptionalStringField, RepeatedStringField, RawBytesField, OptionalBytesField,
      RepeatedBytesField, RawDoubleField, OptionalDoubleField, RepeatedDoubleField, RawFloatField,
      OptionalFloatField, RepeatedFloatField, RawEnumField, OptionalEnumField, RepeatedEnumField,
      RawSubMessageField, OptionalSubMessageField, RepeatedSubMessageField>;
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

// Allows implementing message descriptors, used for reflection features and TextFormat parsing.
template <typename Message, size_t num_fields>
class MessageDescriptor final : public BaseMessageDescriptor, public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldType;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor(
      std::pair<std::string_view, FieldPointer> const (&fields)[num_fields])
      : field_names_(MakeNameArray(fields)),
        field_ptrs_(tsdb2::common::fixed_flat_map_of(fields)) {}

  absl::Span<std::string_view const> GetAllFieldNames() const override { return field_names_; }

  absl::StatusOr<FieldType> GetFieldType(std::string_view const field_name) const override {
    auto const it = field_ptrs_.find(field_name);
    if (it != field_ptrs_.end()) {
      return static_cast<FieldType>(it->second.index());
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
    Message const& typed_message = *static_cast<Message const*>(&message);
    return std::visit(FieldPointerVisitor(typed_message), it->second);
  }

 private:
  class FieldPointerVisitor {
   public:
    explicit FieldPointerVisitor(Message const& message) : message_(message) {}

    ConstFieldValue operator()(int32_t Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<int32_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<int32_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(uint32_t Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<uint32_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<uint32_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(int64_t Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<int64_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<int64_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(uint64_t Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<uint64_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<uint64_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(bool Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<bool> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<bool> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::string Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::optional<std::string> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<std::string> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<uint8_t> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::optional<std::vector<uint8_t>> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<std::vector<uint8_t>> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(double Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<double> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<double> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(float Message::*const value) const { return &(message_.*value); }

    ConstFieldValue operator()(std::optional<float> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(std::vector<float> Message::*const value) const {
      return &(message_.*value);
    }

    ConstFieldValue operator()(typename FieldTypes::RawEnumField const field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

    ConstFieldValue operator()(typename FieldTypes::OptionalEnumField const field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

    ConstFieldValue operator()(typename FieldTypes::RepeatedEnumField const field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

    ConstFieldValue operator()(typename FieldTypes::RawSubMessageField const field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

    ConstFieldValue operator()(typename FieldTypes::OptionalSubMessageField const& field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

    ConstFieldValue operator()(typename FieldTypes::RepeatedSubMessageField const& field) const {
      return field.MakeValue(const_cast<Message*>(&message_));
    }

   private:
    Message const& message_;
  };

  static constexpr std::array<std::string_view, num_fields> MakeNameArray(
      std::pair<std::string_view, FieldPointer> const (&fields)[num_fields]) {
    std::array<std::string_view, num_fields> array;
    for (size_t i = 0; i < num_fields; ++i) {
      array[i] = fields[i].first;
    }
    return array;
  }

  std::array<std::string_view, num_fields> const field_names_;
  tsdb2::common::fixed_flat_map<std::string_view, FieldPointer, num_fields> const field_ptrs_;
};

// We need to specialize the version with 0 values because zero element arrays are not permitted in
// C++. The constructor of this specialization takes no arguments.
template <typename Message>
class MessageDescriptor<Message, 0> final : public BaseMessageDescriptor,
                                            public FieldTypes<Message> {
 public:
  using BaseMessageDescriptor::ConstFieldValue;
  using BaseMessageDescriptor::FieldType;
  using BaseMessageDescriptor::FieldValue;
  using BaseMessageDescriptor::OptionalSubMessage;
  using BaseMessageDescriptor::RawEnum;
  using BaseMessageDescriptor::RawSubMessage;
  using BaseMessageDescriptor::RepeatedSubMessage;

  using FieldTypes = FieldTypes<Message>;
  using typename FieldTypes::FieldPointer;

  explicit constexpr MessageDescriptor() = default;

  absl::Span<std::string_view const> GetAllFieldNames() const override { return {}; }

  absl::StatusOr<FieldType> GetFieldType(std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }

  absl::StatusOr<ConstFieldValue> GetConstFieldValue(
      tsdb2::proto::Message const& message, std::string_view const field_name) const override {
    return absl::InvalidArgumentError(
        absl::StrCat("unknown field \"", absl::CEscape(field_name), "\""));
  }
};

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
