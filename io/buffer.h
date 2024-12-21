#ifndef __TSDB2_IO_BUFFER_H__
#define __TSDB2_IO_BUFFER_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace io {

// Manages an owned, preallocated memory buffer.
class Buffer {
 public:
  // Constructs an empty `Buffer` object. The wrapped buffer is not allocated, methods like `get()`
  // return nullptr, and both the size and capacity are 0.
  explicit Buffer() = default;

  // Constructs a Buffer with an allocated `capacity` and initial length 0.
  explicit Buffer(size_t const capacity)
      : capacity_(capacity), length_(0), data_(new uint8_t[capacity_]) {}

  // Takes ownership of the buffer pointed to by the `data` raw pointer, which must have `capacity`
  // bytes of capacity and at least `length` initialized bytes.
  //
  // REQUIRES: `data` must have been allocated on the heap using `operator new`.
  explicit Buffer(gsl::owner<void*> const data, size_t const capacity, size_t const length)
      : capacity_(capacity), length_(length), data_(static_cast<uint8_t*>(data)) {}

  // Allocates a buffer with `size` capacity and length and copies `data` into it. This constructor
  // does not take ownership of `data`.
  explicit Buffer(void const* const data, size_t const size)
      : capacity_(size), length_(size), data_(new uint8_t[size]) {
    std::memcpy(data_, data, size);
  }

  explicit Buffer(absl::Span<uint8_t const> const bytes)
      : capacity_(bytes.size()), length_(bytes.size()), data_(new uint8_t[bytes.size()]) {
    std::memcpy(data_, bytes.data(), bytes.size());
  }

  // Frees any memory used by the buffer.
  ~Buffer() { delete[] data_; }

  // Moving transfers ownership of the buffer.

  Buffer(Buffer&& other) noexcept
      : capacity_(other.capacity_), length_(other.length_), data_(other.Release()) {}

  Buffer& operator=(Buffer&& other) noexcept {
    if (this != &other) {
      delete[] data_;
      capacity_ = other.capacity_;
      length_ = other.length_;
      data_ = other.Release();
    }
    return *this;
  }

  void swap(Buffer& other) noexcept {
    std::swap(capacity_, other.capacity_);
    std::swap(length_, other.length_);
    std::swap(data_, other.data_);
  }

  friend void swap(Buffer& lhs, Buffer& rhs) noexcept { lhs.swap(rhs); }

  // Returns the allocated capacity.
  size_t capacity() const { return capacity_; }

  // Returns the size of the buffer, in bytes.
  size_t size() const { return length_; }

  // True iff `size` equals 0.
  [[nodiscard]] bool empty() const { return length_ == 0; }

  // Returns a pointer to the buffer.
  void* get() { return data_; }
  void const* get() const { return data_; }

  // Returns an `absl::Span` referring to this buffer's data.
  //
  // REQUIRES: the buffer must not be empty.
  absl::Span<uint8_t const> span() const { return absl::Span<uint8_t const>(data_, length_); }

  // Returns a subspan of this buffer's data from `offset` to the end. Equivalent to calling
  // `span().subspan(offset)`.
  absl::Span<uint8_t const> span(size_t const offset) const { return span().subspan(offset); }

  // Returns a subspan of this buffer's data. Equivalent to calling
  // `span().subspan(offset, length)`.
  absl::Span<uint8_t const> span(size_t const offset, size_t const length) const {
    return span().subspan(offset, length);
  }

  // Returns this buffer's data interpreted as an array of `Type` values.
  //
  // NOTE: the size of the returned span is the size of the buffer divided by `sizeof(Type)`. This
  // function doesn't check that the former is a multiple of the latter, it's up to the caller to
  // ensure correctness.
  template <typename Type>
  absl::Span<Type const> span() const {
    return absl::Span<Type const>(reinterpret_cast<Type const*>(data_), length_ / sizeof(Type));
  }

  // Returns the buffer's data at `offset` interpreted as an array of `Type` values.
  //
  // NOTE: the `offset` is in bytes. The returned data is potentially unaligned.
  //
  // NOTE: the size of the returned span is calculated as the size of the buffer minus offset,
  // divided by `sizeof(Type)`. This function doesn't check that the dividend is a multiple of the
  // divisor, it's up to the caller to ensure correctness.
  template <typename Type>
  absl::Span<Type const> span(size_t const offset) const {
    return absl::Span<Type const>(reinterpret_cast<Type const*>(data_ + offset),
                                  (length_ - offset) / sizeof(Type));
  }

  // Returns the buffer's data at `offset` interpreted as an array of `count` values of type `Type`.
  //
  // NOTE: the `offset` is in bytes. The returned data is potentially unaligned.
  //
  // WARNING: this function doesn't perform any bound checking, it's up to the caller to ensure that
  // the buffer has sufficient space remaining at `offset` to contain `count` values of type `Type`.
  template <typename Type>
  absl::Span<Type const> span(size_t const offset, size_t const count) const {
    return absl::Span<Type const>(reinterpret_cast<Type const*>(data_ + offset), count);
  }

  // Alias for `span()`.
  //
  // REQUIRES: the buffer must not be empty.
  explicit operator absl::Span<uint8_t const>() const { return span(); }

  // Returns a pointer to the buffer as a byte array. This is the same as
  // `static_cast<uint8_t*>(get())`.
  uint8_t* as_byte_array() { return data_; }
  uint8_t const* as_byte_array() const { return data_; }

  // Returns a pointer to the buffer as an array of `Type` elements. This is the same as
  // `reinterpret_cast<Type*>(get())`.
  //
  // WARNING: this function doesn't check that the size of the buffer is a multiple of
  // `sizeof(Type)`. It only reinterprets the memory pointed to by this Buffer.
  template <typename Type>
  Type* as_array() {
    return reinterpret_cast<Type*>(data_);
  }

  // Returns a pointer to the buffer as an array of `Type const` elements. This is the same as
  // `reinterpret_cast<Type const*>(get())`.
  //
  // WARNING: this function doesn't check that the size of the buffer is a multiple of
  // `sizeof(Type)`. It only reinterprets the memory pointed to by this Buffer.
  template <typename Type>
  Type const* as_array() const {
    return reinterpret_cast<Type const*>(data_);
  }

  // Returns a pointer to the buffer as a byte array. This is the same as
  // `reinterpret_cast<uint8_t*>(get())`.
  char* as_char_array() { return reinterpret_cast<char*>(data_); }
  char const* as_char_array() const { return reinterpret_cast<char const*>(data_); }

  // True iff the Buffer object is non-empty AND `size() == capacity()` (i.e. the buffer is full).
  bool is_full() const { return capacity_ > 0 && !(length_ < capacity_); }

  // Returns the bytes at `offset` interpreted as a value of type `Data`. The `offset` is expressed
  // in bytes, independently of `sizeof(Data)`.
  //
  // NOTE: this function doesn't perform any endianness conversion. Since `Buffer` is mainly meant
  // for IPC, bytes will typically be stored in network byte order here. It's the caller's
  // responsibility to call functions like `ntoh*` as needed on the returned value.
  //
  // WARNING: this function doesn't perform any bound checking. In case of an overflow the behavior
  // is undefined.
  template <typename Data>
  Data& at(size_t const offset) {
    return *reinterpret_cast<Data*>(data_ + offset);
  }

  // Returns the bytes at `offset` interpreted as a value of type `Data`. The `offset` is expressed
  // in bytes, independently of `sizeof(Data)`.
  //
  // NOTE: this function doesn't perform any endianness conversion. Since `Buffer` is mainly meant
  // for IPC, bytes will typically be stored in network byte order here. It's the caller's
  // responsibility to call functions like `ntoh*` as needed on the returned value.
  //
  // WARNING: this function doesn't perform any bound checking. In case of an overflow the behavior
  // is undefined.
  template <typename Data>
  Data const& at(size_t const offset) const {
    return *reinterpret_cast<Data const*>(data_ + offset);
  }

  // Returns a const reference to the content of the buffer interpreted as a value of type `Data`.
  //
  // NOTE: this function doesn't perform any endianness conversion. Since `Buffer` is mainly meant
  // for IPC, bytes will typically be stored in network byte order here. It's the caller's
  // responsibility to call functions like `ntoh*` as needed on the returned value.
  //
  // WARNING: this function doesn't perform any bound checking. The caller MUST make sure that
  // `sizeof(Data)` is (less than or) equal to the buffer size as returned by `size()`.
  template <typename Data>
  Data& as() {
    return *reinterpret_cast<Data*>(data_);
  }

  // Returns a const reference to the content of the buffer interpreted as a value of type `Data`.
  //
  // NOTE: this function doesn't perform any endianness conversion. Since `Buffer` is mainly meant
  // for IPC, bytes will typically be stored in network byte order here. It's the caller's
  // responsibility to call functions like `ntoh*` as needed on the returned value.
  //
  // WARNING: this function doesn't perform any bound checking. The caller MUST make sure that
  // `sizeof(Data)` is (less than or) equal to the buffer size as returned by `size()`.
  template <typename Data>
  Data const& as() const {
    return *reinterpret_cast<Data const*>(data_);
  }

  // Appends the provided `word` to the buffer. The word must be of an arithmetic type, i.e.: a
  // boolean, an integer type, a floating point type, or a cv-qualified version thereof. Pointers,
  // aggregates, and other types are not allowed because `Buffer` is mainly meant for IPC, so those
  // types don't make sense in the context of another process.
  //
  // This method check-fails in case of a buffer overflow, i.e. if `size() + sizeof(Word) >
  // capacity()`.
  //
  // NOTE: `Buffer` is mainly intended for IPC but it doesn't perform any endianness conversion, so
  // it's the caller's responsibility to ensure the correct endianness. Bytes would typically be
  // stored in a `Buffer` in network byte order, so the caller will typically need to call functions
  // like `hton*`.
  template <typename Word, std::enable_if_t<std::is_arithmetic_v<Word>, bool> = true>
  Buffer& Append(Word const word) {
    CHECK_LE(length_ + sizeof(Word), capacity_) << "buffer overflow";
    *reinterpret_cast<Word*>(data_ + length_) = word;
    length_ += sizeof(word);
    return *this;
  }

  // Copies the entire content of `other` appending it to the end of this buffer. `other` is not
  // changed. Check-fails if this buffer doesn't have enough capacity (that is, if
  // `size() + other.size() > capacity()`).
  //
  // NOTE: this method may be expensive because the data from `other` is copied (we use
  // `std::memcpy`).
  //
  // NOTE: unlike the template overload of `Append` which takes a word in host byte order, this
  // overload takes a buffer that's supposed to contain data already in network byte order.
  // Therefore not endianness conversion is needed by this overload.
  Buffer& Append(Buffer const& other) {
    CHECK_LE(length_ + other.length_, capacity_) << "buffer overflow";
    std::memcpy(data_ + length_, other.data_, other.length_);
    length_ += other.length_;
    return *this;
  }

  // Increments the size by the specified amount. Meant to be called as a result of writing data to
  // the memory pointed to by `get()` / `as_byte_array()`. Example:
  //
  //   Buffer buffer{20};
  //   buffer.Append<uint32_t>(42);
  //   std::memcpy(source, buffer.get(), 10);
  //   buffer.Advance(10);
  //   std::cout << buffer.size() << std::endl;  // prints "14"
  //
  // Note that you can use the `MemCpy` method instead of `std::memcpy`+`Advance`.
  //
  // `Advance` check-fails if the resulting size is greater than the capacity.
  void Advance(size_t const delta) {
    length_ += delta;
    CHECK_LE(length_, capacity_) << "buffer overflow";
  }

  // Copies `length` bytes from `source` into the buffer, advancing the size of the buffer
  // accordingly.
  //
  // This is equivalent to calling `std::memcpy` followed by `Advance`:
  //
  //   Buffer buffer{20};
  //   buffer.Append<uint32_t>(42);
  //   buffer.MemCpy(source, 10);
  //   std::cout << buffer.size() << std::endl;  // prints "14"
  //
  // `MemCpy` check-fails if the buffer capacity would be exceeded.
  void MemCpy(void const* const source, size_t const length) {
    CHECK_LE(length_ + length, capacity_) << "buffer overflow";
    std::memcpy(data_ + length_, source, length);
    length_ += length;
  }

  // Releases ownership of the buffer, invalidating this object and returning a pointer to the
  // previously wrapped data.
  gsl::owner<uint8_t*> Release() {
    gsl::owner<uint8_t*> const data = data_;
    capacity_ = 0;
    length_ = 0;
    data_ = nullptr;
    return data;
  }

 private:
  // Copies are forbidden because they are potentially expensive.
  Buffer(Buffer const&) = delete;
  Buffer& operator=(Buffer const&) = delete;

  size_t capacity_ = 0;
  size_t length_ = 0;
  gsl::owner<uint8_t*> data_ = nullptr;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_BUFFER_H__
