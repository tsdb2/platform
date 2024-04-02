#ifndef __TSDB2_TSZ_BASE_METRIC_H__
#define __TSDB2_TSZ_BASE_METRIC_H__

#include <string>
#include <string_view>

namespace tsz {
namespace internal {

class BaseMetric {
 public:
  explicit BaseMetric(std::string_view const name) : name_(name) {}

  std::string_view name() const { return name_; }

 private:
  BaseMetric(BaseMetric const &) = delete;
  BaseMetric &operator=(BaseMetric const &) = delete;
  BaseMetric(BaseMetric &&) = delete;
  BaseMetric &operator=(BaseMetric &&) = delete;

  std::string const name_;
};

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_BASE_METRIC_H__
