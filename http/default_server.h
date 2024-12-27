#ifndef __TSDB2_HTTP_DEFAULT_SERVER_H__
#define __TSDB2_HTTP_DEFAULT_SERVER_H__

#include <memory>
#include <string_view>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/trie_map.h"
#include "http/handlers.h"
#include "http/server.h"

namespace tsdb2 {
namespace http {

// Builds the default HTTP server during initialization.
//
// The default server binds to the local address and port specified in the --local_address and
// --local_port command line flags. Module initializers depending on the `DefaultServerModule` may
// add their own path handlers.
class DefaultServerBuilder {
 public:
  static DefaultServerBuilder *Get();

  // Builds the singleton default `Server` instance.
  //
  // WARNING: don't call this function in a module initializer, otherwise the server will be built
  // before other module initializers have a chance to install their handlers!
  std::unique_ptr<Server> Build();

  absl::Status RegisterHandler(std::string_view path, std::unique_ptr<Handler> handler)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  using HandlerSet = tsdb2::common::trie_map<std::unique_ptr<Handler>>;

  explicit DefaultServerBuilder() = default;
  ~DefaultServerBuilder() = delete;

  DefaultServerBuilder(DefaultServerBuilder const &) = delete;
  DefaultServerBuilder &operator=(DefaultServerBuilder const &) = delete;
  DefaultServerBuilder(DefaultServerBuilder &&) = delete;
  DefaultServerBuilder &operator=(DefaultServerBuilder &&) = delete;

  HandlerSet ExtractHandlerSet() ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Mutex mutable mutex_;
  HandlerSet handlers_ ABSL_GUARDED_BY(mutex_);
};

struct DefaultServerModule {
  static std::string_view constexpr name = "default_server_builder";
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_DEFAULT_SERVER_H__
