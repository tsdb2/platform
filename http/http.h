#ifndef __TSDB2_HTTP_HTTP_H__
#define __TSDB2_HTTP_HTTP_H__

#include <cstddef>
#include <string_view>

#include "absl/base/attributes.h"

namespace tsdb2 {
namespace http {

std::string_view constexpr kClientPreface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

struct ABSL_ATTRIBUTE_PACKED FrameHeader {
  unsigned int length : 24;
  unsigned int type : 8;
  unsigned int flags : 8;
  unsigned int reserved : 1;
  unsigned int stream_id : 31;
};

size_t constexpr kFrameHeaderSize = sizeof(FrameHeader);
static_assert(kFrameHeaderSize == 9, "incorrect frame header size");

unsigned int constexpr kFrameTypeData = 0;
unsigned int constexpr kFrameTypeHeaders = 1;
unsigned int constexpr kFrameTypePriority = 2;
unsigned int constexpr kFrameTypeResetStream = 3;
unsigned int constexpr kFrameTypeSettings = 4;
unsigned int constexpr kFrameTypePushPromise = 5;
unsigned int constexpr kFrameTypePing = 6;
unsigned int constexpr kFrameTypeGoAway = 7;
unsigned int constexpr kFrameTypeWindowUpdate = 8;
unsigned int constexpr kFrameTypeContinuation = 9;

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HTTP_H__
