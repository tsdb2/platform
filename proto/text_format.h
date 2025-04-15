#ifndef __TSDB2_PROTO_TEXT_FORMAT_H__
#define __TSDB2_PROTO_TEXT_FORMAT_H__

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/charconv.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/proto.h"
#include "proto/reflection.h"

namespace tsdb2 {
namespace proto {
namespace text {

class Parser {
 public:
  struct Options {
    bool fast_skipping = false;
  };

  static absl::Status InvalidSyntaxError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid TextFormat syntax");
  }

  static absl::Status InvalidFormatError() {
    // TODO: include row and column numbers in the error message.
    return absl::FailedPreconditionError("invalid TextFormat proto format");
  }

  explicit Parser(std::string_view const input, Options const& options)
      : options_(options), input_(input) {}

  // bool at_end() const { return input_.empty(); }

  void ConsumeSeparators();

  void ConsumeFieldSeparators();

  std::optional<std::string> ParseFieldName();

  absl::StatusOr<std::string_view> ParseIdentifier();

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<Integer> ParseInteger() {
    ConsumeSeparators();
    bool negative = false;
    if constexpr (std::is_signed_v<Integer>) {
      if (ConsumePrefix("-")) {
        negative = true;
        ConsumeSeparators();
      }
    }
    std::string_view number;
    Integer result{};
    if (kHexPattern->MatchPrefixArgs(input_, &number)) {
      auto const [ptr, ec] =
          std::from_chars(number.data() + 2, number.data() + number.size(), result, /*base=*/16);
      if (ec != std::errc()) {
        return InvalidFormatError();
      }
    } else if (kFloatPattern->MatchPrefixArgs(input_, &number)) {
      if (kOctalPattern->Test(number)) {
        auto const [ptr, ec] =
            std::from_chars(number.data(), number.data() + number.size(), result, /*base=*/8);
        if (ec != std::errc()) {
          return InvalidFormatError();
        }
      } else if (kIntegerPattern->Test(number)) {
        auto const [ptr, ec] =
            std::from_chars(number.data(), number.data() + number.size(), result, /*base=*/10);
        if (ec != std::errc()) {
          return InvalidFormatError();
        }
      } else {
        return InvalidFormatError();
      }
    } else if (kOctalPattern->MatchPrefixArgs(input_, &number)) {
      auto const [ptr, ec] =
          std::from_chars(number.data(), number.data() + number.size(), result, /*base=*/8);
      if (ec != std::errc()) {
        return InvalidFormatError();
      }
    } else if (kIntegerPattern->MatchPrefixArgs(input_, &number)) {
      auto const [ptr, ec] =
          std::from_chars(number.data(), number.data() + number.size(), result, /*base=*/10);
      if (ec != std::errc()) {
        return InvalidFormatError();
      }
    } else {
      return InvalidFormatError();
    }
    input_.remove_prefix(number.size());
    if constexpr (std::is_signed_v<Integer>) {
      if (negative) {
        return -result;
      }
    }
    return result;
  }

  absl::StatusOr<bool> ParseBoolean();

  absl::StatusOr<std::string> ParseString();
  absl::StatusOr<std::vector<uint8_t>> ParseBytes();

  template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
  absl::StatusOr<Float> ParseFloat() {
    ConsumeSeparators();
    DEFINE_VAR_OR_RETURN(number, ConsumePattern(*kFloatPattern));
    char const last = number.back();
    if (last != 'F' && last != 'f') {
      if constexpr (std::is_same_v<float, Float>) {
        return absl::FailedPreconditionError("float value expected but double found");
      }
    } else {
      number.remove_suffix(1);
    }
    Float result{};
    auto const [ptr, ec] = absl::from_chars(number.data(), number.data() + number.size(), result);
    if (ec != std::errc()) {
      return InvalidFormatError();
    }
    return result;
  }

  template <typename SubMessage, std::enable_if_t<IsProtoMessageV<Message>, bool> = true>
  absl::StatusOr<SubMessage> ParseSubMessage() {
    ConsumeSeparators();
    std::string_view delimiter;
    if (ConsumePrefix("{")) {
      delimiter = "}";
    } else if (ConsumePrefix("<")) {
      delimiter = ">";
    } else {
      return InvalidSyntaxError();
    }
    DEFINE_VAR_OR_RETURN(proto, SubMessage::Parse(&input_));
    RETURN_IF_ERROR(RequirePrefix(delimiter));
    return std::move(proto);
  }

  absl::StatusOr<absl::Time> ParseTimestamp();
  absl::StatusOr<absl::Duration> ParseDuration();

  absl::Status SkipField();

  std::string_view remainder() && { return input_; }

  template <typename Message, std::enable_if_t<IsProtoMessageV<Message>, bool> = true>
  absl::StatusOr<Message> ParseRoot() && {
    DEFINE_VAR_OR_RETURN(proto, Message::Parse(&input_));
    ConsumeSeparators();
    if (input_.empty()) {
      return std::move(proto);
    } else {
      return InvalidSyntaxError();
    }
  }

  template <typename Message, std::enable_if_t<IsProtoMessageV<Message>, bool> = true>
  static bool ParseFlag(std::string_view const text, absl::Nonnull<Message*> const proto,
                        absl::Nonnull<std::string*> const error) {
    auto status_or_parsed = Parser(text, /*options=*/{}).ParseRoot<Message>();
    if (status_or_parsed.ok()) {
      *proto = std::move(status_or_parsed).value();
      return true;
    } else {
      *error = status_or_parsed.status().ToString();
      return false;
    }
  }

 private:
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kIdentifierPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kStringPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kIntegerPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kHexPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kOctalPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kFloatPattern;

  bool ConsumePrefix(std::string_view const prefix) { return absl::ConsumePrefix(&input_, prefix); }

  // Consumes the specified prefix or returns a syntax error.
  absl::Status RequirePrefix(std::string_view prefix);

  void ConsumeWhitespace();

  absl::StatusOr<std::string_view> ConsumePattern(tsdb2::common::RE const& pattern);

  absl::Status SkipSubMessage();

  Options const options_;

  std::string_view input_;
};

template <typename Message, std::enable_if_t<IsProtoMessageV<Message>, bool> = true>
absl::StatusOr<Message> Parse(std::string_view const text, Parser::Options const& options = {}) {
  return Parser(text, options).ParseRoot<Message>();
}

class Stringifier {
 public:
  // Stringification options.
  struct Options {
    enum class Mode {
      kPretty = 0,
      kOneLine = 1,
      kCompressed = 2,
    };

    enum class Brackets { kCurly = 0, kAngle = 1 };

    enum class FieldSeparators {
      kNone = 0,
      kSemicolon = 1,
      kComma = 2,
    };

    Mode mode = Mode::kPretty;
    Brackets brackets = Brackets::kCurly;
    FieldSeparators field_separators = FieldSeparators::kNone;
  };

  explicit Stringifier(Options const& options) : options_(options) {}

  template <typename Message,
            std::enable_if_t<IsProtoMessageV<Message> && HasProtoReflectionV<Message>, bool> = true>
  std::string Stringify(Message const& value) {
    // TODO
    return "";
  }

  template <typename Enum,
            std::enable_if_t<std::is_enum_v<Enum> && HasProtoReflectionV<Enum>, bool> = true>
  std::string Stringify(Enum const value) {
    auto const status_or_name = GetEnumDescriptor<Enum>().GetValueName(value);
    if (status_or_name.ok()) {
      return std::string(status_or_name.value());
    } else {
      return absl::StrCat(tsdb2::util::to_underlying(value));
    }
  }

 private:
  Options const options_;
};

}  // namespace text
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_FORMAT_H__
