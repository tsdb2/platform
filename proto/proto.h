#ifndef __TSDB2_PROTO_PROTO_H__
#define __TSDB2_PROTO_PROTO_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace proto {

// Base class for all protobuf messages.
struct Message {};

// Allows for meaningful hashing and comparisons between optional sub-message fields regardless of
// their indirection type.
template <typename SubMessage>
class OptionalSubMessageRef final {
 public:
  explicit OptionalSubMessageRef() : ptr_(nullptr) {}

  explicit OptionalSubMessageRef(std::optional<SubMessage> const& field)
      : ptr_(field.has_value() ? &(field.value()) : nullptr) {}

  explicit OptionalSubMessageRef(std::unique_ptr<SubMessage> const& field) : ptr_(field.get()) {}
  explicit OptionalSubMessageRef(std::shared_ptr<SubMessage> const& field) : ptr_(field.get()) {}

  ~OptionalSubMessageRef() = default;

  OptionalSubMessageRef(OptionalSubMessageRef const&) = default;
  OptionalSubMessageRef& operator=(OptionalSubMessageRef const&) = default;

  OptionalSubMessageRef(OptionalSubMessageRef&&) noexcept = default;
  OptionalSubMessageRef& operator=(OptionalSubMessageRef&&) noexcept = default;

  friend bool operator==(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    if (lhs.ptr_ != nullptr) {
      return rhs.ptr_ != nullptr && *(lhs.ptr_) == *(rhs.ptr_);
    } else {
      return rhs.ptr_ == nullptr;
    }
  }

  friend bool operator!=(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    return !operator==(lhs, rhs);
  }

  friend bool operator<(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    if (lhs.ptr_ != nullptr) {
      return rhs.ptr_ != nullptr && *(lhs.ptr_) < *(rhs.ptr_);
    } else {
      return rhs.ptr_ != nullptr;
    }
  }

  friend bool operator<=(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    return !(rhs < lhs);
  }

  friend bool operator>(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    return rhs < lhs;
  }

  friend bool operator>=(OptionalSubMessageRef const& lhs, OptionalSubMessageRef const& rhs) {
    return !(lhs < rhs);
  }

  template <typename H>
  friend H AbslHashValue(H h, OptionalSubMessageRef const& ref) {
    if (ref.ptr_ != nullptr) {
      return H::combine(std::move(h), *(ref.ptr_));
    } else {
      return H::combine(std::move(h), nullptr);
    }
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OptionalSubMessageRef const& ref) {
    return State::Combine(std::move(state), ref.ptr_);
  }

 private:
  SubMessage const* ptr_;
};

namespace internal {

// Used by `Tie` (see below) to make a reference to an individual field. It simply returns a const
// reference to the field for most types, but the specializations for optional sub-message fields
// use `OptionalSubMessageRef`.
template <typename Field, typename Enable = void>
class MakeRef {
 public:
  explicit MakeRef(Field const& field) : field_(field) {}
  Field const& operator()() && { return field_; }

 private:
  Field const& field_;
};

template <typename SubMessage>
class MakeRef<std::optional<SubMessage>, std::enable_if_t<std::is_base_of_v<Message, SubMessage>>> {
 public:
  explicit MakeRef(std::optional<SubMessage> const& field) : field_(field) {}
  OptionalSubMessageRef<SubMessage> operator()() && { return field_; }

 private:
  OptionalSubMessageRef<SubMessage> field_;
};

template <typename SubMessage>
class MakeRef<std::unique_ptr<SubMessage>,
              std::enable_if_t<std::is_base_of_v<Message, SubMessage>>> {
 public:
  explicit MakeRef(std::unique_ptr<SubMessage> const& field) : field_(field) {}
  OptionalSubMessageRef<SubMessage> operator()() && { return field_; }

 private:
  OptionalSubMessageRef<SubMessage> field_;
};

template <typename SubMessage>
class MakeRef<std::shared_ptr<SubMessage>,
              std::enable_if_t<std::is_base_of_v<Message, SubMessage>>> {
 public:
  explicit MakeRef(std::shared_ptr<SubMessage> const& field) : field_(field) {}
  OptionalSubMessageRef<SubMessage> operator()() && { return field_; }

 private:
  OptionalSubMessageRef<SubMessage> field_;
};

}  // namespace internal

// Returns a tuple of references to the fields of a protobuf. The returned tuple is suitable for
// message comparisons, hashing, and fingerprinting. You don't need to use this function directly,
// all generated protobuf messages have their own `Tie` method based on this function and are also
// directly comparable, hashable, and fingerprintable.
template <typename... Fields>
auto Tie(Fields&&... fields) {
  return std::tuple(internal::MakeRef{std::forward<Fields>(fields)}()...);
}

// Handles extension data inside extensible messages.
class ExtensionData final {
 public:
  explicit ExtensionData() = default;

  explicit ExtensionData(tsdb2::io::Buffer data) : data_(std::move(data)) {}

  ~ExtensionData() = default;

  ExtensionData(ExtensionData const& other) : data_(other.data_.Clone()) {}

  ExtensionData& operator=(ExtensionData const& other) {
    if (this != &other) {
      data_ = other.data_.Clone();
    }
    return *this;
  }

  ExtensionData(ExtensionData&&) noexcept = default;
  ExtensionData& operator=(ExtensionData&&) noexcept = default;

  void swap(ExtensionData& other) noexcept {
    if (this != &other) {
      using std::swap;  // Ensure ADL.
      swap(data_, other.data_);
    }
  }

  friend void swap(ExtensionData& lhs, ExtensionData& rhs) noexcept { lhs.swap(rhs); }

  friend bool operator==(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ == rhs.data_;
  }

  friend bool operator!=(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ != rhs.data_;
  }

  friend bool operator<(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ < rhs.data_;
  }

  friend bool operator<=(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ <= rhs.data_;
  }

  friend bool operator>(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ > rhs.data_;
  }

  friend bool operator>=(ExtensionData const& lhs, ExtensionData const& rhs) {
    return lhs.data_ >= rhs.data_;
  }

  template <typename H>
  friend H AbslHashValue(H h, ExtensionData const& data) {
    return H::combine(std::move(h), data.data_);
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, ExtensionData const& data) {
    return State::Combine(std::move(state), data.data_);
  }

  [[nodiscard]] bool empty() const { return data_.empty(); }
  size_t size() const { return data_.size(); }
  size_t capacity() const { return data_.capacity(); }
  absl::Span<uint8_t const> span() const { return data_.span(); }

  void Clear() { data_.Clear(); }

  tsdb2::io::Buffer Release() {
    tsdb2::io::Buffer buffer = std::move(data_);
    data_ = tsdb2::io::Buffer();
    return buffer;
  }

 private:
  tsdb2::io::Buffer data_;
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
