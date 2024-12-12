#ifndef __TSDB2_SERVER_TESTING_H__
#define __TSDB2_SERVER_TESTING_H__

#include "common/testing.h"  // IWYU pragma: export
#include "gtest/gtest.h"
#include "server/init_tsdb2.h"

namespace tsdb2 {
namespace testing {
namespace init {

class Test : public ::testing::Test {
 protected:
  explicit Test() { tsdb2::init::InitForTesting(); }
};

}  // namespace init
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_TESTING_H__
