#include "proto/generator.h"

#include "gtest/gtest.h"

namespace {

using ::tsdb2::proto::generator::MakeHeaderFileName;
using ::tsdb2::proto::generator::MakeSourceFileName;

TEST(GeneratorTest, MakeHeaderFileName) {
  EXPECT_EQ(MakeHeaderFileName("foo.proto"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.bar"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.bar.proto"), "foo.bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo.proto.bar"), "foo.proto.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo"), "foo.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.proto"), "foo/bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.baz"), "foo/bar.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.baz.proto"), "foo/bar.baz.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar.proto.baz"), "foo/bar.proto.pb.h");
  EXPECT_EQ(MakeHeaderFileName("foo/bar"), "foo/bar.pb.h");
}

TEST(GeneratorTest, MakeSourceFileName) {
  EXPECT_EQ(MakeSourceFileName("foo.proto"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.bar"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.bar.proto"), "foo.bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo.proto.bar"), "foo.proto.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo"), "foo.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.proto"), "foo/bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.baz"), "foo/bar.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.baz.proto"), "foo/bar.baz.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar.proto.baz"), "foo/bar.proto.pb.cc");
  EXPECT_EQ(MakeSourceFileName("foo/bar"), "foo/bar.pb.cc");
}

// TODO

}  // namespace
