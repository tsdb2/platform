#ifndef __TSDB2_HTTP_MOCK_CHANNEL_MANAGER_H__
#define __TSDB2_HTTP_MOCK_CHANNEL_MANAGER_H__

#include <string_view>

#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "http/channel.h"
#include "http/handlers.h"

namespace tsdb2 {
namespace testing {
namespace http {

class MockChannelManager : public tsdb2::http::ChannelManager {
 public:
  MOCK_METHOD(void, RemoveChannel, (tsdb2::http::BaseChannel*), (override));
  MOCK_METHOD((absl::StatusOr<tsdb2::http::Handler*>), GetHandler, (std::string_view), (override));
};

}  // namespace http
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_MOCK_CHANNEL_MANAGER_H__
