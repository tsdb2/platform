#ifndef __TSDB2_HTTP_HANDLERS_H__
#define __TSDB2_HTTP_HANDLERS_H__

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "http/hpack.h"
#include "http/http.h"
#include "io/cord.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

// This interface allows HTTP/2 handlers to interact with a stream.
class StreamInterface {
 public:
  using DataCallback =
      absl::AnyInvocable<void(absl::StatusOr<tsdb2::io::Cord> status_or_data, bool end)>;

  explicit StreamInterface() = default;
  virtual ~StreamInterface() = default;

  // Reads the next chunk of data for the stream.
  //
  // If any data is already buffered in the stream the callback is invoked immediately, otherwise it
  // will be called as soon as new data is available.
  //
  // The end flag received by the callback indicates whether the other end has closed its end of the
  // stream, indicating that this is the last chunk of the data.
  //
  // In case of error the callback will receive an error status, the end flag is meaningless, and
  // the stream is no longer usable.
  virtual void ReadData(DataCallback callback) = 0;

  // Sends a HEADERS frames, possibly followed by one or more CONTINUATION frames, and optionally
  // closes the local end of the stream.
  virtual absl::Status SendFields(hpack::HeaderSet const& fields, bool end_stream) = 0;

  // Like `SendFields` but logs any errors and returns void.
  void SendFieldsOrLog(hpack::HeaderSet const& fields, bool end_stream);

  // Sends one or more DATA frames performing the necessary splitting automatically and optionally
  // closes the local end of the stream.
  virtual absl::Status SendData(tsdb2::net::Buffer buffer, bool end_stream) = 0;

  // Like `SendData` but logs any errors and returns void.
  void SendDataOrLog(tsdb2::net::Buffer buffer, bool end_stream);

  // Sends HEADERS and DATA frames and closes the local end of the stream.
  absl::Status SendResponse(hpack::HeaderSet const& fields, tsdb2::net::Buffer data);

  // Like `SendResponse` but logs any errors and returns void.
  void SendResponseOrLog(hpack::HeaderSet const& fields, tsdb2::net::Buffer data);

 private:
  StreamInterface(StreamInterface const&) = delete;
  StreamInterface& operator=(StreamInterface const&) = delete;
  StreamInterface(StreamInterface&&) = delete;
  StreamInterface& operator=(StreamInterface&&) = delete;
};

// Abstract interface of an HTTP/2 request handler.
class Handler {
 public:
  explicit Handler() = default;
  virtual ~Handler() = default;

  // Invoked by the server-side channel to handle a request for a particular stream. The handler
  // must respond using the methods provided by `StreamInterface`.
  virtual void operator()(StreamInterface* stream, Request const& request) = 0;

 private:
  Handler(Handler const&) = delete;
  Handler& operator=(Handler const&) = delete;
  Handler(Handler&&) = delete;
  Handler& operator=(Handler&&) = delete;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HANDLERS_H__
