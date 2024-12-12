#include <string_view>

#include "gmock/gmock.h"
#include "tsz/internal/metric.h"

namespace tsz {
namespace internal {
namespace testing {

class MockMetricManager : public MetricManager {
 public:
  explicit MockMetricManager() = default;
  ~MockMetricManager() override = default;

  MOCK_METHOD(void, DeleteMetricInternal, (std::string_view name), (override));

 private:
  MockMetricManager(MockMetricManager const &) = delete;
  MockMetricManager &operator=(MockMetricManager const &) = delete;
  MockMetricManager(MockMetricManager &&) = delete;
  MockMetricManager &operator=(MockMetricManager &&) = delete;
};

}  // namespace testing
}  // namespace internal
}  // namespace tsz
