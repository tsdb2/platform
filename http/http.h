#ifndef __TSDB2_HTTP_HTTP_H__
#define __TSDB2_HTTP_HTTP_H__

#include <cstddef>
#include <string_view>

#include "absl/base/attributes.h"

namespace tsdb2 {
namespace http {

inline std::string_view constexpr kClientPreface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

struct ABSL_ATTRIBUTE_PACKED FrameHeader {
  unsigned int length : 24;
  unsigned int type : 8;
  unsigned int flags : 8;
  unsigned int reserved : 1;
  unsigned int stream_id : 31;
};

inline size_t constexpr kFrameHeaderSize = sizeof(FrameHeader);
static_assert(kFrameHeaderSize == 9, "incorrect frame header size");

inline unsigned int constexpr kFrameTypeData = 0;
inline unsigned int constexpr kFrameTypeHeaders = 1;
inline unsigned int constexpr kFrameTypePriority = 2;
inline unsigned int constexpr kFrameTypeResetStream = 3;
inline unsigned int constexpr kFrameTypeSettings = 4;
inline unsigned int constexpr kFrameTypePushPromise = 5;
inline unsigned int constexpr kFrameTypePing = 6;
inline unsigned int constexpr kFrameTypeGoAway = 7;
inline unsigned int constexpr kFrameTypeWindowUpdate = 8;
inline unsigned int constexpr kFrameTypeContinuation = 9;

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HTTP_H__
