#ifndef __TSDB2_SERVER_MODULE_MANAGER_H__
#define __TSDB2_SERVER_MODULE_MANAGER_H__

#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace init {

class ModuleDependency {
 public:
  ModuleDependency(  // NOLINT(google-explicit-constructor)
      BaseModule* const module)
      : module_(module), reverse_(false) {}

  explicit ModuleDependency(BaseModule* const module, bool const reverse)
      : module_(module), reverse_(reverse) {}

  bool is_reverse() const { return reverse_; }
  BaseModule* module() const { return module_; }

 private:
  BaseModule* module_;
  bool reverse_;
};

// Tag for direct module dependencies.
//
// It can be used with `tsdb2::init::RegisterModule`, but it's completely optional and often
// omitted. It's provided only for consistency with `ReverseDependency`.
//
// Example:
//
//   class FooModule : public tsdb2::init::BaseModule {
//    public:
//     explicit FooModule() : BaseModule("foo") {
//       tsdb2::init::RegisterModule(
//           this, Dependency(BarModule::Get()), Dependency(BazModule::Get()));
//     }
//   };
//
// Equivalent to:
//
//   class FooModule : public tsdb2::init::BaseModule {
//    public:
//     explicit FooModule() : BaseModule("foo") {
//       tsdb2::init::RegisterModule(this, BarModule::Get(), BazModule::Get());
//     }
//   };
//
inline ModuleDependency Dependency(BaseModule* const module) {
  return ModuleDependency(module, /*reverse=*/false);
}

// Tag for reverse module dependencies.
//
// Reverse dependencies indicate that if a module A has a reverse dependency on module B, then B
// depends on A.
//
// This tag can be used with `tsdb2::init::RegisterModule`. Example:
//
//   class FooModule : public tsdb2::init::BaseModule {
//    public:
//     explicit FooModule() : BaseModule("foo") {
//       tsdb2::init::RegisterModule(
//           this, Dependency(BarModule::Get()), ReverseDependency(BazModule::Get()));
//     }
//   };
//
// In the above, module `Foo` depends on `Bar` but also has a reverse dependency on module `Baz`, so
// the full dependency graph is:
//
//   Baz -> Foo -> Bar
//
// where "A -> B" indicates "A depends on B".
//
// An example use case for reverse dependencies is HTTP handlers registering themselves in the
// default HTTP server (each handler must reverse-depend on the default server module, we can't
// declare all existing handlers as direct dependencies of the default server module because some
// may not even be linked in).
inline ModuleDependency ReverseDependency(BaseModule* const module) {
  return ModuleDependency(module, /*reverse=*/true);
}

// Manages all registered modules.
//
// This class is thread-safe.
class ModuleManager {
 public:
  // Returns the singleton `ModuleManager` instance.
  static ModuleManager* GetInstance() { return GetSingleton()->Get(); }

  // Registers a module. This is the core implementation of `tsdb2::init::RegisterModule`.
  //
  // `dependencies` is the list of direct dependencies, i.e. the list of modules that `module`
  // directly depends on. Don't specify transitive dependencies in this list.
  //
  // Circular dependencies are not checked at this stage; they're checked later on by
  // `InitializeModules`.
  void RegisterModule(BaseModule* module, absl::Span<ModuleDependency const> dependencies)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Performs initialization of all registered modules respecting their dependency order. Each
  // module is initialized by calling its `Initialize` method.
  //
  // This method fails if there are circular dependencies or if one of the modules fails to
  // initialize. In the latter case it won't attempt to initialize other modules.
  absl::Status InitializeModules() ABSL_LOCKS_EXCLUDED(mutex_);

  // Like `Initialize` but runs the modules' `InitializeForTesting` methods instead of `Initialize`.
  // Use only in tests.
  absl::Status InitializeModulesForTesting() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // The unit test fixture is a friend so that it can override the singleton for testing.
  friend class InitTest;

  using ModuleSet = absl::flat_hash_set<BaseModule*>;
  using DependencyMap = absl::flat_hash_map<BaseModule*, std::vector<BaseModule*>>;

  // Checks if there are circular dependencies. This is the implementation of
  // `CheckCircularDependencies`.
  class DependencyChecker {
   public:
    explicit DependencyChecker(DependencyMap const& dependencies) : dependencies_(dependencies) {}

    ~DependencyChecker() = default;

    absl::Status Run();

    // For internal use by the checking algorithm.
    absl::Status PushModule(BaseModule* module);

    // For internal use by the checking algorithm.
    void PopModule(BaseModule* module);

   private:
    DependencyChecker(DependencyChecker const&) = delete;
    DependencyChecker& operator=(DependencyChecker const&) = delete;
    DependencyChecker(DependencyChecker&&) = delete;
    DependencyChecker& operator=(DependencyChecker&&) = delete;

    absl::Status CheckCircularDependencies(BaseModule* module);

    DependencyMap const& dependencies_;
    absl::flat_hash_set<BaseModule*> visited_;
    absl::flat_hash_set<BaseModule*> path_;
    std::vector<std::string> ancestor_names_;
  };

  // Scoped object that controls the pushing and popping of a module in the `DependencyChecker`.
  // Used by the dependency checking algorithm.
  class ModuleFrame final {
   public:
    static absl::StatusOr<ModuleFrame> Create(DependencyChecker* const parent,
                                              BaseModule* const module) {
      RETURN_IF_ERROR(parent->PushModule(module));
      return ModuleFrame(parent, module);
    }

    ~ModuleFrame() {
      if (parent_ != nullptr) {
        parent_->PopModule(module_);
      }
    }

    ModuleFrame(ModuleFrame&& other) noexcept : parent_(other.parent_), module_(other.module_) {
      other.parent_ = nullptr;
      other.module_ = nullptr;
    }

   private:
    explicit ModuleFrame(DependencyChecker* const parent, BaseModule* const module)
        : parent_(parent), module_(module) {}

    ModuleFrame(ModuleFrame const&) = delete;
    ModuleFrame& operator=(ModuleFrame const&) = delete;
    ModuleFrame& operator=(ModuleFrame&& other) = delete;

    DependencyChecker* parent_;
    BaseModule* module_;
  };

  // Runs the initialization algorithm, which makes sure each module is initialized exactly once
  // respecting the order required by the dependencies.
  class Initializer {
   public:
    explicit Initializer(DependencyMap const& dependencies, bool const testing)
        : dependencies_(dependencies), testing_(testing) {}

    absl::Status Run();

   private:
    // Returns the set of root modules, i.e. those that no other module depends on.
    absl::flat_hash_set<BaseModule*> GetRoots();

    // Recursive implementation of the initialization algorithm.
    absl::Status InitializeModule(BaseModule* module);

    DependencyMap const& dependencies_;
    bool const testing_;
    absl::flat_hash_set<BaseModule*> initialized_;
  };

  static gsl::owner<ModuleManager*> CreateInstance();
  static tsdb2::common::Singleton<ModuleManager>* GetSingleton();
  explicit ModuleManager() = default;
  ~ModuleManager() = default;

  ModuleManager(ModuleManager const&) = delete;
  ModuleManager& operator=(ModuleManager const&) = delete;
  ModuleManager(ModuleManager&&) = delete;
  ModuleManager& operator=(ModuleManager&&) = delete;

  // Returns a unique text representation of a module with its name (used for error logging).
  static std::string GetModuleString(BaseModule* module);

  void DoRegisterModule(BaseModule* module) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Checks if there are any circular dependencies across the registered modules, returning an error
  // status if one is found.
  absl::Status CheckCircularDependencies() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  absl::Status InitializeModule(BaseModule* module) ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutable mutex_;

  // The set of registered modules, stored here to check if a module gets registered more than once.
  // If that does happen we check-fail because we don't want to run the same initialization code
  // multiple times.
  ModuleSet registered_modules_ ABSL_GUARDED_BY(mutex_);

  // Keeps track of module dependencies. When module A is the key of an entry in this map, and
  // modules B, C, and D is the list in the corresponding value, it means A depends on B, C, and D.
  //
  // All registered modules have a key in this map. If the corresponding value is empty it means
  // that module doesn't depend on any other modules.
  DependencyMap dependencies_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_MODULE_MANAGER_H__
