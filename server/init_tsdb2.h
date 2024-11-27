#ifndef __TSDB2_SERVER_INIT_TSDB2_H__
#define __TSDB2_SERVER_INIT_TSDB2_H__

#include <string>
#include <string_view>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace init {

// Registers a module in the initialization system.
//
// `dependencies` is the list of direct dependencies, i.e. the list of modules that `module`
// directly depends on. Don't specify transitive dependencies in this list.
//
// It is safe to invoke this function in global scope. In fact, the standard use case of
// `RegisterModule` is to invoke it in the constructor of modules instantiated in global scope (see
// the documentation of `BaseModule` for further details). The internal implementation uses the
// localized static initialization pattern to avoid initialization order fiascos.
void RegisterModule(BaseModule *module, absl::Span<BaseModule *const> dependencies);

// Registers a module in the initialization system.
//
// `dependencies` is the list of direct dependencies, i.e. the list of modules that `module`
// directly depends on. Don't specify transitive dependencies in this list.
//
// This overload takes the list of dependencies as variadic arguments rather than an `absl::Span`.
//
// Similarly to the other overload, this function is safe to invoke in global scope (e.g. in the
// constructors of modules instantiated in global scope).
template <
    typename... Dependencies,
    std::enable_if_t<std::conjunction_v<std::is_base_of<BaseModule, Dependencies>...>, bool> = true>
void RegisterModule(BaseModule *const module, Dependencies *const... dependencies) {
  RegisterModule(module, {dependencies...});
}

void InitServer(int argc, char *argv[]);

void InitForTesting();

void Wait();

bool IsDone();

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_INIT_TSDB2_H__
