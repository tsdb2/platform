#ifndef __TSDB2_HTTP_HUFFMAN_H__
#define __TSDB2_HTTP_HUFFMAN_H__

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/types/span.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace http {
namespace hpack {

// This class implements encoding and decoding for a specific Huffman code, the one used in the
// HPACK header compression algorithm. See https://httpwg.org/specs/rfc7541.html#huffman.code.
class HuffmanCode final {
 public:
  // Decodes the provided byte array.
  static std::string Decode(absl::Span<uint8_t const> data);

  // Encodes the provided text.
  //
  // NOTE: the returned bits don't necessarily align to the byte boundary, but if they don't we
  // implement the behavior mandated by the HPACK specs which consists of padding the tail with the
  // bits of the EOS symbol, which are all 1's.
  static tsdb2::io::Buffer Encode(std::string_view text);
};

}  // namespace hpack
}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HUFFMAN_H__
