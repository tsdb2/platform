#ifndef __TSDB2_HTTP_TESTING_H__
#define __TSDB2_HTTP_TESTING_H__

#include <string_view>

#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "http/channel.h"
#include "http/handlers.h"
#include "http/http.h"

namespace tsdb2 {
namespace testing {
namespace http {

class MockHandler : public tsdb2::http::Handler {
 public:
  MOCK_METHOD(void, Run, (tsdb2::http::StreamInterface*, tsdb2::http::Request const&));

  inline void operator()(tsdb2::http::StreamInterface* const stream,
                         tsdb2::http::Request const& request) override {
    Run(stream, request);
  }
};

class MockChannelManager : public tsdb2::http::ChannelManager {
 public:
  MOCK_METHOD(void, RemoveChannel, (tsdb2::http::BaseChannel*), (override));
  MOCK_METHOD((absl::StatusOr<tsdb2::http::Handler*>), GetHandler, (std::string_view), (override));
};

}  // namespace http
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_TESTING_H__
