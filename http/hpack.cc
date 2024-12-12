#include "http/hpack.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/utilities.h"
#include "http/huffman.h"
#include "io/buffer.h"
#include "io/cord.h"

namespace tsdb2 {
namespace http {
namespace hpack {

namespace {

using ::tsdb2::io::Buffer;
using ::tsdb2::io::Cord;

// See https://httpwg.org/specs/rfc7541.html#static.table.definition for the static header table
// definition.

size_t constexpr kNumStaticHeaders = 61;
std::string_view constexpr kStaticHeaders[kNumStaticHeaders][2]{
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip,deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""},
};

template <size_t... Is>
constexpr auto IndexStaticHeaders(std::index_sequence<Is...> /*index_sequence*/) {
  using StaticHeader = std::pair<std::string_view, std::string_view>;
  return tsdb2::common::fixed_flat_map_of<StaticHeader, size_t>(tsdb2::common::to_array(
      {std::make_pair(std::make_pair(kStaticHeaders[Is][0], kStaticHeaders[Is][1]), Is)...}));
}

auto constexpr kIndexedStaticHeaders =
    IndexStaticHeaders(std::make_index_sequence<kNumStaticHeaders>());

}  // namespace

void DynamicHeaderTable::SetMaxSize(size_t const new_size) {
  max_size_ = new_size;
  while (!headers_.empty() && size_ > max_size_) {
    size_ -= GetHeaderSize(headers_.back());
    headers_.pop_back();
  }
}

bool DynamicHeaderTable::Add(Header header) {
  size_ += GetHeaderSize(header);
  headers_.push_front(std::move(header));
  while (!headers_.empty() && size_ > max_size_) {
    size_ -= GetHeaderSize(headers_.back());
    headers_.pop_back();
  }
  return !headers_.empty();
}

intptr_t DynamicHeaderTable::FindHeader(Header const &header) {
  for (intptr_t i = 0; i < headers_.size(); ++i) {
    if (headers_[i] == header) {
      return i;
    }
  }
  return -1;
}

intptr_t DynamicHeaderTable::FindHeaderName(std::string_view const name) {
  for (intptr_t i = 0; i < headers_.size(); ++i) {
    if (headers_[i].first == name) {
      return i;
    }
  }
  return -1;
}

size_t DynamicHeaderTable::GetHeaderSize(Header const &header) {
  // https://httpwg.org/specs/rfc7541.html#calculating.table.size
  return header.first.size() + header.second.size() + 32;
}

absl::StatusOr<HeaderSet> Decoder::Decode(absl::Span<uint8_t const> const data) {
  HeaderSet headers;
  size_t offset = 0;
  while (offset < data.size()) {
    auto const first_byte = data[offset];
    if ((first_byte & 0x80) != 0) {
      DEFINE_CONST_OR_RETURN(index, DecodeInteger(data, offset, 7));
      if (index < 1) {
        return absl::InvalidArgumentError(
            "invalid HPACK encoding: indices must be greater than zero");
      }
      DEFINE_VAR_OR_RETURN(header, GetHeader(index - 1));
      headers.emplace_back(std::move(header));
    } else if ((first_byte & 0x40) != 0) {
      DEFINE_CONST_OR_RETURN(index, DecodeInteger(data, offset, 6));
      if (index > 0) {
        DEFINE_VAR_OR_RETURN(name, GetHeaderName(index - 1));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        dynamic_headers_.Add(std::make_pair(name, value));
        headers.emplace_back(std::move(name), std::move(value));
      } else {
        DEFINE_VAR_OR_RETURN(name, DecodeString(data, offset));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        dynamic_headers_.Add(std::make_pair(name, value));
        headers.emplace_back(std::move(name), std::move(value));
      }
    } else if ((first_byte & 0x20) != 0) {
      DEFINE_CONST_OR_RETURN(new_size, DecodeInteger(data, offset, 5));
      if (new_size > max_dynamic_header_table_size_) {
        return absl::InvalidArgumentError(
            absl::StrCat("the requested dynamic header table size limit exceeds the latest "
                         "SETTINGS_HEADER_TABLE_SIZE value (",
                         max_dynamic_header_table_size_, ")"));
      }
      dynamic_headers_.SetMaxSize(new_size);
    } else if ((first_byte & 0x10) != 0) {
      DEFINE_CONST_OR_RETURN(index, DecodeInteger(data, offset, 4));
      if (index > 0) {
        DEFINE_VAR_OR_RETURN(name, GetHeaderName(index - 1));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        headers.emplace_back(std::move(name), std::move(value));
      } else {
        DEFINE_VAR_OR_RETURN(name, DecodeString(data, offset));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        headers.emplace_back(std::move(name), std::move(value));
      }
    } else {
      DEFINE_CONST_OR_RETURN(index, DecodeInteger(data, offset, 4));
      if (index > 0) {
        DEFINE_VAR_OR_RETURN(name, GetHeaderName(index - 1));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        headers.emplace_back(std::move(name), std::move(value));
      } else {
        DEFINE_VAR_OR_RETURN(name, DecodeString(data, offset));
        DEFINE_VAR_OR_RETURN(value, DecodeString(data, offset));
        headers.emplace_back(std::move(name), std::move(value));
      }
    }
  }
  return headers;
}

absl::Status Decoder::IntegerDecodingError(std::string_view const message) {
  return absl::InvalidArgumentError(absl::StrCat("integer decoding failed: ", message));
}

absl::Status Decoder::StringDecodingError(std::string_view const message) {
  return absl::InvalidArgumentError(absl::StrCat("string decoding failed: ", message));
}

absl::StatusOr<size_t> Decoder::DecodeInteger(absl::Span<uint8_t const> const data, size_t &offset,
                                              size_t const prefix_bits) {
  if (offset >= data.size()) {
    return IntegerDecodingError("reached end of input");
  }
  uint8_t const mask = (1 << prefix_bits) - 1;
  size_t value = data[offset++] & mask;
  if (value < mask) {
    return value;
  }
  uint8_t byte;
  size_t m = 0;
  do {
    if (offset >= data.size()) {
      return IntegerDecodingError("reached end of input");
    }
    if (m > 64) {
      return IntegerDecodingError("exceeds 64 bits");
    }
    byte = data[offset++];
    value += (byte & 0x7F) << m;
    m += 7;
  } while ((byte & 0x80) != 0);
  return value;
}

absl::StatusOr<std::string> Decoder::DecodeString(absl::Span<uint8_t const> const data,
                                                  size_t &offset) {
  if (offset >= data.size()) {
    return StringDecodingError("reached end of input");
  }
  bool const use_huffman = (data[offset] & 0x80) != 0;
  DEFINE_CONST_OR_RETURN(length, DecodeInteger(data, offset, 7));
  if (offset + length > data.size()) {
    return StringDecodingError("reached end of input");
  }
  auto const subspan = data.subspan(offset, length);
  offset += length;
  if (use_huffman) {
    return HuffmanCode::Decode(subspan);
  } else {
    return std::string{subspan.begin(), subspan.end()};
  }
}

absl::StatusOr<Header> Decoder::GetHeader(size_t const index) const {
  if (index < kNumStaticHeaders) {
    auto const &header = kStaticHeaders[index];
    return Header(header[0], header[1]);
  } else if (index - kNumStaticHeaders < dynamic_headers_.size()) {
    return dynamic_headers_[index - kNumStaticHeaders];
  } else {
    return absl::InvalidArgumentError("invalid header index");
  }
}

absl::StatusOr<std::string> Decoder::GetHeaderName(size_t const index) const {
  if (index < kNumStaticHeaders) {
    return std::string(kStaticHeaders[index][0]);
  } else if (index - kNumStaticHeaders < dynamic_headers_.size()) {
    return dynamic_headers_[index - kNumStaticHeaders].first;
  } else {
    return absl::InvalidArgumentError("invalid header index");
  }
}

Buffer Encoder::Encode(HeaderSet const &headers) {
  Cord cord;
  for (auto const &header : headers) {
    auto index = FindHeader(header);
    if (index > 0) {
      auto buffer = EncodeInteger(index, 7);
      buffer.at<uint8_t>(0) |= 0x80;
      cord.Append(std::move(buffer));
      continue;
    }
    index = FindHeaderName(header.first);
    if (index > 0) {
      auto buffer = EncodeInteger(index, 7);
      buffer.at<uint8_t>(0) |= 0x40;
      cord.Append(std::move(buffer));
    } else {
      uint8_t const code = 0x40;
      cord.Append(Buffer(&code, 1));
      cord.Append(EncodeString(header.first, /*use_huffman=*/true));
    }
    cord.Append(EncodeString(header.second, /*use_huffman=*/true));
    dynamic_headers_.Add(header);
  }
  return std::move(cord).Flatten();
}

Buffer Encoder::EncodeInteger(size_t value, size_t const prefix_bits) {
  CHECK_GT(prefix_bits, 0);
  CHECK_LE(prefix_bits, 8);
  uint8_t const mask = (1 << prefix_bits) - 1;
  if (value < mask) {
    uint8_t const byte = value;
    return Buffer(&byte, 1);
  }
  static size_t constexpr kMaxEncodedSize = 11;
  Buffer buffer{kMaxEncodedSize};
  buffer.Append<uint8_t>(mask);
  value -= mask;
  while (value > 0x7F) {
    buffer.Append<uint8_t>(0x80 + (value & 0x7F));
    value >>= 7;
  }
  return buffer;
}

Cord Encoder::EncodeString(std::string_view const string, bool const use_huffman) {
  Cord cord;
  if (use_huffman) {
    auto buffer = HuffmanCode::Encode(string);
    cord.Append(EncodeInteger(buffer.size(), /*prefix_bits=*/7));
    cord[0] |= 0x80;
    cord.Append(std::move(buffer));
  } else {
    cord.Append(EncodeInteger(string.size(), /*prefix_bits=*/7));
    Buffer buffer{string.size()};
    buffer.MemCpy(string.data(), string.size());
    cord.Append(std::move(buffer));
  }
  return cord;
}

size_t Encoder::FindHeader(Header const &header) {
  auto const it = kIndexedStaticHeaders.find(header);
  if (it != kIndexedStaticHeaders.end()) {
    return it->second + 1;
  }
  auto const index = dynamic_headers_.FindHeader(header);
  if (index < 0) {
    return 0;
  }
  return kNumStaticHeaders + index + 1;
}

size_t Encoder::FindHeaderName(std::string_view const name) {
  auto const it = kIndexedStaticHeaders.lower_bound(Header(name, ""));
  if (it != kIndexedStaticHeaders.end() && it->first.first == name) {
    return it->second + 1;
  }
  auto const index = dynamic_headers_.FindHeaderName(name);
  if (index < 0) {
    return 0;
  }
  return kNumStaticHeaders + index + 1;
}

}  // namespace hpack
}  // namespace http
}  // namespace tsdb2
