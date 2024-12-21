#ifndef __TSDB2_SERVER_HEALTHZ_H__
#define __TSDB2_SERVER_HEALTHZ_H__

#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/no_destructor.h"
#include "common/singleton.h"
#include "server/base_module.h"

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

class HealthzModule : public tsdb2::init::BaseModule {
 public:
  static HealthzModule *Get() { return instance_.Get(); }

  absl::Status Initialize() override;

 private:
  friend class tsdb2::common::NoDestructor<HealthzModule>;
  static tsdb2::common::NoDestructor<HealthzModule> instance_;

  explicit HealthzModule();
};

}  // namespace server
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_HEALTHZ_H__
