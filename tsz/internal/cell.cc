#include "tsz/internal/cell.h"

#include <algorithm>
#include <variant>

#include "absl/functional/overload.h"
#include "absl/time/time.h"

namespace tsz {
namespace internal {

void Cell::swap(Cell &other) {
  using std::swap;  // ensure ADL
  swap(metric_fields_, other.metric_fields_);
  swap(hash_, other.hash_);
  swap(value_, other.value_);
  swap(start_time_, other.start_time_);
  swap(last_update_time_, other.last_update_time_);
}

void Cell::Reset(absl::Time const new_start_time) {
  std::visit(absl::Overload{
                 [](bool &value) { value = false; },
                 [](int64_t &value) { value = 0; },
                 [](double &value) { value = 0.0; },
                 [](std::string &value) { value.clear(); },
                 [](Distribution &value) { value.Clear(); },
             },
             value_);
  start_time_ = new_start_time;
  last_update_time_ = new_start_time;
}

}  // namespace internal
}  // namespace tsz
