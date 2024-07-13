#ifndef __TSDB2_HTTP_HPACK_H__
#define __TSDB2_HTTP_HPACK_H__

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace tsdb2 {
namespace http {

using HeaderSet = std::vector<std::pair<std::string, std::string>>;

class HPACKDecoder {
 public:
  explicit HPACKDecoder() = default;

  HPACKDecoder(HPACKDecoder const &) = default;
  HPACKDecoder &operator=(HPACKDecoder const &) = default;
  HPACKDecoder(HPACKDecoder &&) noexcept = default;
  HPACKDecoder &operator=(HPACKDecoder &&) noexcept = default;

  absl::StatusOr<HeaderSet> Decode(absl::Span<uint8_t const> data);

 private:
  HeaderSet dynamic_headers_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HPACK_H__
