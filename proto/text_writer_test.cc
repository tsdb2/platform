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

}  // namespace
