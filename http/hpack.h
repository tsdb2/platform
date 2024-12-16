#ifndef __TSDB2_HTTP_HPACK_H__
#define __TSDB2_HTTP_HPACK_H__

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "http/http.h"
#include "io/buffer.h"
#include "io/cord.h"

namespace tsdb2 {
namespace http {
namespace hpack {

using Header = std::pair<std::string, std::string>;
using HeaderSet = std::vector<Header>;

// Implements the HPACK dynamic header table.
//
// The table size calculated in octets (as per
// https://httpwg.org/specs/rfc7541.html#calculating.table.size) is capped to a configurable maximum
// value, initially 4096.
//
// This class is not thread-safe, only thread-friendly.
class DynamicHeaderTable final {
 public:
  explicit DynamicHeaderTable(size_t const max_size) : max_size_(max_size) {}
  ~DynamicHeaderTable() = default;

  DynamicHeaderTable(DynamicHeaderTable const &) = default;
  DynamicHeaderTable &operator=(DynamicHeaderTable const &) = default;
  DynamicHeaderTable(DynamicHeaderTable &&) noexcept = default;
  DynamicHeaderTable &operator=(DynamicHeaderTable &&) noexcept = default;

  // Returns the current table size, in octets.
  size_t size() const { return size_; }

  // Returns the number of header entries currently in the table.
  size_t num_headers() const { return headers_.size(); }

  // Returns the maximum table size, in octets.
  size_t max_size() const { return max_size_; }

  // Changes the maximum table size, possibly removing headers that no longer fit. The `new_size` is
  // in octets.
  void SetMaxSize(size_t new_size);

  // Returns the i-th header in the table. The `index` is zero-based.
  //
  // WARNING: this function does not perform bounds checking, it's up to the caller to make sure
  // that `index` is strictly less than `num_headers()`.
  Header const &operator[](size_t const index) const { return headers_[index]; }

  // Adds a new header to the table, possibly evicting the oldest entries until the table size is
  // less than or equal to the maximum size.
  //
  // It is possible that the provided header is larger than the maximum table size, in which case
  // the final eviction algorithm will end up evicting all entries and emptying the table. That is
  // not an error, as per the specs.
  //
  // The returned boolean is true if the new header was inserted and false if it was evicted, i.e.
  // false indicates that the table is now empty.
  bool Add(Header header);

  // Searches the specified header in the table. Returns its zero-based index if a match is found,
  // or a negative number otherwise.
  intptr_t FindHeader(Header const &header);

  // Searches the table for a header with the specified name. Returns its zero-based index if a
  // match is found, or a negative number otherwise.
  intptr_t FindHeaderName(std::string_view name);

 private:
  static size_t GetHeaderSize(Header const &header);

  // The maximum table size, in octets.
  size_t max_size_;

  // The current table size, in octets.
  size_t size_ = 0;

  // The headers in the table.
  std::deque<Header> headers_;
};

// An HPACK decoder.
//
// This class is not thread-safe, only thread-friendly.
class Decoder final {
 public:
  explicit Decoder() = default;
  ~Decoder() = default;

  Decoder(Decoder const &) = default;
  Decoder &operator=(Decoder const &) = default;
  Decoder(Decoder &&) noexcept = default;
  Decoder &operator=(Decoder &&) noexcept = default;

  // Returns the maximum dynamic header table size, in octets.
  size_t max_dynamic_header_table_size() const { return max_dynamic_header_table_size_; }

  // Updates the maximum dynamic header table size. This is invoked in response to a change in the
  // `SETTINGS_HEADER_TABLE_SIZE` setting.
  void set_max_dynamic_header_table_size(size_t const new_size) {
    max_dynamic_header_table_size_ = new_size;
    if (dynamic_headers_.max_size() > max_dynamic_header_table_size_) {
      dynamic_headers_.SetMaxSize(max_dynamic_header_table_size_);
    }
  }

  absl::StatusOr<HeaderSet> Decode(absl::Span<uint8_t const> data);

 private:
  static absl::Status IntegerDecodingError(std::string_view message);
  static absl::Status StringDecodingError(std::string_view message);

  static absl::StatusOr<size_t> DecodeInteger(absl::Span<uint8_t const> data, size_t &offset,
                                              size_t prefix_bits);

  static absl::StatusOr<std::string> DecodeString(absl::Span<uint8_t const> data, size_t &offset);

  // Returns the i-th header from the unified address space of the static and dynamic tables.
  // `index` is zero based. If `index` is greater than the last available index, an error status is
  // returned.
  absl::StatusOr<Header> GetHeader(size_t index) const;

  // Like `GetHeader`, but returns the header name only. Used when parsing literal header fields
  // with incremental indexing.
  absl::StatusOr<std::string> GetHeaderName(size_t index) const;

  // The maximum size of the `dynamic_headers_` table calculated in octets as per
  // https://httpwg.org/specs/rfc7541.html#calculating.table.size).
  size_t max_dynamic_header_table_size_ = kDefaultMaxDynamicHeaderTableSize;

  DynamicHeaderTable dynamic_headers_{max_dynamic_header_table_size_};
};

// An HPACK encoder.
//
// This class is not thread-safe, only thread-friendly.
class Encoder final {
 public:
  explicit Encoder() = default;
  ~Encoder() = default;

  Encoder(Encoder const &) = default;
  Encoder &operator=(Encoder const &) = default;
  Encoder(Encoder &&) noexcept = default;
  Encoder &operator=(Encoder &&) noexcept = default;

  // Returns the maximum dynamic header table size, in octets.
  size_t max_dynamic_header_table_size() const { return max_dynamic_header_table_size_; }

  // Updates the maximum dynamic header table size. This is invoked in response to a change in the
  // `SETTINGS_HEADER_TABLE_SIZE` setting.
  void set_max_dynamic_header_table_size(size_t const new_size) {
    max_dynamic_header_table_size_ = new_size;
    if (dynamic_headers_.max_size() > max_dynamic_header_table_size_) {
      dynamic_headers_.SetMaxSize(max_dynamic_header_table_size_);
    }
  }

  tsdb2::io::Buffer Encode(HeaderSet const &headers);

 private:
  static tsdb2::io::Buffer EncodeInteger(size_t value, size_t prefix_bits);
  static tsdb2::io::Cord EncodeString(std::string_view string);

  // Searches the specified header in the static and dynamic header tables, returning its index if
  // found. Indices start from 1, so they're ready to be encoded as per the HPACK specs. 0 is
  // returned if the header is not found.
  size_t FindHeader(Header const &header);

  // Searches the static and dynamic header tables for a header with the specified name, returning
  // its index if found. Indices start from 1, so they're ready to be encoded as per the HPACK
  // specs. 0 is returned if the header is not found.
  size_t FindHeaderName(std::string_view name);

  // The maximum size of the `dynamic_headers_` table calculated in octets as per
  // https://httpwg.org/specs/rfc7541.html#calculating.table.size).
  size_t max_dynamic_header_table_size_ = kDefaultMaxDynamicHeaderTableSize;

  // This copy of the dynamic table tracks the state of the dynamic table in the decoder of the peer
  // endpoint. Unless there's a bug, the two dynamic tables must be identical at all times.
  DynamicHeaderTable dynamic_headers_{max_dynamic_header_table_size_};
};

}  // namespace hpack
}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HPACK_H__
