#include "http/default_server.h"
#include "server/init_tsdb2.h"

int main(int argc, char* argv[]) {
  tsdb2::init::InitServer(argc, argv);
  auto const server = tsdb2::http::DefaultServerBuilder::Get()->Build();
  server->WaitForTermination().IgnoreError();
  return 0;
}
