#ifndef __TSDB2_SERVER_INIT_TSDB2_H__
#define __TSDB2_SERVER_INIT_TSDB2_H__

#include <initializer_list>

#include "server/base_module.h"
#include "server/module_manager.h"  // IWYU pragma: export

namespace tsdb2 {
namespace init {

// Invoked by the `tsdb2::init::Module` constructor to register a module in the initialization
// system.
//
// `dependencies` is the list of modules that `module` directly depends on. Each dependency is a
// `ModuleDependency` object specifying the dependency module and whether it's a reverse dependency
// (see the docs for `tsdb2::init::ReverseDependency` for an explanation about reverse
// dependencies).
//
// This function is safe to call in the global scope, where all `tsdb2::init::Module` objects should
// be instantiated.
void RegisterModule(BaseModule *module, std::initializer_list<ModuleDependency> dependencies);

void InitServer(int argc, char *argv[]);

void InitForTesting();

void Wait();

bool IsDone();

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_INIT_TSDB2_H__
