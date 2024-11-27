#include "json/json.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/flat_map.h"
#include "common/to_array.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace json {

namespace {

auto constexpr kEscapeCodeByCharacter = tsdb2::common::fixed_flat_map_of<char, std::string_view>({
    {'"', "\\\""},
    {'\\', "\\\\"},
    {'\b', "\\b"},
    {'\f', "\\f"},
    {'\n', "\\n"},
    {'\r', "\\r"},
    {'\t', "\\t"},
});

auto constexpr kHighHexCodes = tsdb2::common::to_array({
    "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
    "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
    "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
    "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
    "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
    "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF",
});

auto constexpr kEscapedCharacterByCode = tsdb2::common::fixed_flat_map_of<char, char>({
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

namespace internal {

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

}  // namespace internal

absl::Status Parser::ReadNull() {
  ConsumeWhitespace();
  return RequirePrefix("null");
}

absl::StatusOr<bool> Parser::ReadBoolean() {
  ConsumeWhitespace();
  if (ConsumePrefix("true")) {
    return true;
  } else if (ConsumePrefix("false")) {
    return false;
  } else {
    return InvalidSyntaxError();
  }
}

absl::StatusOr<std::string> Parser::ReadString() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("\""));
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
  std::string result;
  result.reserve(count + 1);
  for (size_t i = 0, j = 0; i < offset; ++i, ++j) {
    if (input_[i] != '\\') {
      result += input_[i];
    } else if (input_[++i] != 'u') {
      result += kEscapedCharacterByCode.at(input_[i]);
    } else {
      result += ParseHexDigit(input_[i + 3]) * 16 + ParseHexDigit(input_[i + 4]);
      i += 4;
    }
  }
  input_.remove_prefix(offset + 1);
  return std::move(result);
}

bool Parser::IsHexDigit(char const ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

uint8_t Parser::ParseHexDigit(char const ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else {
    return ch - 'a' + 10;
  }
}

absl::Status Parser::RequirePrefix(std::string_view const prefix) {
  if (ConsumePrefix(prefix)) {
    return absl::OkStatus();
  } else {
    return InvalidSyntaxError();
  }
}

absl::Status Parser::ExpectPrefix(std::string_view const prefix) {
  if (ConsumePrefix(prefix)) {
    return absl::OkStatus();
  } else {
    return InvalidFormatError();
  }
}

void Parser::ConsumeWhitespace() {
  size_t offset = 0;
  while (offset < input_.size() && IsWhitespace(input_[offset])) {
    ++offset;
  }
  input_.remove_prefix(offset);
}

absl::Status Parser::SkipString(size_t& offset) {
  auto const quote = input_[offset];
  for (++offset; offset < input_.size(); ++offset) {
    auto ch = input_[offset];
    if (ch == '\\') {
      ++offset;
      if (offset < input_.size()) {
        ch = input_[offset];
        if (ch == 'u') {
          offset += 4;
        }
      } else {
        return InvalidSyntaxError();
      }
    } else if (ch == quote) {
      return absl::OkStatus();
    }
  }
  return InvalidSyntaxError();
}

absl::Status Parser::FastSkipArray(size_t& offset) {
  for (++offset; offset < input_.size(); ++offset) {
    auto const ch = input_[offset];
    switch (ch) {
      case '\"':
      case '\'':
        RETURN_IF_ERROR(SkipString(offset));
        break;
      case '[':
        RETURN_IF_ERROR(FastSkipArray(offset));
        break;
      case '{':
        RETURN_IF_ERROR(FastSkipObject(offset));
        break;
      case ']':
        return absl::OkStatus();
    }
  }
  return InvalidSyntaxError();
}

absl::Status Parser::FastSkipObject(size_t& offset) {
  for (++offset; offset < input_.size(); ++offset) {
    auto const ch = input_[offset];
    switch (ch) {
      case '\"':
      case '\'':
        RETURN_IF_ERROR(SkipString(offset));
        break;
      case '[':
        RETURN_IF_ERROR(FastSkipArray(offset));
        break;
      case '{':
        RETURN_IF_ERROR(FastSkipObject(offset));
        break;
      case '}':
        return absl::OkStatus();
    }
  }
  return InvalidSyntaxError();
}

absl::Status Parser::FastSkipField() {
  for (size_t offset = 0; offset < input_.size(); ++offset) {
    switch (input_[offset]) {
      case '\"':
      case '\'':
        RETURN_IF_ERROR(SkipString(offset));
        break;
      case '[':
        RETURN_IF_ERROR(FastSkipArray(offset));
        break;
      case '{':
        RETURN_IF_ERROR(FastSkipObject(offset));
        break;
      case ',':
      case '}':
        input_.remove_prefix(offset);
        return absl::OkStatus();
    }
  }
  return InvalidSyntaxError();
}

absl::Status Parser::SkipStringPartial() {
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
  return std::move(ReadFloat<long double>()).status();
}

absl::Status Parser::SkipField(bool const fast) {
  if (fast) {
    return FastSkipField();
  } else {
    return SkipValue();
  }
}

void Stringifier::WriteNull() { output_.Append("null"); }

void Stringifier::WriteBoolean(bool const value) { output_.Append(value ? "true" : "false"); }

void Stringifier::WriteString(std::string_view const value) {
  output_.Append(internal::EscapeAndQuoteString(value));
}

std::string Stringifier::Finish() && { return std::string(output_.Flatten()); }

void Stringifier::Indent() {
  if (++indentation_level_ > indentation_cords_.size()) {
    indentation_cords_.emplace_back(std::string(indentation_level_ * options_.indent_width, ' '));
  }
}

void Stringifier::Dedent() { --indentation_level_; }

void Stringifier::WriteIndentation() {
  if (indentation_level_ > 0) {
    output_.Append(indentation_cords_[indentation_level_ - 1]);
  }
}

}  // namespace json
}  // namespace tsdb2

absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, bool* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadBoolean());
  return absl::OkStatus();
}

absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, std::string* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadString());
  return absl::OkStatus();
}

void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier, bool const value) {
  stringifier->WriteBoolean(value);
}

void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier, char const value[]) {
  stringifier->WriteString(value);
}

void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier, std::string_view const value) {
  stringifier->WriteString(value);
}
