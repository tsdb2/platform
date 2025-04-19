#include "proto/text_writer.h"

#include <utility>

#include "gtest/gtest.h"

namespace {

using ::tsdb2::proto::internal::TextWriter;

class TextWriterTest : public ::testing::Test {
 protected:
  TextWriter writer_;
};

TEST_F(TextWriterTest, Empty) { EXPECT_EQ(std::move(writer_).Finish(), ""); }

TEST_F(TextWriterTest, AppendLine) {
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n");
}

TEST_F(TextWriterTest, AppendMultiPartLine) {
  writer_.AppendLine("lorem ", 42, " ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem 42 ipsum\n");
}

TEST_F(TextWriterTest, AppendTwoLines) {
  writer_.AppendLine("dolor amet");
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "dolor amet\nlorem ipsum\n");
}

TEST_F(TextWriterTest, Indent) {
  writer_.Indent();
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum\n");
}

TEST_F(TextWriterTest, IndentTwice) {
  writer_.Indent();
  writer_.Indent();
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "    lorem ipsum\n");
}

TEST_F(TextWriterTest, Dedent) {
  writer_.AppendLine("lorem ipsum");
  writer_.Indent();
  writer_.AppendLine("dolor amet");
  writer_.Dedent();
  writer_.AppendLine("adipisci elit");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n  dolor amet\nadipisci elit\n");
}

TEST_F(TextWriterTest, DedentTwice) {
  writer_.AppendLine("lorem");
  writer_.Indent();
  writer_.AppendLine("ipsum");
  writer_.Indent();
  writer_.AppendLine("dolor");
  writer_.Dedent();
  writer_.AppendLine("amet");
  writer_.Dedent();
  writer_.AppendLine("adipisci");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\n    dolor\n  amet\nadipisci\n");
}

TEST_F(TextWriterTest, AppendUnindentedLine) {
  writer_.Indent();
  writer_.AppendUnindentedLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n");
}

TEST_F(TextWriterTest, AppendMultiPartUnindentedLine) {
  writer_.Indent();
  writer_.AppendUnindentedLine("lorem ", 42, " ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem 42 ipsum\n");
}

TEST_F(TextWriterTest, IndentedAndUnindentedLines) {
  writer_.AppendLine("lorem");
  writer_.Indent();
  writer_.AppendLine("ipsum");
  writer_.Indent();
  writer_.AppendUnindentedLine("dolor");
  writer_.Dedent();
  writer_.AppendLine("amet");
  writer_.Dedent();
  writer_.AppendLine("adipisci");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\ndolor\n  amet\nadipisci\n");
}

TEST_F(TextWriterTest, AppendEmptyLine) {
  writer_.AppendEmptyLine();
  EXPECT_EQ(std::move(writer_).Finish(), "\n");
}

TEST_F(TextWriterTest, AppendEmptyLineBetweenLines) {
  writer_.AppendLine("lorem");
  writer_.AppendEmptyLine();
  writer_.AppendLine("ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n\nipsum\n");
}

TEST_F(TextWriterTest, EmptyLineIsNotIndented) {
  writer_.Indent();
  writer_.AppendLine("lorem");
  writer_.AppendEmptyLine();
  writer_.AppendLine("ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem\n\n  ipsum\n");
}

TEST_F(TextWriterTest, IndentedScope) {
  writer_.AppendLine("lorem");
  {
    TextWriter::IndentedScope is{&writer_};
    writer_.AppendLine("ipsum");
  }
  writer_.AppendLine("dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\ndolor\n");
}

TEST_F(TextWriterTest, NestedIndentedScope) {
  writer_.AppendLine("lorem");
  {
    TextWriter::IndentedScope is{&writer_};
    writer_.AppendLine("ipsum");
    {
      TextWriter::IndentedScope is{&writer_};
      writer_.AppendLine("dolor");
    }
    writer_.AppendLine("amet");
  }
  writer_.AppendLine("adipisci");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\n    dolor\n  amet\nadipisci\n");
}

TEST_F(TextWriterTest, OverrideIndentWidth) {
  TextWriter writer{TextWriter::Options{.indent_width = 3}};
  writer.AppendLine("lorem");
  writer.Indent();
  writer.AppendLine("ipsum");
  writer.Indent();
  writer.AppendLine("dolor");
  writer.Dedent();
  writer.AppendLine("amet");
  writer.Dedent();
  writer.AppendLine("adipisci");
  EXPECT_EQ(std::move(writer).Finish(), "lorem\n   ipsum\n      dolor\n   amet\nadipisci\n");
}

TEST_F(TextWriterTest, Append) {
  writer_.Append("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum");
}

TEST_F(TextWriterTest, AppendMultiPart) {
  writer_.Append("lorem ", 42, " ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem 42 ipsum");
}

TEST_F(TextWriterTest, AppendTwice) {
  writer_.Append("lorem ipsum");
  writer_.Append(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum dolor amet");
}

TEST_F(TextWriterTest, AppendAndFinish) {
  writer_.Append("lorem");
  writer_.FinishLine();
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n");
}

TEST_F(TextWriterTest, AppendAndFinishSinglePart) {
  writer_.Append("lorem");
  writer_.FinishLine(" dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem dolor\n");
}

TEST_F(TextWriterTest, AppendAndFinishMultiPart) {
  writer_.Append("lorem");
  writer_.FinishLine(" dolor ", 42, " amet");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem dolor 42 amet\n");
}

TEST_F(TextWriterTest, AppendIndented) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum");
}

TEST_F(TextWriterTest, AppendTwiceIndented) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  writer_.Append(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum dolor amet");
}

TEST_F(TextWriterTest, NextIsStillIndented) {
  writer_.Indent();
  writer_.Append("lorem");
  writer_.FinishLine(" ipsum");
  writer_.Append("dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum\n  dolor");
}

TEST_F(TextWriterTest, IgnoreIndentInsideLine1) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  writer_.Indent();
  writer_.Append(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum dolor amet");
}

TEST_F(TextWriterTest, IgnoreIndentInsideLine2) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  writer_.Indent();
  writer_.FinishLine(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum dolor amet\n");
}

TEST_F(TextWriterTest, NextIsMoreIndented) {
  writer_.Indent();
  writer_.Append("lorem");
  writer_.Indent();
  writer_.FinishLine(" ipsum");
  writer_.Append("dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum\n    dolor");
}

TEST_F(TextWriterTest, IgnoreDedentInsideLine1) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  writer_.Dedent();
  writer_.FinishLine(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum dolor amet\n");
}

TEST_F(TextWriterTest, IgnoreDedentInsideLine2) {
  writer_.Indent();
  writer_.Append("lorem ipsum");
  writer_.Dedent();
  writer_.FinishLine(" dolor amet");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum dolor amet\n");
}

TEST_F(TextWriterTest, NextIsDedented) {
  writer_.Indent();
  writer_.Append("lorem");
  writer_.Dedent();
  writer_.FinishLine(" ipsum");
  writer_.Append("dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum\ndolor");
}

}  // namespace
