#include "server/module_manager.h"

#include <initializer_list>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace init {

void ModuleManager::RegisterModule(BaseModule* const module,
                                   std::initializer_list<ModuleDependency> const dependencies) {
  absl::MutexLock lock{&mutex_};
  DoRegisterModule(module);
  auto const [it, inserted] = dependencies_.try_emplace(module);
  auto& direct_dependencies = it->second;
  for (auto const& dependency : dependencies) {
    if (!dependency.is_reverse()) {
      direct_dependencies.emplace_back(dependency.module());
    }
  }
  // Reverse dependencies must be processed separately because each `try_emplace` call may cause a
  // rehash and invalidate the `direct_dependencies` reference above.
  for (auto const& dependency : dependencies) {
    if (dependency.is_reverse()) {
      auto const [it, unused] = dependencies_.try_emplace(dependency.module());
      it->second.emplace_back(module);
    }
  }
}

absl::Status ModuleManager::InitializeModules() {
  absl::MutexLock lock{&mutex_};
  RETURN_IF_ERROR(CheckCircularDependencies());
  return Initializer(dependencies_, /*testing=*/false).Run();
}

absl::Status ModuleManager::InitializeModulesForTesting() {
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
  for (auto* const dependency : it->second) {
    RETURN_IF_ERROR(CheckCircularDependencies(dependency));
  }
  return absl::OkStatus();
}

absl::Status ModuleManager::Initializer::Run() {
  for (auto* const root : GetRoots()) {
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
    for (auto* const dependency : dependencies) {
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
  for (auto* const dependency : it->second) {
    RETURN_IF_ERROR(InitializeModule(dependency));
  }
  if (testing_) {
    return module->InitializeForTesting();
  } else {
    return module->Initialize();
  }
}

gsl::owner<ModuleManager*> ModuleManager::CreateInstance() { return new ModuleManager(); }

tsdb2::common::Singleton<ModuleManager>* ModuleManager::GetSingleton() {
  // NOTE: we need to use both the localized static initialization pattern and `Singleton` for this
  // object. The former avoids initialization order fiascos that may arise due to the various
  // modules being instantiated in global scope and depending on the `ModuleManager`; while the
  // latter allows for thread-safe overrides in our unit tests.
  static tsdb2::common::Singleton<ModuleManager> instance{&ModuleManager::CreateInstance};
  return &instance;
}

std::string ModuleManager::GetModuleString(BaseModule* const module) {
  return absl::StrCat("\"", module->name(), "\" (0x", absl::Hex(module, absl::kZeroPad16), ")");
}

void ModuleManager::DoRegisterModule(BaseModule* const module) {
  auto const [it, inserted] = registered_modules_.emplace(module);
  CHECK(inserted) << "module " << GetModuleString(module) << " has been registered twice!";
}

absl::Status ModuleManager::CheckCircularDependencies() {
  return DependencyChecker(dependencies_).Run();
}

}  // namespace init
}  // namespace tsdb2
