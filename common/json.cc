#include "common/json.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace common {
namespace json {
namespace internal {

namespace {

auto constexpr kEscapeSequences = fixed_flat_map_of<char, char>({
    {'"', '"'},
    {'\\', '\\'},
    {'/', '/'},
    {'b', '\b'},
    {'f', '\f'},
    {'n', '\n'},
    {'r', '\r'},
    {'t', '\t'},
});

}  // namespace

absl::Status Parser::ReadTo(bool* const result) {
  ConsumeWhitespace();
  if (absl::ConsumePrefix(&input_, "true")) {
    *result = true;
    return absl::OkStatus();
  } else if (absl::ConsumePrefix(&input_, "false")) {
    *result = false;
    return absl::OkStatus();
  } else {
    return InvalidSyntaxError();
  }
}

absl::Status Parser::ReadTo(std::string* const result) {
  ConsumeWhitespace();
  if (!absl::ConsumePrefix(&input_, "\"")) {
    return InvalidSyntaxError();
  }
  size_t offset = 0;
  size_t count = 0;
  while (offset < input_.size() && input_[offset] != '"') {
    ++count;
    if (input_[offset++] == '\\') {
      if (offset >= input_.size()) {
        return InvalidSyntaxError();
      }
      char const ch = input_[offset++];
      if (ch != 'u') {
        if (!kEscapeSequences.contains(ch)) {
          return InvalidSyntaxError();
        }
      } else {
        if (offset + 4 >= input_.size()) {
          return InvalidSyntaxError();
        }
        if (input_[offset] != '0' || input_[offset + 1] != '0') {
          // TODO: implement UTF-8 encoding.
          return absl::UnimplementedError("UTF-8 encoding not implemented");
        }
        if (!IsHexDigit(input_[offset + 2]) || !IsHexDigit(input_[offset + 3])) {
          return InvalidSyntaxError();
        }
        offset += 4;
      }
    }
  }
  if (offset >= input_.size() || input_[offset] != '"') {
    return InvalidSyntaxError();
  }
  auto const buffer = new char[count + 1];
  for (size_t i = 0, j = 0; i < offset; ++i, ++j) {
    if (input_[i] != '\\') {
      buffer[j] = input_[i];
    } else if (input_[++i] != 'u') {
      buffer[j] = kEscapeSequences.at(input_[i]);
    } else {
      buffer[j] = ParseHexDigit(input_[i + 3]) * 16 + ParseHexDigit(input_[i + 4]);
      i += 4;
    }
  }
  input_.remove_prefix(offset + 1);
  *result = std::string(
      absl::MakeCordFromExternal(std::string_view(buffer, count), [buffer] { delete[] buffer; }));
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace json
}  // namespace common
}  // namespace tsdb2
