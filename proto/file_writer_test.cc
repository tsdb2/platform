#include "proto/file_writer.h"

#include <utility>

#include "gtest/gtest.h"

namespace {

using ::tsdb2::proto::internal::FileWriter;

class FileWriterTest : public ::testing::Test {
 protected:
  FileWriter writer_;
};

TEST_F(FileWriterTest, Empty) { EXPECT_EQ(std::move(writer_).Finish(), ""); }

TEST_F(FileWriterTest, AppendLine) {
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n");
}

TEST_F(FileWriterTest, AppendTwoLines) {
  writer_.AppendLine("dolor amet");
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "dolor amet\nlorem ipsum\n");
}

TEST_F(FileWriterTest, Indent) {
  writer_.Indent();
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem ipsum\n");
}

TEST_F(FileWriterTest, IndentTwice) {
  writer_.Indent();
  writer_.Indent();
  writer_.AppendLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "    lorem ipsum\n");
}

TEST_F(FileWriterTest, Dedent) {
  writer_.AppendLine("lorem ipsum");
  writer_.Indent();
  writer_.AppendLine("dolor amet");
  writer_.Dedent();
  writer_.AppendLine("adipisci elit");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n  dolor amet\nadipisci elit\n");
}

TEST_F(FileWriterTest, DedentTwice) {
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

TEST_F(FileWriterTest, AppendUnindentedLine) {
  writer_.Indent();
  writer_.AppendUnindentedLine("lorem ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem ipsum\n");
}

TEST_F(FileWriterTest, IndentedAndUnindentedLines) {
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

TEST_F(FileWriterTest, AppendEmptyLine) {
  writer_.AppendEmptyLine();
  EXPECT_EQ(std::move(writer_).Finish(), "\n");
}

TEST_F(FileWriterTest, AppendEmptyLineBetweenLines) {
  writer_.AppendLine("lorem");
  writer_.AppendEmptyLine();
  writer_.AppendLine("ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n\nipsum\n");
}

TEST_F(FileWriterTest, EmptyLineIsNotIndented) {
  writer_.Indent();
  writer_.AppendLine("lorem");
  writer_.AppendEmptyLine();
  writer_.AppendLine("ipsum");
  EXPECT_EQ(std::move(writer_).Finish(), "  lorem\n\n  ipsum\n");
}

TEST_F(FileWriterTest, IndentedScope) {
  writer_.AppendLine("lorem");
  {
    FileWriter::IndentedScope is{&writer_};
    writer_.AppendLine("ipsum");
  }
  writer_.AppendLine("dolor");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\ndolor\n");
}

TEST_F(FileWriterTest, NestedIndentedScope) {
  writer_.AppendLine("lorem");
  {
    FileWriter::IndentedScope is{&writer_};
    writer_.AppendLine("ipsum");
    {
      FileWriter::IndentedScope is{&writer_};
      writer_.AppendLine("dolor");
    }
    writer_.AppendLine("amet");
  }
  writer_.AppendLine("adipisci");
  EXPECT_EQ(std::move(writer_).Finish(), "lorem\n  ipsum\n    dolor\n  amet\nadipisci\n");
}

}  // namespace
