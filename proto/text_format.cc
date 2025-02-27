#include "proto/text_format.h"

#include <cstddef>
#include <string_view>

#include "absl/status/status.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/proto.h"

namespace tsdb2 {
namespace proto {
namespace text {

namespace {

std::string_view constexpr kIdentifierPattern = "[A-Za-z_][A-Za-z0-9_]*";

}  // namespace

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

absl::Status Parser::Parse(BaseMessageDescriptor const& descriptor, Message* const proto) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("{"));
  ConsumeWhitespace();
  while (!ConsumePrefix("}")) {
    std::string_view field_name;
    RETURN_IF_ERROR(tsdb2::common::RE::ConsumePrefixArgs(&input_, kIdentifierPattern, &field_name));
    DEFINE_CONST_OR_RETURN(type_and_label, descriptor.GetFieldTypeAndLabel(field_name));
    auto const [field_type, field_label] = type_and_label;
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    ConsumeWhitespace();
    // TODO
  }
  return absl::OkStatus();
}

}  // namespace text
}  // namespace proto
}  // namespace tsdb2
