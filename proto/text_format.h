#ifndef __TSDB2_PROTO_TEXT_FORMAT_H__
#define __TSDB2_PROTO_TEXT_FORMAT_H__

#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/strip.h"
#include "common/utilities.h"
#include "proto/proto.h"

namespace tsdb2 {
namespace proto {
namespace text {

class Parser {
 public:
  explicit Parser(std::string_view const input) : input_(input) {}

  template <typename Message,
            std::enable_if_t<
                std::is_base_of_v<BaseMessageDescriptor, decltype(Message::MESSAGE_DESCRIPTOR)>,
                bool> = true>
  absl::StatusOr<Message> Parse() {
    Message proto;
    RETURN_IF_ERROR(Parse(Message::MESSAGE_DESCRIPTOR, &proto));
    return std::move(proto);
  }

 private:
  static absl::Status InvalidSyntaxError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid TextFormat syntax");
  }

  static bool IsWhitespace(char const ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
  }

  bool at_end() const { return input_.empty(); }

  bool ConsumePrefix(std::string_view const prefix) { return absl::ConsumePrefix(&input_, prefix); }

  // Consumes the specified prefix or returns a syntax error.
  absl::Status RequirePrefix(std::string_view prefix);

  void ConsumeWhitespace();

  absl::Status Parse(BaseMessageDescriptor const& descriptor, Message* proto);

  std::string_view input_;
};

class Stringifier {
 public:
  // TODO
};

}  // namespace text
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_FORMAT_H__
