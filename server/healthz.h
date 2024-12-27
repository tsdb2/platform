#ifndef __TSDB2_SERVER_HEALTHZ_H__
#define __TSDB2_SERVER_HEALTHZ_H__

#include <string_view>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/singleton.h"

namespace tsdb2 {
namespace server {

class Healthz final {
 public:
  using CheckFn = absl::AnyInvocable<absl::Status()>;

  static tsdb2::common::Singleton<Healthz> instance;

  void AddCheck(CheckFn check) ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status RunChecks() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  friend class HealthzTest;

  explicit Healthz() = default;

  absl::Mutex mutable mutex_;
  std::vector<CheckFn> checks_ ABSL_GUARDED_BY(mutex_);
};

struct HealthzModule {
  static std::string_view constexpr name = "healthz";
  absl::Status Initialize();
};

}  // namespace server
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_HEALTHZ_H__
