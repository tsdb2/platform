#include "http/handlers.h"

#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "common/utilities.h"
#include "http/hpack.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

void StreamInterface::SendFieldsOrLog(hpack::HeaderSet const& fields, bool const end_stream) {
  auto const status = SendFields(fields, end_stream);
  if (!status.ok()) {
    LOG(ERROR) << status;
  }
}

void StreamInterface::SendDataOrLog(tsdb2::net::Buffer buffer, bool const end_stream) {
  auto const status = SendData(std::move(buffer), end_stream);
  if (!status.ok()) {
    LOG(ERROR) << status;
  }
}

absl::Status StreamInterface::SendResponse(hpack::HeaderSet const& fields,
                                           tsdb2::net::Buffer data) {
  RETURN_IF_ERROR(SendFields(fields, /*end_stream=*/false));
  return SendData(std::move(data), /*end_stream=*/true);
}

void StreamInterface::SendResponseOrLog(hpack::HeaderSet const& fields, tsdb2::net::Buffer data) {
  auto const status = SendResponse(fields, std::move(data));
  if (!status.ok()) {
    LOG(ERROR) << status;
  }
}

}  // namespace http
}  // namespace tsdb2
