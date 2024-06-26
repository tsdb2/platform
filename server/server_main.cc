#include "http/http_node.h"
#include "server/init_tsdb2.h"

int main(int argc, char* argv[]) {
  tsdb2::init::InitServer(argc, argv);
  tsdb2::http::HttpNode::GetDefault()->WaitForTermination();
  return 0;
}
