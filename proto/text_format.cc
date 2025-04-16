#include "proto/text_format.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "common/flat_map.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/duration.pb.sync.h"
#include "proto/time_util.h"
#include "proto/timestamp.pb.sync.h"

namespace tsdb2 {
namespace proto {
namespace text {

namespace {

auto constexpr kEscapedCharacterByCode = tsdb2::common::fixed_flat_map_of<char, char>({
    {'a', 7},
    {'b', 8},
    {'f', 12},
    {'n', 10},
    {'r', 13},
    {'t', 9},
    {'v', 11},
    {'?', 63},
    {'\\', 92},
    {'\'', 39},
    {'\"', 34},
});

bool IsWhitespace(char const ch) {
  return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}

bool IsHexDigit(char const ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
}

uint8_t ParseHexDigit(char const ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else {
    return ch - 'a' + 10;
  }
}

}  // namespace

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kIdentifierPattern{
    tsdb2::common::RE::CreateOrDie("([A-Za-z_][A-Za-z0-9_]*)")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kStringPattern{
    tsdb2::common::RE::CreateOrDie("(\"((?:[^\"]|\\\\\")*)\")")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kIntegerPattern{
    tsdb2::common::RE::CreateOrDie("(0|[1-9][0-9]*)")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kHexPattern{
    tsdb2::common::RE::CreateOrDie("(0[Xx][0-9A-Fa-f]+)")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kOctalPattern{
    tsdb2::common::RE::CreateOrDie("(0[0-7]+)")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kFloatPattern{
    tsdb2::common::RE::CreateOrDie("((?:[0-9]*\\.)?[0-9]+(?:[Ee][+-]?[0-9]+)?[Ff]?)")};

absl::Status Parser::RequirePrefix(std::string_view const prefix) {
  if (ConsumePrefix(prefix)) {
    return absl::OkStatus();
  } else {
    return InvalidSyntaxError();
  }
}

void Parser::ConsumeSeparators() {
  ConsumeWhitespace();
  while (ConsumePrefix("#")) {
    size_t comment_length = 0;
    while (comment_length < input_.size() && input_[comment_length] != '\n') {
      ++comment_length;
    }
    input_.remove_prefix(comment_length);
    ConsumeWhitespace();
  }
}

void Parser::ConsumeFieldSeparators() {
  ConsumeSeparators();
  ConsumePrefix(",") || ConsumePrefix(";");
}

std::optional<std::string> Parser::ParseFieldName() {
  ConsumeSeparators();
  std::string_view name;
  if (kIdentifierPattern->MatchPrefixArgs(input_, &name)) {
    input_.remove_prefix(name.size());
    return std::string(name);
  } else {
    return std::nullopt;
  }
}

absl::StatusOr<std::string_view> Parser::ParseIdentifier() {
  ConsumeSeparators();
  return ConsumePattern(*kIdentifierPattern);
}

absl::StatusOr<bool> Parser::ParseBoolean() {
  ConsumeSeparators();
  DEFINE_CONST_OR_RETURN(word, ParseIdentifier());
  if (word == "true") {
    return true;
  } else if (word == "false") {
    return false;
  } else {
    return InvalidFormatError();
  }
}

absl::StatusOr<std::string> Parser::ParseString() {
  ConsumeSeparators();
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
      result += static_cast<char>(ParseHexDigit(input_[i + 3]) * 16 + ParseHexDigit(input_[i + 4]));
      i += 4;
    }
  }
  input_.remove_prefix(offset + 1);
  return std::move(result);
}

absl::StatusOr<std::vector<uint8_t>> Parser::ParseBytes() {
  DEFINE_CONST_OR_RETURN(string, ParseString());
  return std::vector<uint8_t>{string.begin(), string.end()};
}

absl::StatusOr<absl::Time> Parser::ParseTimestamp() {
  DEFINE_CONST_OR_RETURN(proto, ParseSubMessage<google::protobuf::Timestamp>());
  return DecodeGoogleApiProto(proto);
}

absl::StatusOr<absl::Duration> Parser::ParseDuration() {
  DEFINE_CONST_OR_RETURN(proto, ParseSubMessage<google::protobuf::Duration>());
  return DecodeGoogleApiProto(proto);
}

absl::Status Parser::SkipField() {
  ConsumeSeparators();
  if (!ConsumePrefix(":")) {
    return SkipSubMessage();
  }
  ConsumeSeparators();
  std::string_view prefix;
  for (auto const& pattern : {
           *kIdentifierPattern,
           *kStringPattern,
           *kIntegerPattern,
           *kHexPattern,
           *kOctalPattern,
           *kFloatPattern,
       }) {
    if (pattern.MatchPrefixArgs(input_, &prefix)) {
      input_.remove_prefix(prefix.size());
      ConsumeFieldSeparators();
      return absl::OkStatus();
    }
  }
  // All else failing, this must be a sub-message.
  return SkipSubMessage();
}

void Parser::ConsumeWhitespace() {
  size_t offset = 0;
  while (offset < input_.size() && IsWhitespace(input_[offset])) {
    ++offset;
  }
  input_.remove_prefix(offset);
}

absl::StatusOr<std::string_view> Parser::ConsumePattern(tsdb2::common::RE const& pattern) {
  std::string_view match;
  if (!pattern.MatchPrefixArgs(input_, &match)) {
    return InvalidSyntaxError();
  }
  input_.remove_prefix(match.size());
  return match;
}

absl::Status Parser::SkipSubMessage() {
  ConsumeSeparators();
  std::string_view delimiter;
  if (ConsumePrefix("{")) {
    delimiter = "}";
  } else if (ConsumePrefix("<")) {
    delimiter = ">";
  } else {
    return InvalidSyntaxError();
  }
  ConsumeSeparators();
  while (!ConsumePrefix(delimiter)) {
    RETURN_IF_ERROR(ParseIdentifier());
    RETURN_IF_ERROR(SkipField());
    ConsumeSeparators();
  }
  ConsumeFieldSeparators();
  return absl::OkStatus();
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2
