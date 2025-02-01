#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "proto/plugin.h"

absl::StatusOr<std::vector<uint8_t>> ReadInput() {
  std::vector<uint8_t> buffer;
  uint8_t temp[4096];
  while (size_t bytesRead = ::fread(temp, 1, sizeof(temp), stdin)) {
    buffer.insert(buffer.end(), temp, temp + bytesRead);
  }
  if (::ferror(stdin) != 0) {
    return absl::ErrnoToStatus(errno, "fread");
  }
  return std::move(buffer);
}

int main() {
  auto const status_or_buffer = ReadInput();
  if (!status_or_buffer.ok()) {
    std::string const message{status_or_buffer.status().message()};
    ::fprintf(stderr, "Error: %s\n", message.c_str());  // NOLINT
    return 1;
  }
  auto const& buffer = status_or_buffer.value();

  // TODO

  return 0;
}
