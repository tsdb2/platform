#include "common/env.h"

#include <errno.h>
#include <stdlib.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "common/no_destructor.h"
#include "common/re.h"

// See 'man 7 environ'.
extern char** environ;  // NOLINT(readability-redundant-declaration)

namespace tsdb2 {
namespace common {

namespace {

NoDestructor<RE> const kEnvironEntryPattern{RE::CreateOrDie("([^=]+)=(.*)")};

ABSL_CONST_INIT absl::Mutex global_env_mutex{absl::kConstInit};

}  // namespace

std::optional<std::string> GetEnv(std::string const& name) ABSL_LOCKS_EXCLUDED(global_env_mutex) {
  absl::MutexLock lock{&global_env_mutex};
  auto* const value = ::getenv(std::string(name).c_str());
  return value != nullptr ? std::make_optional<std::string>(value) : std::nullopt;
}

absl::Status SetEnv(std::string const& name, std::string const& value)
    ABSL_LOCKS_EXCLUDED(global_env_mutex) {
  absl::MutexLock lock{&global_env_mutex};
  int const result = ::setenv(name.c_str(), value.c_str(), /*overwrite=*/1);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, absl::StrFormat("setenv(\"%s\", \"%s\", 1)",
                                                      absl::CEscape(name), absl::CEscape(value)));
  } else {
    return absl::OkStatus();
  }
}

absl::Status SetEnvIfUnset(std::string const& name, std::string const& value)
    ABSL_LOCKS_EXCLUDED(global_env_mutex) {
  absl::MutexLock lock{&global_env_mutex};
  int const result = ::setenv(name.c_str(), value.c_str(), /*overwrite=*/0);
  if (result < 0) {
    return absl::ErrnoToStatus(errno, absl::StrFormat("setenv(\"%s\", \"%s\", 0)",
                                                      absl::CEscape(name), absl::CEscape(value)));
  } else {
    return absl::OkStatus();
  }
}

absl::Status UnsetEnv(std::string const& name) ABSL_LOCKS_EXCLUDED(global_env_mutex) {
  absl::MutexLock lock{&global_env_mutex};
  int const result = ::unsetenv(name.c_str());
  if (result < 0) {
    return absl::ErrnoToStatus(errno, absl::StrFormat("unsetenv(\"%s\")", absl::CEscape(name)));
  } else {
    return absl::OkStatus();
  }
}

absl::flat_hash_map<std::string, std::string> Environ() ABSL_LOCKS_EXCLUDED(global_env_mutex) {
  std::vector<std::string> entries;
  {
    absl::MutexLock lock{&global_env_mutex};
    for (size_t i = 0; environ[i] != nullptr; ++i) {
      entries.emplace_back(environ[i]);
    }
  }
  absl::flat_hash_map<std::string, std::string> dictionary;
  for (auto const& entry : entries) {
    std::string_view key;
    std::string_view value;
    if (kEnvironEntryPattern->MatchArgs(entry, &key, &value)) {
      dictionary.try_emplace(key, value);
    } else {
      LOG(ERROR) << "cannot parse environment variable entry: \"" << absl::CEscape(entry) << "\"";
    }
  }
  return dictionary;
}

}  // namespace common
}  // namespace tsdb2
