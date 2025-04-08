#include "proto/text_format.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/proto.h"
#include "proto/reflection.h"

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

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kFieldSeparatorPattern{
    tsdb2::common::RE::CreateOrDie("([;,]?)")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const Parser::kIdentifierPattern{
    tsdb2::common::RE::CreateOrDie("([A-Za-z_][A-Za-z0-9_]*)")};

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

absl::StatusOr<std::string_view> Parser::ConsumePattern(tsdb2::common::RE const& pattern) {
  std::string_view number;
  if (!pattern.MatchPrefixArgs(input_, &number)) {
    return InvalidSyntaxError();
  }
  input_.remove_prefix(number.size());
  return number;
}

absl::StatusOr<std::string_view> Parser::ConsumeIdentifier() {
  return ConsumePattern(*kIdentifierPattern);
}

absl::StatusOr<bool> Parser::ParseBoolean() {
  ConsumeSeparators();
  DEFINE_CONST_OR_RETURN(word, ConsumeIdentifier());
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

absl::StatusOr<std::string_view> Parser::ParseEnum() {
  ConsumeSeparators();
  return ConsumeIdentifier();
}

absl::StatusOr<std::vector<std::string_view>> Parser::ParseEnumArray() {
  std::vector<std::string_view> values;
  ConsumeSeparators();
  RETURN_IF_ERROR(ExpectPrefix("["));
  ConsumeSeparators();
  if (!ConsumePrefix("]")) {
    DEFINE_CONST_OR_RETURN(value, ParseEnum());
    values.emplace_back(value);
    ConsumeSeparators();
  }
  while (!ConsumePrefix("]")) {
    RETURN_IF_ERROR(RequirePrefix(","));
    ConsumeSeparators();
    DEFINE_CONST_OR_RETURN(value, ParseEnum());
    values.emplace_back(value);
    ConsumeSeparators();
  }
  return std::move(values);
}

absl::Status Parser::ParseFields(BaseMessageDescriptor const& descriptor, Message* const proto) {
  ConsumeSeparators();
  tsdb2::common::flat_set<std::string> parsed_fields;
  parsed_fields.reserve(descriptor.GetAllFieldNames().size());
  while (!input_.empty()) {
    DEFINE_CONST_OR_RETURN(field_name, ConsumeIdentifier());
    auto const [unused, inserted] = parsed_fields.emplace(field_name);
    if (!inserted) {
      return absl::InvalidArgumentError(
          absl::StrCat("field \"", field_name, "\" specified multiple times"));
    }
    ConsumeSeparators();
    DEFINE_CONST_OR_RETURN(type_and_kind, descriptor.GetFieldTypeAndKind(field_name));
    auto const [field_type, field_kind] = type_and_kind;
    if (field_type != BaseMessageDescriptor::FieldType::kSubMessageField ||
        field_kind == BaseMessageDescriptor::FieldKind::kRepeated) {
      RETURN_IF_ERROR(RequirePrefix(":"));
    } else {
      ConsumePrefix(":");
    }
    DEFINE_VAR_OR_RETURN(field, descriptor.GetFieldValue(proto, field_name));
    RETURN_IF_ERROR(std::visit(FieldParser(this), field));
    ConsumeSeparators();
    RETURN_IF_ERROR(ConsumePattern(*kFieldSeparatorPattern));
    ConsumeSeparators();
  }
  return absl::OkStatus();
}

absl::Status Parser::ParseMessage(BaseMessageDescriptor const& descriptor, Message* const proto) {
  ConsumeSeparators();
  std::string_view delimiter;
  if (ConsumePrefix("{")) {
    delimiter = "}";
  } else if (ConsumePrefix("<")) {
    delimiter = ">";
  } else {
    return InvalidSyntaxError();
  }
  RETURN_IF_ERROR(ParseFields(descriptor, proto));
  ConsumeSeparators();
  return RequirePrefix(delimiter);
}

absl::Status Parser::ParseMessageArray(BaseMessageDescriptor::RepeatedSubMessage* const field) {
  field->Clear();
  ConsumeSeparators();
  RETURN_IF_ERROR(ExpectPrefix("["));
  ConsumeSeparators();
  auto const& descriptor = field->descriptor();
  if (!ConsumePrefix("]")) {
    RETURN_IF_ERROR(ParseMessage(descriptor, field->Append()));
    ConsumeSeparators();
  }
  while (!ConsumePrefix("]")) {
    RETURN_IF_ERROR(RequirePrefix(","));
    ConsumeSeparators();
    RETURN_IF_ERROR(ParseMessage(descriptor, field->Append()));
    ConsumeSeparators();
  }
  return absl::OkStatus();
}

absl::Status Parser::ParseMap(BaseMessageDescriptor::Map* const field) {
  field->Clear();
  ConsumeSeparators();
  // TODO
  return absl::OkStatus();
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2
