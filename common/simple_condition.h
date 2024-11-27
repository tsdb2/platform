#ifndef __TSDB2_COMMON_SIMPLE_CONDITION_H__
#define __TSDB2_COMMON_SIMPLE_CONDITION_H__

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

// An `absl::Condition` that takes a single `AnyInvocable` argument, and is therefore easier to use
// than `absl::Condition` itself.
//
// Without `SimpleCondition`:
//
//   bool condition = false;
//   mutex.Await(absl::Condition(+[] (bool const* const condition) {
//     return *condition;
//   }, &condition));
//
// With `SimpleCondition`:
//
//   bool condition = false;
//   mutex.Await(SimpleCondition([&] {
//     return condition;
//   }));
//
class SimpleCondition : public absl::Condition {
 public:
  explicit SimpleCondition(absl::AnyInvocable<bool() const> callback)
      : absl::Condition(this, &SimpleCondition::Run), callback_(std::move(callback)) {}

 private:
  SimpleCondition(SimpleCondition const &) = delete;
  SimpleCondition &operator=(SimpleCondition const &) = delete;
  SimpleCondition(SimpleCondition &&) = delete;
  SimpleCondition &operator=(SimpleCondition &&) = delete;

  bool Run() const { return callback_(); }

  absl::AnyInvocable<bool() const> callback_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_SIMPLE_CONDITION_H__
