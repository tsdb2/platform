#ifndef __TSDB2_SERVER_BASE_MODULE_H__
#define __TSDB2_SERVER_BASE_MODULE_H__

#include <string>
#include <string_view>

#include "absl/status/status.h"

namespace tsdb2 {
namespace init {

// Base class for all modules.
//
// Modules MUST be instantiated in global scope, wrapped by `tsdb2::common::NoDestructor`, and MUST
// register themselves using `tsdb2::init::RegisterModule`. The initialization system guarantees
// that their `Initialize` method will be invoked exactly once before the body of `main`, but after
// Abseil flags and logging are initialized. Therefore it's okay for a module to read Abseil flags
// and issue log entries in its `Initialize` methods.
//
// Note that `tsdb2::init::RegisterModule` allows declaring dependencies of a module; the
// initialization system checks for circular dependencies, checkfailing if one is found, and also
// ensures that every module is initialized after all of its dependencies.
//
// In unit tests the initialization system calls `InitializeForTesting` instead of `Initialize`.
//
// A quick example of how to define a `FooModule` in a foo.h/foo.cc unit follows. The module depends
// on a `bar_module` and `baz_module` provided by another unit.
//
// foo.h:
//
//   class FooModule : public tsdb2::init::BaseModule {
//    public:
//     explicit FooModule();
//     absl::Status Initialize() override;
//     absl::Status InitializeForTesting() override;
//   };
//
//   extern tsdb2::common::NoDestructor<FooModule> foo_module;
//
//
// foo.cc:
//
//   tsdb2::common::NoDestructor<FooModule> foo_module;
//
//   FooModule::FooModule() : BaseModule("foo") {
//     tsdb2::init::RegisterModule(this, &bar_module, &baz_module);
//   }
//
//   absl::Status FooModule::Initialize() {
//     // ... initialization code ...
//   }
//
//   absl::Status FooModule::InitializeForTesting() {
//     // In this example we don't need to do anything different for testing environments, so we
//     // just defer to `Initialize`.
//     return Initialize();
//   }
//
//
// Alternatively you can instantiate the module as a static field of the module class, which allows
// you to make the constructor private and prevent further accidental instantiations.
//
// foo.h:
//
//   class FooModule : public tsdb2::init::BaseModule {
//    public:
//     static FooModule *Get() { return instance_.Get(); }
//
//     absl::Status Initialize() override;
//     absl::Status InitializeForTesting() override;
//
//    private:
//     friend class tsdb2::common::NoDestructor<FooModule>;
//     static tsdb2::common::NoDestructor<FooModule> instance_;
//
//     explicit FooModule();
//   };
//
//
// foo.cc:
//
//   tsdb2::common::NoDestructor<FooModule> FooModule::instance_;
//
//   FooModule::FooModule() : BaseModule("foo") {
//     tsdb2::init::RegisterModule(this, BarModule::Get(), BazModule::Get());
//   }
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
// The default implementation of `Initialize` does nothing and returns OK, while the default
// implementation of `InitializeForTesting` simply defers to `Initialize`. Because of this you don't
// need to always provide explicit implementations.
class BaseModule {
 public:
  virtual ~BaseModule() = default;

  std::string_view name() const { return name_; }

  virtual absl::Status Initialize() { return absl::OkStatus(); }
  virtual absl::Status InitializeForTesting() { return Initialize(); }

 protected:
  explicit BaseModule(std::string_view const name) : name_(name) {}

 private:
  BaseModule(BaseModule const &) = delete;
  BaseModule &operator=(BaseModule const &) = delete;
  BaseModule(BaseModule &&) = delete;
  BaseModule &operator=(BaseModule &&) = delete;

  std::string name_;
};

}  // namespace init
}  // namespace tsdb2

#endif  // __TSDB2_SERVER_BASE_MODULE_H__
