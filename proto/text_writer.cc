#include "proto/text_writer.h"

#include <string>

namespace tsdb2 {
namespace proto {
namespace internal {

void TextWriter::Indent() {
  if (++indentation_level_ > indentation_cords_.size()) {
    indentation_cords_.emplace_back(std::string(indentation_level_ * options_.indent_width, ' '));
  }
}

void TextWriter::Dedent() { --indentation_level_; }

void TextWriter::AppendEmptyLine() {
  content_.Append("\n");
  new_line_ = true;
}

std::string TextWriter::Finish() && { return std::string(content_.Flatten()); }

void TextWriter::AppendIndentation() {
  if (new_line_ && indentation_level_ > 0) {
    content_.Append(indentation_cords_[indentation_level_ - 1]);
  }
}

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2
