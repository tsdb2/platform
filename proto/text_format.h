#ifndef __TSDB2_PROTO_TEXT_FORMAT_H__
#define __TSDB2_PROTO_TEXT_FORMAT_H__

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/charconv.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/proto.h"
#include "proto/text_writer.h"

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

  bool ConsumePrefix(std::string_view const prefix) { return absl::ConsumePrefix(&input_, prefix); }

  // Consumes the specified prefix or returns a syntax error.
  absl::Status RequirePrefix(std::string_view prefix);

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
    SubMessage message;
    RETURN_IF_ERROR(Tsdb2ProtoParse(this, &message));
    ConsumeSeparators();
    RETURN_IF_ERROR(RequirePrefix(delimiter));
    return std::move(message);
  }

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  absl::StatusOr<Enum> ParseEnum() {
    Enum value;
    RETURN_IF_ERROR(Tsdb2ProtoParse(this, &value));
    return value;
  }

  absl::StatusOr<absl::Time> ParseTimestamp();
  absl::StatusOr<absl::Duration> ParseDuration();

  absl::Status SkipField();

  std::string_view remainder() && { return input_; }

  template <typename Proto>
  absl::StatusOr<Proto> ParseRoot() && {
    Proto proto;
    RETURN_IF_ERROR(Tsdb2ProtoParse(this, &proto));
    ConsumeSeparators();
    if (input_.empty()) {
      return std::move(proto);
    } else {
      return InvalidSyntaxError();
    }
  }

  template <typename Proto>
  static bool ParseFlag(std::string_view const text, absl::Nonnull<Proto*> const proto,
                        absl::Nonnull<std::string*> const error) {
    auto status_or_parsed = Parser(text, /*options=*/{}).ParseRoot<Proto>();
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

  void ConsumeWhitespace();

  absl::StatusOr<std::string_view> ConsumePattern(tsdb2::common::RE const& pattern);

  absl::Status SkipSubMessage();

  Options const options_;

  std::string_view input_;
};

template <typename Proto>
absl::StatusOr<Proto> Parse(std::string_view const text, Parser::Options const& options = {}) {
  return Parser(text, options).ParseRoot<Proto>();
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
    size_t indent_width = 2;
    Brackets brackets = Brackets::kCurly;
    FieldSeparators field_separators = FieldSeparators::kNone;
  };

  explicit Stringifier(Options const& options) : options_(options) {}

  void AppendIdentifier(std::string_view const identifier) { writer_.Append(identifier); }

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  void AppendInteger(Integer const value) {
    writer_.Append(absl::StrCat(value));
  }

  template <typename Value>
  void AppendField(std::string_view const name, Value const& value) {
    return FieldAppender<std::decay_t<Value>>{}(this, name, value);
  }

  std::string Finish() && { return std::move(writer_).Finish(); }

  template <typename Proto>
  std::string Stringify(Proto const& proto, Options const& options) {
    Stringifier stringifier{options};
    Tsdb2ProtoStringify(&stringifier, proto);
    return std::move(stringifier).Finish();
  }

  template <typename Proto>
  std::string Stringify(Proto const& proto) {
    return Stringify(proto, Options{});
  }

  template <typename Proto>
  static std::string StringifyFlag(Proto const& proto) {
    return Stringifier(Options{
                           .mode = Options::Mode::kCompressed,
                           .field_separators = Options::FieldSeparators::kComma,
                       })
        .Stringify(proto);
  }

 private:
  class Scope final {
   public:
    explicit Scope(Stringifier* const parent)
        : parent_(parent), writer_scope_(&parent_->writer_), saved_(parent_->first_field_) {
      parent_->first_field_ = true;
    }

    ~Scope() { parent_->first_field_ = saved_; }

   private:
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;

    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    Stringifier* const parent_;
    internal::TextWriter::IndentedScope writer_scope_;
    bool saved_;
  };

  template <typename Value, typename Enable = void>
  struct FieldAppender {
    void operator()(Stringifier* const stringifier, std::string_view const name,
                    Value const& value) const {
      stringifier->AppendPrimitiveField(name, value);
    }
  };

  template <typename SubMessage>
  struct FieldAppender<SubMessage, std::enable_if_t<IsProtoMessageV<SubMessage>>> {
    void operator()(Stringifier* const stringifier, std::string_view const name,
                    SubMessage const& value) const {
      stringifier->AppendSubMessageField(name, value);
    }
  };

  template <typename Enum>
  struct FieldAppender<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
    void operator()(Stringifier* const stringifier, std::string_view const name,
                    Enum const value) const {
      stringifier->AppendEnumField(name, value);
    }
  };

  template <>
  struct FieldAppender<absl::Time> {
    void operator()(Stringifier* const stringifier, std::string_view const name,
                    absl::Time const time) const {
      stringifier->AppendTimestamp(name, time);
    }
  };

  template <>
  struct FieldAppender<absl::Duration> {
    void operator()(Stringifier* const stringifier, std::string_view const name,
                    absl::Duration const duration) const {
      stringifier->AppendDuration(name, duration);
    }
  };

  template <typename Value, typename Enable = void>
  struct StringifyPrimitiveValue;

  template <typename Integer>
  struct StringifyPrimitiveValue<Integer,
                                 std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>>> {
    std::string operator()(Integer const value) const { return absl::StrCat(value); }
  };

  template <>
  struct StringifyPrimitiveValue<bool> {
    std::string operator()(bool const value) const { return value ? "true" : "false"; }
  };

  template <typename Float>
  struct StringifyPrimitiveValue<Float, std::enable_if_t<std::is_floating_point_v<Float>>> {
    std::string operator()(Float const value) const { return absl::StrCat(value); }
  };

  template <>
  struct StringifyPrimitiveValue<std::string> {
    std::string operator()(std::string_view const value) const {
      return absl::StrCat("\"", absl::CEscape(value), "\"");
    }
  };

  template <>
  struct StringifyPrimitiveValue<std::vector<uint8_t>> {
    std::string operator()(absl::Span<uint8_t const> const value) const {
      // TODO: stringify bytes fields.
      return "\"\"";
    }
  };

  template <typename Value>
  void AppendPrimitiveField(std::string_view const name, Value const& value) {
    writer_.Append(name, ": ", StringifyPrimitiveValue<std::decay_t<Value>>{}(value));
    switch (options_.field_separators) {
      case Options::FieldSeparators::kComma:
        writer_.FinishLine(",");
        break;
      case Options::FieldSeparators::kSemicolon:
        writer_.FinishLine(";");
        break;
      default:
        writer_.FinishLine();
        break;
    }
    first_field_ = false;
  }

  template <typename SubMessage, std::enable_if_t<IsProtoMessageV<SubMessage>, bool> = true>
  void AppendSubMessageField(std::string_view const name, SubMessage const& value) {
    if (options_.brackets != Options::Brackets::kCurly) {
      writer_.AppendLine(name, " <");
    } else {
      writer_.AppendLine(name, " {");
    }
    {
      Scope scope{this};
      Tsdb2ProtoStringify(this, value);
    }
    if (options_.brackets != Options::Brackets::kCurly) {
      writer_.Append(name, ">");
    } else {
      writer_.Append(name, "}");
    }
    switch (options_.field_separators) {
      case Options::FieldSeparators::kComma:
        writer_.FinishLine(",");
        break;
      case Options::FieldSeparators::kSemicolon:
        writer_.FinishLine(";");
        break;
      default:
        writer_.FinishLine();
        break;
    }
  }

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  void AppendEnumField(std::string_view const name, Enum const value) {
    writer_.Append(name, ": ");
    Tsdb2ProtoStringify(this, value);
    switch (options_.field_separators) {
      case Options::FieldSeparators::kComma:
        writer_.FinishLine(",");
        break;
      case Options::FieldSeparators::kSemicolon:
        writer_.FinishLine(";");
        break;
      default:
        writer_.FinishLine();
        break;
    }
    first_field_ = false;
  }

  void AppendTimestamp(std::string_view name, absl::Time value);
  void AppendDuration(std::string_view name, absl::Duration value);

  Options const options_;
  internal::TextWriter writer_;
  bool first_field_ = true;
};

template <typename Proto>
std::string Stringify(Proto const& proto, Stringifier::Options const& options = {}) {
  return Stringifier(options).Stringify(proto);
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_FORMAT_H__
