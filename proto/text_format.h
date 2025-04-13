#ifndef __TSDB2_PROTO_TEXT_FORMAT_H__
#define __TSDB2_PROTO_TEXT_FORMAT_H__

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/charconv.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/duration.pb.sync.h"
#include "proto/proto.h"
#include "proto/reflection.h"
#include "proto/time_util.h"
#include "proto/timestamp.pb.sync.h"

namespace tsdb2 {
namespace proto {
namespace text {

class Parser {
 public:
  explicit Parser(std::string_view const input) : input_(input) {}

  template <typename Message,
            std::enable_if_t<IsProtoMessageV<Message> && HasProtoReflectionV<Message>, bool> = true>
  absl::StatusOr<Message> Parse() {
    Message proto;
    RETURN_IF_ERROR(ParseFields(Message::MESSAGE_DESCRIPTOR, &proto));
    ConsumeSeparators();
    if (!input_.empty()) {
      return InvalidSyntaxError();
    }
    return std::move(proto);
  }

  template <typename Enum,
            std::enable_if_t<std::is_enum_v<Enum> && HasProtoReflectionV<Enum>, bool> = true>
  absl::StatusOr<Enum> Parse() {
    DEFINE_CONST_OR_RETURN(value_name, ParseEnum());
    ConsumeSeparators();
    if (!input_.empty()) {
      return InvalidSyntaxError();
    }
    DEFINE_CONST_OR_RETURN(value, GetEnumDescriptor<Enum>().GetValueForName(value_name));
    return static_cast<Enum>(value);
  }

 private:
  template <typename Value, typename Enable = void>
  struct ValueParser;

  template <typename Integer>
  struct ValueParser<Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>>> {
    absl::StatusOr<Integer> operator()(Parser* const parent) const {
      return parent->ParseInteger<Integer>();
    }
  };

  template <>
  struct ValueParser<bool> {
    absl::StatusOr<bool> operator()(Parser* const parent) const { return parent->ParseBoolean(); }
  };

  template <>
  struct ValueParser<std::string> {
    absl::StatusOr<std::string> operator()(Parser* const parent) const {
      return parent->ParseString();
    }
  };

  template <>
  struct ValueParser<std::vector<uint8_t>> {
    absl::StatusOr<std::vector<uint8_t>> operator()(Parser* const parent) const {
      return parent->ParseBytes();
    }
  };

  template <typename Float>
  struct ValueParser<Float, std::enable_if_t<std::is_floating_point_v<Float>>> {
    absl::StatusOr<bool> operator()(Parser* const parent) const {
      return parent->ParseFloat<Float>();
    }
  };

  template <>
  struct ValueParser<absl::Time> {
    absl::StatusOr<absl::Time> operator()(Parser* const parent) const {
      ::google::protobuf::Timestamp proto;
      RETURN_IF_ERROR(
          parent->ParseMessage(::google::protobuf::Timestamp::MESSAGE_DESCRIPTOR, &proto));
      return DecodeGoogleApiProto(proto);
    }
  };

  template <>
  struct ValueParser<absl::Duration> {
    absl::StatusOr<absl::Duration> operator()(Parser* const parent) const {
      ::google::protobuf::Duration proto;
      RETURN_IF_ERROR(
          parent->ParseMessage(::google::protobuf::Duration::MESSAGE_DESCRIPTOR, &proto));
      return DecodeGoogleApiProto(proto);
    }
  };

  class FieldParser final {
   public:
    explicit FieldParser(Parser* const parent) : parent_(parent) {}

    template <typename Value>
    absl::Status operator()(Value* const field) const {
      DEFINE_CONST_OR_RETURN(value, parent_->ParseValue<Value>());
      *field = std::move(value);
      return absl::OkStatus();
    }

    template <typename Value>
    absl::Status operator()(std::optional<Value>* const field) const {
      DEFINE_CONST_OR_RETURN(value, parent_->ParseValue<Value>());
      field->emplace(std::move(value));
      return absl::OkStatus();
    }

    template <typename Element>
    absl::Status operator()(std::vector<Element>* const field) const {
      DEFINE_VAR_OR_RETURN(value, parent_->ParseValue<Element>());
      field->emplace_back(std::move(value));
      return absl::OkStatus();
    }

    absl::Status operator()(BaseMessageDescriptor::RawEnum& field) const {
      DEFINE_CONST_OR_RETURN(name, parent_->ParseEnum());
      return field.SetValue(name);
    }

    absl::Status operator()(BaseMessageDescriptor::OptionalEnum& field) const {
      DEFINE_CONST_OR_RETURN(name, parent_->ParseEnum());
      return field.SetValue(name);
    }

    absl::Status operator()(BaseMessageDescriptor::RepeatedEnum& field) const {
      DEFINE_CONST_OR_RETURN(name, parent_->ParseEnum());
      return field.AppendValue(name);
    }

    absl::Status operator()(BaseMessageDescriptor::RawSubMessage& field) const {
      return parent_->ParseMessage(field.descriptor(), field.mutable_message());
    }

    absl::Status operator()(BaseMessageDescriptor::OptionalSubMessage& field) const {
      return parent_->ParseMessage(field.descriptor(), field.Reset());
    }

    absl::Status operator()(BaseMessageDescriptor::RepeatedSubMessage& field) const {
      return parent_->ParseMessage(field.descriptor(), field.Append());
    }

    absl::Status operator()(BaseMessageDescriptor::Map& field) const {
      return parent_->ParseMap(&field);
    }

    absl::Status operator()(BaseMessageDescriptor::OneOf& field) const {
      // TODO: implement parsing of oneof-grouped fields.
      return absl::UnimplementedError("oneof");
    }

   private:
    Parser* const parent_;
  };

  static absl::Status InvalidSyntaxError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid TextFormat syntax");
  }

  static absl::Status InvalidFormatError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid TextFormat proto format");
  }

  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kFieldSeparatorPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kIdentifierPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kIntegerPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kHexPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kOctalPattern;
  static tsdb2::common::NoDestructor<tsdb2::common::RE> const kFloatPattern;

  bool ConsumePrefix(std::string_view const prefix) { return absl::ConsumePrefix(&input_, prefix); }

  // Consumes the specified prefix or returns a syntax error.
  absl::Status RequirePrefix(std::string_view prefix);

  // Consumes the specified prefix or returns a format error.
  absl::Status ExpectPrefix(std::string_view prefix);

  void ConsumeWhitespace();

  // Consumes whitespace and comments.
  void ConsumeSeparators();

  absl::StatusOr<std::string_view> ConsumePattern(tsdb2::common::RE const& pattern);
  absl::StatusOr<std::string_view> ConsumeIdentifier();

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
        return absl::InvalidArgumentError("float value expected but double found");
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

  template <typename Value>
  absl::StatusOr<Value> ParseValue() {
    return ValueParser<Value>{}(this);
  }

  absl::StatusOr<std::string_view> ParseEnum();

  absl::Status ParseFields(BaseMessageDescriptor const& descriptor, Message* proto);

  absl::Status ParseMessage(BaseMessageDescriptor const& descriptor, Message* proto);
  absl::Status ParseMap(BaseMessageDescriptor::Map* field);

  std::string_view input_;
};

template <typename Proto>
inline absl::StatusOr<Proto> Parse(std::string_view const text) {
  return Parser{text}.Parse<Proto>();
}

class Stringifier {
 public:
  struct Options {
    bool compressed = false;
  };

  explicit Stringifier(Options options) : options_(options) {}

  template <typename Proto>
  std::string Stringify(Proto const& proto) {
    // TODO
    return "";
  }

 private:
  Options const options_;
};

template <typename Proto>
inline std::string Stringify(Proto const& proto, Stringifier::Options const& options = {}) {
  return Stringifier{options}.Stringify(proto);
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_FORMAT_H__
