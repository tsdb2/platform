#include "server/module_manager.h"

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace init {

void ModuleManager::RegisterModule(BaseModule* const module,
                                   absl::Span<BaseModule* const> const dependencies) {
  absl::MutexLock lock{&mutex_};
  auto const [unused_it, inserted] =
      dependencies_.try_emplace(module, dependencies.begin(), dependencies.end());
  CHECK(inserted) << "module " << GetModuleString(module) << " has been registered twice!";
}

absl::Status ModuleManager::InitializeModules() {
  absl::flat_hash_set<BaseModule*> roots;
  absl::MutexLock lock{&mutex_};
  RETURN_IF_ERROR(CheckCircularDependencies());
  return Initializer(dependencies_, /*testing=*/false).Run();
}

absl::Status ModuleManager::InitializeModulesForTesting() {
  absl::flat_hash_set<BaseModule*> roots;
  absl::MutexLock lock{&mutex_};
  RETURN_IF_ERROR(CheckCircularDependencies());
  return Initializer(dependencies_, /*testing=*/true).Run();
}

absl::Status ModuleManager::DependencyChecker::Run() {
  for (auto const& [module, unused_dependencies] : dependencies_) {
    RETURN_IF_ERROR(CheckCircularDependencies(module));
  }
  return absl::OkStatus();
}

absl::Status ModuleManager::DependencyChecker::PushModule(BaseModule* const module) {
  ancestor_names_.emplace_back(GetModuleString(module));
  auto const [unused_it, inserted] = path_.emplace(module);
  if (inserted) {
    return absl::OkStatus();
  } else {
    return absl::FailedPreconditionError(
        absl::StrCat("circular module dependency: ", absl::StrJoin(ancestor_names_, " -> ")));
  }
}

void ModuleManager::DependencyChecker::PopModule(BaseModule* const module) {
  path_.erase(module);
  ancestor_names_.pop_back();
}

absl::Status ModuleManager::DependencyChecker::CheckCircularDependencies(BaseModule* const module) {
  DEFINE_OR_RETURN(frame, ModuleFrame::Create(this, module));
  auto const [unused_it, inserted] = visited_.emplace(module);
  if (!inserted) {
    return absl::OkStatus();
  }
  auto const it = dependencies_.find(module);
  CHECK(it != dependencies_.end())
      << "module " << GetModuleString(module) << " not found in dependency graph";
  for (auto const dependency : it->second) {
    RETURN_IF_ERROR(CheckCircularDependencies(dependency));
  }
  return absl::OkStatus();
}

absl::Status ModuleManager::Initializer::Run() {
  for (auto const root : GetRoots()) {
    RETURN_IF_ERROR(InitializeModule(root));
  }
  return absl::OkStatus();
}

absl::flat_hash_set<BaseModule*> ModuleManager::Initializer::GetRoots() {
  absl::flat_hash_set<BaseModule*> roots;
  roots.reserve(dependencies_.size());
  for (auto const& [module, dependencies] : dependencies_) {
    roots.emplace(module);
  }
  for (auto const& [module, dependencies] : dependencies_) {
    for (auto const dependency : dependencies) {
      roots.erase(dependency);
    }
  }
  return roots;
}

absl::Status ModuleManager::Initializer::InitializeModule(BaseModule* const module) {
  auto const [unused_it, inserted] = initialized_.emplace(module);
  if (!inserted) {
    return absl::OkStatus();
  }
  auto const it = dependencies_.find(module);
  CHECK(it != dependencies_.end())
      << "module " << GetModuleString(module) << " not found in dependency graph";
  for (auto const dependency : it->second) {
    RETURN_IF_ERROR(InitializeModule(dependency));
  }
  if (testing_) {
    return module->InitializeForTesting();
  } else {
    return module->Initialize();
  }
}

ModuleManager* ModuleManager::CreateInstance() { return new ModuleManager(); }

tsdb2::common::Singleton<ModuleManager>* ModuleManager::GetSingleton() {
  // NOTE: we need to use both the localized static initialization pattern and `Singleton` for this
  // object. The former avoids initialization order fiascos, while the latter allows overriding in
  // tests and also makes the instance trivially destructible (as if we used `NoDestructor`). We
  // can't just use `Singleton` in global scope or as a static class member because it would be
  // undefined when the `Singleton` itself is constructed, so it woulnd't necessarily be available
  // when other modules are constructed. The `GetSingleton` function on the other hand can construct
  // the `Singleton` (and the wrapped `ModuleManager`) on demand.
  static tsdb2::common::Singleton<ModuleManager> kInstance{&ModuleManager::CreateInstance};
  return &kInstance;
}

std::string ModuleManager::GetModuleString(BaseModule* const module) {
  return absl::StrCat("\"", module->name(), "\" (0x", absl::Hex(module, absl::kZeroPad16), ")");
}

absl::Status ModuleManager::CheckCircularDependencies() {
  return DependencyChecker(dependencies_).Run();
}

}  // namespace init
}  // namespace tsdb2
