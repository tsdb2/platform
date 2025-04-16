#ifndef __TSDB2_PROTO_TEXT_WRITER_H__
#define __TSDB2_PROTO_TEXT_WRITER_H__

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"

namespace tsdb2 {
namespace proto {
namespace internal {

class TextWriter {
 public:
  struct Options {
    size_t indent_width = 2;
  };

  class IndentedScope final {
   public:
    explicit IndentedScope(TextWriter* const parent) : parent_(parent) { parent_->Indent(); }
    ~IndentedScope() { parent_->Dedent(); }

   private:
    IndentedScope(IndentedScope const&) = delete;
    IndentedScope& operator=(IndentedScope const&) = delete;
    IndentedScope(IndentedScope&&) = delete;
    IndentedScope& operator=(IndentedScope&&) = delete;

    TextWriter* const parent_;
  };

  explicit TextWriter(Options const& options) : options_(options) {}
  explicit TextWriter() : TextWriter(/*options=*/{}) {}

  void Indent();
  void Dedent();

  template <typename... Args>
  void Append(Args&&... args) {
    AppendIndentation();
    content_.Append(absl::StrCat(std::forward<Args>(args)...));
    new_line_ = false;
  }

  template <typename... Args>
  void AppendLine(Args&&... args) {
    AppendIndentation();
    content_.Append(absl::StrCat(std::forward<Args>(args)..., "\n"));
    new_line_ = true;
  }

  template <typename... Args>
  void FinishLine(Args&&... args) {
    content_.Append(absl::StrCat(std::forward<Args>(args)..., "\n"));
    new_line_ = true;
  }

  template <typename... Args>
  void AppendUnindentedLine(Args&&... args) {
    FinishLine(std::forward<Args>(args)...);
  }

  void AppendEmptyLine();

  std::string Finish() &&;

 private:
  void AppendIndentation();

  Options const options_;

  size_t indentation_level_ = 0;
  std::vector<absl::Cord> indentation_cords_;

  absl::Cord content_;
  bool new_line_ = true;
};

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_WRITER_H__
