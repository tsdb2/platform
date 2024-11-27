#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/internal/metric.h"

namespace tsz {
namespace internal {
namespace testing {

class MockMetricManager : public MetricManager {
 public:
  ~MockMetricManager() override = default;
  MOCK_METHOD(void, DeleteMetricInternal, (std::string_view name), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace tsz
