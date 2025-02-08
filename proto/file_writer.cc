#include "proto/file_writer.h"

#include <string>
#include <string_view>

namespace tsdb2 {
namespace proto {
namespace internal {

void FileWriter::Indent() {
  if (++indentation_level_ > indentation_cords_.size()) {
    indentation_cords_.emplace_back(std::string(indentation_level_ * kIndentWidth, ' '));
  }
}

void FileWriter::Dedent() { --indentation_level_; }

void FileWriter::AppendLine(std::string_view const line) {
  AppendIndentation();
  content_.Append(line);
  content_.Append("\n");
}

void FileWriter::AppendUnindentedLine(std::string_view const line) {
  content_.Append(line);
  content_.Append("\n");
}

void FileWriter::AppendEmptyLine() { content_.Append("\n"); }

std::string FileWriter::Finish() && { return std::string(content_.Flatten()); }

void FileWriter::AppendIndentation() {
  if (indentation_level_ > 0) {
    content_.Append(indentation_cords_[indentation_level_ - 1]);
  }
}

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2
