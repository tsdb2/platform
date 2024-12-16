#ifndef __TSDB2_HTTP_HANDLERS_H__
#define __TSDB2_HTTP_HANDLERS_H__

#include "http/hpack.h"
#include "http/http.h"
#include "io/cord.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

class StreamInterface {
 public:
  explicit StreamInterface() = default;
  virtual ~StreamInterface() = default;

  virtual tsdb2::io::Cord ReadData() = 0;
  virtual void SendFields(hpack::HeaderSet const& fields, bool end_stream) = 0;
  virtual void SendData(tsdb2::net::Buffer buffer, bool end_stream) = 0;

 private:
  StreamInterface(StreamInterface const&) = delete;
  StreamInterface& operator=(StreamInterface const&) = delete;
  StreamInterface(StreamInterface&&) = delete;
  StreamInterface& operator=(StreamInterface&&) = delete;
};

class Handler {
 public:
  explicit Handler() = default;
  virtual ~Handler() = default;

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
