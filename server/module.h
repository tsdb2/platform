#ifndef __TSDB2_SERVER_MODULE_H__
#define __TSDB2_SERVER_MODULE_H__

#include <string_view>
#include <type_traits>

#include "absl/status/status.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"
#include "server/module_manager.h"

namespace tsdb2 {
namespace init {

namespace module_internal {

// Checks if the traits class has the static `name` field.
template <typename ModuleTraits, typename Enable = void>
struct HasName : public std::false_type {};

template <typename ModuleTraits>
struct HasName<ModuleTraits, std::void_t<decltype(ModuleTraits::name)>>
    : public std::is_constructible<std::string_view, decltype(ModuleTraits::name)> {};

template <typename ModuleTraits>
inline bool constexpr HasNameV = HasName<ModuleTraits>::value;

// Checks if the traits class has the `Initialize` method.
template <typename ModuleTraits, typename Enable = void>
struct HasInitialize : public std::false_type {};

template <typename ModuleTraits>
struct HasInitialize<ModuleTraits, std::void_t<decltype(std::declval<ModuleTraits>().Initialize())>>
    : public std::is_same<absl::Status, decltype(std::declval<ModuleTraits>().Initialize())> {};

template <typename ModuleTraits>
inline bool constexpr HasInitializeV = HasInitialize<ModuleTraits>::value;

// Checks if the traits class has the `InitializeForTesting` method.
template <typename ModuleTraits, typename Enable = void>
struct HasInitializeForTesting : public std::false_type {};

template <typename ModuleTraits>
struct HasInitializeForTesting<
    ModuleTraits, std::void_t<decltype(std::declval<ModuleTraits>().InitializeForTesting())>>
    : public std::is_same<absl::Status,
                          decltype(std::declval<ModuleTraits>().InitializeForTesting())> {};

template <typename ModuleTraits>
inline bool constexpr HasInitializeForTestingV = HasInitializeForTesting<ModuleTraits>::value;

// Module implementation
template <typename Traits>
class ModuleImpl final : public BaseModule {
 public:
  static_assert(std::is_default_constructible_v<Traits>,
                "Module traits must be default-constructible.");

  static_assert(HasNameV<Traits>, "Module traits must have a static string field called `name`.");

  static ModuleImpl* Get() { return GetSingleton()->Get(); }

  absl::Status Initialize() override { return Initializer()(&traits_); }
  absl::Status InitializeForTesting() override { return TestingInitializer()(&traits_); }

 private:
  template <typename TraitsAlias = Traits, typename Enable = void>
  struct Initializer {
    absl::Status operator()(TraitsAlias* /*traits*/) const { return absl::OkStatus(); }
  };

  template <typename TraitsAlias>
  struct Initializer<TraitsAlias, std::enable_if_t<HasInitializeV<TraitsAlias>>> {
    absl::Status operator()(TraitsAlias* const traits) const { return traits->Initialize(); }
  };

  template <typename TraitsAlias = Traits, typename Enable = void>
  struct TestingInitializer {
    absl::Status operator()(TraitsAlias* const traits) const { return Initializer()(traits); }
  };

  template <typename TraitsAlias>
  struct TestingInitializer<TraitsAlias, std::enable_if_t<HasInitializeForTestingV<TraitsAlias>>> {
    absl::Status operator()(TraitsAlias* const traits) const {
      return traits->InitializeForTesting();
    }
  };

  static gsl::owner<ModuleImpl*> CreateInstance() {
    return gsl::owner<ModuleImpl*>(new ModuleImpl());
  }

  static tsdb2::common::Singleton<ModuleImpl>* GetSingleton() {
    // NOTE: we need to use both the localized static initialization pattern and `Singleton` for
    // this object. The former avoids initialization order fiascos that may arise due to the
    // `Module` object being instantiated in global scope by the user and depending on the
    // corresponding `ModuleImpl` singleton; while the latter allows for thread-safe overrides in
    // our unit tests.
    static tsdb2::common::Singleton<ModuleImpl> instance{&ModuleImpl::CreateInstance};
    return &instance;
  }

  explicit ModuleImpl() : BaseModule(Traits::name) {}

  Traits traits_;

  friend class ModuleTest;
};

}  // namespace module_internal

// Tag for direct module dependencies.
//
// It can be used in a `tsdb2::init::Module` definition, but it's completely optional and often
// omitted. It's provided only for consistency with `ReverseDependency`.
//
// Example usage:
//
//   static tsdb2::init::Module<
//       FooModule, Dependency<BarModule>, Dependency<BazModule>> const foo_module;
//
template <typename Module>
struct Dependency {};

// Tag for reverse module dependencies.
//
// Reverse dependencies indicate that if a module A has a reverse dependency on module B, then B
// depends on A.
//
// This tag can be used in a `tsdb2::init::Module` definition. Example:
//
//   static tsdb2::init::Module<
//       FooModule, Dependency<BarModule>, ReverseDependency<BazModule>> const foo_module;
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
template <typename Module>
struct ReverseDependency {};

// This class simplifies the creation of initialization modules.
//
// When using `Module` the user doesn't have to worry about instantiating the module, making it
// trivially destructible, avoiding initializationorder fiascos, etc.
//
// The following example shows the definition of a `FooModule` depending on two other modules called
// `BarModule` and `BazModule`:
//
//   // foo.h
//
//   class FooModule {
//    public:
//     static std::string_view constexpr name = "foo";
//     absl::Status Initialize();
//     absl::Status InitializeForTesting();
//   };
//
//
//   // foo.cc
//
//   static tsdb2::init::Module<FooModule, BarModule, BazModule> const foo_module;
//
//   absl::Status FooModule::Initialize() {
//     // ...
//   }
//
//   absl::Status FooModule::InitializeForTesting() {
//     // ...
//   }
//
//
// NOTE: remember to instantiate the `Module` object in global scope in your .cc file, otherwise the
// module won't get registered and won't run.
//
// NOTE: unlike raw modules, user modules defined with `tsdb2::init::Module` MUST NOT inherit
// `tsdb2::init::BaseModule`. Doing so would result in a compilation error.
//
// Both `Initialize` and `InitializeForTesting` are optional.
template <typename Traits, typename... Dependencies>
class Module {
 public:
  static auto Get() { return module_internal::ModuleImpl<Traits>::Get(); }

  explicit Module() {
    tsdb2::init::RegisterModule(
        module_internal::ModuleImpl<Traits>::Get(),
        {ModuleDependency(GetModuleT<Dependencies>::Get(), IsReverseDependencyV<Dependencies>)...});
  }

 private:
  template <typename Arg>
  struct Match {
    using ModuleType = Module<Arg>;
    static inline bool constexpr kIsReverseDependency = false;
  };

  template <typename ModuleTraits>
  struct Match<Dependency<ModuleTraits>> {
    using ModuleType = Module<ModuleTraits>;
    static inline bool constexpr kIsReverseDependency = false;
  };

  template <typename ModuleTraits>
  struct Match<ReverseDependency<ModuleTraits>> {
    using ModuleType = Module<ModuleTraits>;
    static inline bool constexpr kIsReverseDependency = true;
  };

  template <typename Arg>
  using GetModuleT = typename Match<Arg>::ModuleType;

  template <typename Arg>
  static inline bool constexpr IsReverseDependencyV = Match<Arg>::kIsReverseDependency;
};

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_MODULE_H__
