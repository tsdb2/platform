#ifndef __TSDB2_COMMON_ENV_H__
#define __TSDB2_COMMON_ENV_H__

#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"

namespace tsdb2 {
namespace common {

// This unit provides idiomatic and thread-safe utilities to read and manage environment variables.
//
// NOTE: all parameters for these functions are passed by const-ref to std::string rather than by
// std::string_view because their implementations need to copy the string anyway in order to obtain
// a string that is NUL-terminated and suitable for system library calls. `std::string_view::data`
// doesn't necessarily return a NUL-terminated character array.

// Retrieves an environment variable, returning an empty optional if it doesn't exist.
std::optional<std::string> GetEnv(std::string const& name);

// Sets the value of an environment variable, creating it if it doesn't exist.
absl::Status SetEnv(std::string const& name, std::string const& value);

// Updates the value of an environment variable, failing if it doesn't exist.
absl::Status SetEnvIfUnset(std::string const& name, std::string const& value);

// Removes an environment variable. Returns an error if it didn't exist.
absl::Status UnsetEnv(std::string const& name);

// Returns a dictionary of all environment variables.
absl::flat_hash_map<std::string, std::string> Environ();

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_ENV_H__
