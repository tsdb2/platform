#include "common/re.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/re/automaton.h"
#include "common/re/parser.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

bool RE::Test(std::string_view const input, std::string_view const pattern) {
  auto const status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    return status_or_re->Test(input);
  } else {
    return false;
  }
}

absl::StatusOr<std::vector<std::string>> RE::Match(std::string_view const input,
                                                   std::string_view const pattern) {
  auto status_or_re = RE::Create(pattern);
  if (status_or_re.ok()) {
    auto maybe_results = status_or_re->Match(input);
    if (maybe_results) {
      return std::move(maybe_results).value();
    } else {
      return absl::NotFoundError(input);
    }
  } else {
    return std::move(status_or_re).status();
  }
}

absl::StatusOr<RE> RE::Create(std::string_view const pattern) {
  ASSIGN_VAR_OR_RETURN(reffed_ptr<regexp_internal::AutomatonInterface>, automaton,
                       regexp_internal::Parse(pattern));
  return RE(std::move(automaton));
}

}  // namespace common
}  // namespace tsdb2
