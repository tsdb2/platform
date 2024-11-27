#include "io/fd.h"

#include <utility>

#include "absl/hash/hash.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::io::FD;

TEST(FDTest, Empty) {
  FD fd;
  EXPECT_TRUE(fd.empty());
  EXPECT_FALSE(fd.operator bool());
}

TEST(FDTest, NotEmpty) {
  FD fd{123};
  EXPECT_FALSE(fd.empty());
  EXPECT_TRUE(fd.operator bool());
  EXPECT_EQ(fd.get(), 123);
  EXPECT_EQ(*fd, 123);
}

TEST(FDTest, Close) {
  FD fd{123};
  fd.Close();
  EXPECT_TRUE(fd.empty());
}

TEST(FDTest, Release) {
  FD fd{123};
  EXPECT_EQ(fd.Release(), 123);
  EXPECT_TRUE(fd.empty());
}

TEST(FDTest, MoveConstruct) {
  FD fd1{123};
  FD fd2{std::move(fd1)};
  EXPECT_TRUE(fd1.empty());
  EXPECT_FALSE(fd2.empty());
  EXPECT_EQ(fd2.get(), 123);
}

TEST(FDTest, MoveAssign) {
  FD fd1{123};
  FD fd2;
  fd2 = std::move(fd1);
  EXPECT_TRUE(fd1.empty());
  EXPECT_FALSE(fd2.empty());
  EXPECT_EQ(fd2.get(), 123);
}

TEST(FDTest, Swap) {
  FD fd1{123};
  FD fd2{345};
  fd1.swap(fd2);
  EXPECT_FALSE(fd1.empty());
  EXPECT_EQ(fd1.get(), 345);
  EXPECT_FALSE(fd2.empty());
  EXPECT_EQ(fd2.get(), 123);
}

TEST(FDTest, AdlSwap) {
  FD fd1{123};
  FD fd2{345};
  swap(fd1, fd2);
  EXPECT_FALSE(fd1.empty());
  EXPECT_EQ(fd1.get(), 345);
  EXPECT_FALSE(fd2.empty());
  EXPECT_EQ(fd2.get(), 123);
}

TEST(FDTest, Hashable) {
  EXPECT_EQ(absl::HashOf(FD(123)), absl::HashOf(FD(123)));
  EXPECT_NE(absl::HashOf(FD(123)), absl::HashOf(FD(456)));
}

TEST(FDTest, Comparable) {
  FD fd1{123};
  FD fd2{456};
  EXPECT_FALSE(fd1 == fd2);
  EXPECT_TRUE(fd1 != fd2);
  EXPECT_TRUE(fd1 < fd2);
  EXPECT_TRUE(fd1 <= fd2);
  EXPECT_FALSE(fd1 > fd2);
  EXPECT_FALSE(fd1 >= fd2);
}

}  // namespace
