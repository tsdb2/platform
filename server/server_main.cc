#include "server/init_tsdb2.h"

int main(int argc, char* argv[]) {
  ::tsdb2::init::InitServer(argc, argv);

  // TODO: run HTTP/2 server.

  return 0;
}
