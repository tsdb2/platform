#include "io/fd.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <string_view>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "common/testing.h"
#include "gtest/gtest.h"

namespace {

using ::testing::TestTempFile;
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

TEST(FDTest, Clone) {
  static char const kData[] = "sator arepo tenet opera rotas";
  auto const status_or_file = TestTempFile::Create("fd_test");
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  auto const& fd1 = file.fd();
  ASSERT_GE(::write(*fd1, kData, sizeof(kData) - 1), 0) << absl::ErrnoToStatus(errno, "write");
  ASSERT_GE(::lseek(*fd1, 0, SEEK_SET), 0) << absl::ErrnoToStatus(errno, "lseek");
  auto const status_or_clone = fd1.Clone();
  EXPECT_OK(status_or_clone);
  auto const& fd2 = status_or_clone.value();
  char buffer[sizeof(kData)];
  std::memset(buffer, 0, sizeof(kData));
  ASSERT_GE(::read(*fd2, buffer, sizeof(kData) - 1), 0) << absl::ErrnoToStatus(errno, "read");
  EXPECT_EQ(std::string_view(kData), std::string_view(buffer));
}

}  // namespace
