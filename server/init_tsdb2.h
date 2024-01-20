#ifndef __TSDB2_SERVER_INIT_TSDB2_H__
#define __TSDB2_SERVER_INIT_TSDB2_H__

namespace tsdb2 {
namespace init {

void InitServer(int argc, char* argv[]);

void Wait();

bool IsDone();

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_INIT_TSDB2_H__
