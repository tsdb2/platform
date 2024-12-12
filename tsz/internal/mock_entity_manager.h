#ifndef __TSDB2_TSZ_INTERNAL_MOCK_ENTITY_MANAGER_H__
#define __TSDB2_TSZ_INTERNAL_MOCK_ENTITY_MANAGER_H__

#include <string_view>

#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "tsz/internal/entity.h"
#include "tsz/internal/metric_config.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {
namespace testing {

class MockEntityManager : public tsz::internal::EntityManager {
 public:
  explicit MockEntityManager() = default;
  ~MockEntityManager() override = default;

  MOCK_METHOD(absl::StatusOr<MetricConfig const *>, GetConfigForMetric,
              (std::string_view metric_name), (const, override));

  MOCK_METHOD(bool, DeleteEntityInternal, (FieldMap const &entity_labels), (override));

 private:
  MockEntityManager(MockEntityManager const &) = delete;
  MockEntityManager &operator=(MockEntityManager const &) = delete;
  MockEntityManager(MockEntityManager &&) = delete;
  MockEntityManager &operator=(MockEntityManager &&) = delete;
};

}  // namespace testing
}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_MOCK_ENTITY_MANAGER_H__
