#include "proto/text_format.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
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
  std::string_view match;
  if (!pattern.MatchPrefixArgs(input_, &match)) {
    return InvalidSyntaxError();
  }
  input_.remove_prefix(match.size());
  return match;
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

tsdb2::common::flat_set<std::string> Parser::GetRequiredFieldNames(
    BaseMessageDescriptor const& descriptor) {
  auto const required_fields = descriptor.GetRequiredFieldNames();
  tsdb2::common::flat_set<std::string> missing_required_fields;
  missing_required_fields.reserve(required_fields.size());
  for (auto const name : required_fields) {
    missing_required_fields.emplace(name);
  }
  return missing_required_fields;
}

absl::Status Parser::ParseFields(BaseMessageDescriptor const& descriptor, Message* const proto,
                                 std::optional<std::string_view> const delimiter) {
  ConsumeSeparators();
  auto missing_required_fields = GetRequiredFieldNames(descriptor);
  tsdb2::common::flat_set<std::string> parsed_fields;
  parsed_fields.reserve(descriptor.GetAllFieldNames().size());
  while (!(input_.empty() || (delimiter && absl::StartsWith(input_, *delimiter)))) {
    DEFINE_CONST_OR_RETURN(field_name, ConsumeIdentifier());
    missing_required_fields.erase(field_name);
    auto const status_or_type_kind = descriptor.GetFieldTypeAndKind(field_name);
    if (!status_or_type_kind.ok()) {
      RETURN_IF_ERROR(SkipField());
      ConsumeSeparators();
      continue;
    }
    auto const [field_type, field_kind] = status_or_type_kind.value();
    if (field_kind != BaseMessageDescriptor::FieldKind::kRepeated &&
        field_kind != BaseMessageDescriptor::FieldKind::kMap) {
      auto const [unused, inserted] = parsed_fields.emplace(field_name);
      if (!inserted) {
        return absl::FailedPreconditionError(
            absl::StrCat("non-repeated field \"", field_name, "\" specified multiple times"));
      }
    }
    ConsumeSeparators();
    if (field_type != BaseMessageDescriptor::FieldType::kSubMessageField &&
        field_type != BaseMessageDescriptor::FieldType::kMapField) {
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
  if (missing_required_fields.empty()) {
    return absl::OkStatus();
  } else {
    return absl::FailedPreconditionError(
        absl::StrCat("the following required fields are missing: ",
                     absl::StrJoin(missing_required_fields, ", ")));
  }
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
  RETURN_IF_ERROR(ParseFields(descriptor, proto, delimiter));
  ConsumeSeparators();
  return RequirePrefix(delimiter);
}

absl::Status Parser::ParseMapEntry(BaseMessageDescriptor::Map* const field) {
  auto const& entry_descriptor = field->entry_descriptor();
  auto const proto = entry_descriptor.CreateInstance();
  RETURN_IF_ERROR(ParseMessage(entry_descriptor, proto.get()));
  // TODO: insert in the map.
  return absl::OkStatus();
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
    RETURN_IF_ERROR(ConsumeIdentifier());
    RETURN_IF_ERROR(SkipField());
    ConsumeSeparators();
  }
  return absl::OkStatus();
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
      return absl::OkStatus();
    }
  }
  // All else failing, this must be a sub-message.
  return SkipSubMessage();
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2
