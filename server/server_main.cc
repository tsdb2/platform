#include <cstddef>

#include "absl/flags/flag.h"
#include "server/init_tsdb2.h"

ABSL_FLAG(size_t, tsdb2__select_server_num_workers, 100,
          "Number of worker threads in the select server.");

int main(int argc, char* argv[]) {
  ::tsdb2::init::InitServer(argc, argv);

  // TODO: run HTTP/2 server.

  return 0;
}
