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

  void Indent();
  void Dedent();

  template <typename... Args>
  void AppendLine(Args&&... args) {
    AppendIndentation();
    content_.Append(absl::StrCat(std::forward<Args>(args)..., "\n"));
  }

  template <typename... Args>
  void AppendUnindentedLine(Args&&... args) {
    content_.Append(absl::StrCat(std::forward<Args>(args)..., "\n"));
  }

  void AppendEmptyLine();

  std::string Finish() &&;

 private:
  static size_t constexpr kIndentWidth = 2;

  void AppendIndentation();

  size_t indentation_level_ = 0;
  std::vector<absl::Cord> indentation_cords_;

  absl::Cord content_;
};

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TEXT_WRITER_H__
