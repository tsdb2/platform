#include "http/server.h"
#include "server/init_tsdb2.h"

int main(int argc, char* argv[]) {
  tsdb2::init::InitServer(argc, argv);
  tsdb2::http::Server::GetDefault()->WaitForTermination().IgnoreError();
  return 0;
}
