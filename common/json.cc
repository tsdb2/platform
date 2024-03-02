#include "common/json.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"
#include "common/to_array.h"

namespace tsdb2 {
namespace common {
namespace json {
namespace internal {

namespace {

auto constexpr kEscapeCodeByCharacter = fixed_flat_map_of<char, std::string_view>({
    {'"', "\\\""},
    {'\\', "\\\\"},
    {'\b', "\\b"},
    {'\f', "\\f"},
    {'\n', "\\n"},
    {'\r', "\\r"},
    {'\t', "\\t"},
});

auto constexpr kHighHexCodes = to_array({
    "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
    "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
    "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
    "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
    "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
    "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF",
});

auto constexpr kEscapedCharacterByCode = fixed_flat_map_of<char, char>({
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

std::string EscapeAndQuoteString(std::string_view const input) {
  size_t size = 3;  // start by counting the two quote characters and the NUL terminator
  for (char const ch : input) {
    if (ch < 0) {
      size += 6;
    } else if (kEscapeCodeByCharacter.contains(ch)) {
      size += 2;
    } else {
      ++size;
    }
  }
  std::string result;
  result.reserve(size);
  result += '"';
  for (char const ch : input) {
    if (ch < 0) {
      // TODO: we should actually implement UTF-8 to UTF-16 transcoding.
      result += "\\u00";
      result += kHighHexCodes[static_cast<uint8_t>(ch) - 128];
    } else if (auto it = kEscapeCodeByCharacter.find(ch); it != kEscapeCodeByCharacter.end()) {
      result += it->second;
    } else {
      result += ch;
    }
  }
  result += '"';
  return result;
}

absl::Status Parser::SkipStringPartial() {
  // TODO: this code is not UTF-8 aware.
  for (size_t i = 0; i < input_.size(); ++i) {
    switch (input_[i]) {
      case '\\':
        if (++i >= input_.size()) {
          return InvalidSyntaxError();
        }
        if (input_[i] != 'u') {
          if (kEscapedCharacterByCode.contains(input_[i])) {
            break;
          } else {
            return InvalidSyntaxError();
          }
        }
        if (i + 4 >= input_.size() || !IsHexDigit(input_[i + 1]) || !IsHexDigit(input_[i + 2]) ||
            !IsHexDigit(input_[i + 3]) || !IsHexDigit(input_[i + 4])) {
          return InvalidSyntaxError();
        }
        break;
      case '"':
        input_.remove_prefix(i + 1);
        return absl::OkStatus();
    }
  }
  return InvalidSyntaxError();
}

absl::Status Parser::SkipObjectPartial() {
  ConsumeWhitespace();
  if (ConsumePrefix("}")) {
    return absl::OkStatus();
  }
  RETURN_IF_ERROR(RequirePrefix("\""));
  RETURN_IF_ERROR(SkipStringPartial());
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix(":"));
  RETURN_IF_ERROR(SkipValue());
  ConsumeWhitespace();
  while (ConsumePrefix(",")) {
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix("\""));
    RETURN_IF_ERROR(SkipStringPartial());
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    RETURN_IF_ERROR(SkipValue());
    ConsumeWhitespace();
  }
  return RequirePrefix("}");
}

absl::Status Parser::SkipArrayPartial() {
  ConsumeWhitespace();
  if (ConsumePrefix("]")) {
    return absl::OkStatus();
  }
  RETURN_IF_ERROR(SkipValue());
  ConsumeWhitespace();
  while (ConsumePrefix(",")) {
    RETURN_IF_ERROR(SkipValue());
    ConsumeWhitespace();
  }
  return RequirePrefix("]");
}

absl::Status Parser::SkipValue() {
  ConsumeWhitespace();
  if (ConsumePrefix("null")) {
    return absl::OkStatus();
  }
  if (ConsumePrefix("true")) {
    return absl::OkStatus();
  }
  if (ConsumePrefix("false")) {
    return absl::OkStatus();
  }
  if (ConsumePrefix("\"")) {
    return SkipStringPartial();
  }
  if (ConsumePrefix("{")) {
    return SkipObjectPartial();
  }
  if (ConsumePrefix("[")) {
    return SkipArrayPartial();
  }
  // If none of the above prefixes were found then it must be a number.
  long double value;
  return ReadTo(&value);
}

absl::Status Parser::ReadTo(bool* const result) {
  ConsumeWhitespace();
  if (ConsumePrefix("true")) {
    *result = true;
    return absl::OkStatus();
  } else if (ConsumePrefix("false")) {
    *result = false;
    return absl::OkStatus();
  } else {
    return InvalidSyntaxError();
  }
}

absl::Status Parser::ReadTo(std::string* const result) {
  ConsumeWhitespace();
  if (!ConsumePrefix("\"")) {
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
        if (!kEscapedCharacterByCode.contains(ch)) {
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
      buffer[j] = kEscapedCharacterByCode.at(input_[i]);
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
