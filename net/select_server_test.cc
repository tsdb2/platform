#include "net/select_server.h"

#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::Not;
using ::testing::status::IsOkAndHolds;
using ::tsdb2::net::ListenerSocket;
using ::tsdb2::net::SelectServer;

class SelectServerTest : public ::testing::Test {
 public:
  explicit SelectServerTest() { select_server_->StartOrDie(); }

 protected:
  SelectServer* const select_server_ = SelectServer::GetInstance();
};

TEST_F(SelectServerTest, Listen) {
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket>("localhost", 8080),
              IsOkAndHolds(Not(nullptr)));
}

// TODO

}  // namespace
