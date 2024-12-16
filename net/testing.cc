#include "net/testing.h"

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/sequence_number.h"
#include "common/testing.h"

namespace tsdb2 {
namespace net {
namespace testing {

std::string MakeTestSocketPath() {
  static tsdb2::common::SequenceNumber id;
  auto const directory = ::testing::GetTestTmpDir();
  if (absl::EndsWith(directory, "/")) {
    return absl::StrCat(directory, "sockets_test.", id.GetNext(), ".sock");
  } else {
    return absl::StrCat(directory, "/sockets_test.", id.GetNext(), ".sock");
  }
}

}  // namespace testing
}  // namespace net
}  // namespace tsdb2
