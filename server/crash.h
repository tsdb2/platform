#ifndef __TSDB2_SERVER_CRASH_H__
#define __TSDB2_SERVER_CRASH_H__

namespace tsdb2 {
namespace init {

void InstallCrashHandler();

class CrashHandlerInstaller final {
 public:
  explicit CrashHandlerInstaller();
};

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_CRASH_H__
