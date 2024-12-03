#ifndef __TSDB2_COMMON_PROMISE_H__
#define __TSDB2_COMMON_PROMISE_H__

#include <algorithm>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/functional/function_ref.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

namespace promise_internal {

template <typename Value>
struct PromiseTraits {
  using StatusOrValue = absl::StatusOr<Value>;
  using Maybe = std::optional<StatusOrValue>;
};

template <>
struct PromiseTraits<void> {
  using StatusOrValue = absl::Status;
  using Maybe = std::optional<StatusOrValue>;
};

}  // namespace promise_internal

// Here we provide an implementation of promises that don't block the calling thread, suitable for
// asynchronous programming. This implementation is fully thread-safe.
//
// Unlike `std::promise` this class is fully asynchronous and doesn't require waiting in any
// circumstances. It's more similar to EcmaScript's promises. Example:
//
//   Promise<Foo> LoadFoo() {
//     return Promise<Foo>([](auto resolve) {
//       // NOTE: `resolve` is a movable functor that can be called at any time as long as the
//       // returned Promise object is alive. It's okay (and it's actually the most frequent use
//       // case) to call it much later, e.g. after receiving an RPC response.
//       resolve(foo);
//     });
//   }
//
//   Promise<Bar> BuildBarFromFoo(Foo foo) {
//     return Promise<Bar>([](auto resolve) {
//       // NOTE: same as above, `resolve` doesn't have to be called in this scope.
//       resolve(Bar(foo));
//     });
//   }
//
//   Promise<Baz> DoEverything() {
//     return LoadFoo()
//         .Then([](absl::StatusOr<Foo> status_or_foo) {
//           if (status_or_foo.ok()) {
//             return BuildBarFromFoo(std::move(status_or_foo).value());
//           } else {
//             return Promise<Bar>::Reject(std::move(status_or_foo).status());
//           }
//         })
//         .Then([](absl::StatusOr<Bar> status_or_bar) -> absl::StatusOr<Baz> {
//           if (status_or_bar.ok()) {
//             auto const& bar = status_or_bar.value();
//             return bar.CreateBaz();
//           } else {
//             return std::move(status_or_bar).status();
//           }
//         });
//   }
//
//
// The resolver function provided to the constructor callback is the interface to the promise for
// the promise's implementor, e.g. `LoadFoo` and `BuildBarFromFoo` in the example above. It's a
// simple closure that takes a single `absl::StatusOr<Value>` argument, with `Value` being the type
// of the promised value, and returns `void`. It can be called at any time as long as the
// constructed `Promise` remains alive, but it must be called only once. If it's never called, any
// installed `Then` callbacks will never run.
//
// The returned `Promise` object provides a single `Then` method that allows installing a callback
// to execute when the promise is resolved. The provided callback may have several different
// signatures:
//
//   * void(absl::StatusOr<Value>)
//   * void(Value)
//   * NextValue(absl::StatusOr<Value>)
//   * NextValue(Value)
//   * absl::Status(absl::StatusOr<Value>)
//   * absl::Status(Value)
//   * absl::StatusOr<NextValue>(absl::StatusOr<Value>)
//   * absl::StatusOr<NextValue>(Value)
//   * Promise<NextValue>(absl::StatusOr<Value>)
//   * Promise<NextValue>(Value)
//
//
// The last two signatures allow chaining many promises into one, like we did in the `DoEverything`
// function of the example above.
//
// Functions with any of the signatures taking an `absl::StatusOr` will be invoked regardless of the
// success status of the promise (the `absl::StatusOr` passed to the resolver will be forwarded to
// the `Then` callback as-is). Conversely, if a `Then` function expects a straight value with no
// status, it will not be invoked in case of error; instead the error status will be propagated
// through the chain until a `StatusOr` callback is encountered.
//
// A promise might as well be of type `void` (i.e. `Promise<void>`), in which case the resolver
// takes a simple `absl::Status` and the above signatures for the `Then` callback become as follows:
//
//   * void(absl::Status)
//   * void()
//   * NextValue(absl::Status)
//   * NextValue()
//   * absl::Status(absl::Status)
//   * absl::Status()
//   * absl::StatusOr<NextValue>(absl::Status)
//   * absl::StatusOr<NextValue>()
//   * Promise<NextValue>(absl::Status)
//   * Promise<NextValue>()
//
//
// Note that if you just want to ignore all errors until the end of the chain you can append a void
// promise to handle the status. Exploiting all these capabilities, the `DoEverything` function of
// the above example can be rewritten as follows:
//
//   Promise<void> DoEverything() {
//     return LoadFoo()
//         .Then([](Foo foo) {
//           return BuildBarFromFoo(std::move(foo));
//         })
//         .Then([](Bar bar) {
//           return bar.CreateBaz();
//         })
//         .Then([](Baz baz) {
//           return UseBaz(std::move(baz));
//         })
//         .Then([](absl::Status const status) {
//           LOG_IF(ERROR, !status.ok()) << status;
//         });
//   }
//
//
// Our Promise implementation doesn't have an `Else` method, just `Then`. `Else` is not needed
// because we use `absl::StatusOr` throughout the stack, so we don't need separate "else" functions
// to handle errors. Similarly, the constructor callback implemented by the provider of the promise
// doesn't need a separate `reject` function because any error statuses can be passed to `resolve`.
template <typename Value>
class Promise {
 private:
  using Traits = promise_internal::PromiseTraits<Value>;
  using ThenFn = absl::AnyInvocable<void(typename Traits::StatusOrValue)>;

  // Internal promise implementation.
  //
  // While `Promise` itself acts as a `std::unique_ptr<Impl>`, this class is allocated on the heap
  // and is never moved. We need pointer stability for it because it will be referred to by two (or
  // more) concurrent threads: the one providing the promise and the one consuming it.
  class Impl final {
   public:
    explicit Impl(absl::FunctionRef<void(ThenFn)> const callback) {
      callback(absl::bind_front(&Impl::Resolve, this));
    }

    template <typename NextValue, typename Callback>
    Promise<NextValue> ThenNext(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          resolve(callback(std::move(status_or_value)));
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          if (status_or_value.ok()) {
            resolve(callback(std::move(status_or_value).value()));
          } else {
            resolve(std::move(status_or_value).status());
          }
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback)](absl::Status status) mutable {
          if (status.ok()) {
            resolve(callback());
          } else {
            resolve(std::move(status));
          }
        });
      });
    }

    template <typename Callback>
    Promise<void> ThenVoid(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          callback(std::move(status_or_value));
          resolve(absl::OkStatus());
        });
      });
    }

    template <typename Callback, typename ValueAlias = Value,
              std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
    Promise<void> ThenVoidSkipError(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          if (status_or_value.ok()) {
            callback(std::move(status_or_value).value());
            resolve(absl::OkStatus());
          } else {
            resolve(std::move(status_or_value).status());
          }
        });
      });
    }

    template <typename Callback, typename ValueAlias = Value,
              std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
    Promise<void> ThenVoidSkipError(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback)](absl::Status status) mutable {
          if (status.ok()) {
            callback();
          }
          resolve(std::move(status));
        });
      });
    }

    template <typename Callback>
    Promise<void> ThenStatus(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          resolve(callback(std::move(status_or_value)));
        });
      });
    }

    template <typename Callback, typename ValueAlias = Value,
              std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
    Promise<void> ThenStatusSkipError(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          if (status_or_value.ok()) {
            resolve(callback(std::move(status_or_value).value()));
          } else {
            resolve(std::move(status_or_value).status());
          }
        });
      });
    }

    template <typename Callback, typename ValueAlias = Value,
              std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
    Promise<void> ThenStatusSkipError(Callback&& callback) {
      return Promise<void>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback)](absl::Status status) mutable {
          if (status.ok()) {
            resolve(callback());
          } else {
            resolve(std::move(status));
          }
        });
      });
    }

    template <typename NextValue, typename Callback>
    Promise<NextValue> ThenStatusOrNext(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          resolve(callback(std::move(status_or_value)));
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenStatusOrNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback =
                  std::move(callback)](typename Traits::StatusOrValue status_or_value) mutable {
          if (status_or_value.ok()) {
            resolve(callback(std::move(status_or_value).value()));
          } else {
            resolve(std::move(status_or_value).status());
          }
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenStatusOrNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback)](absl::Status status) mutable {
          if (status.ok()) {
            resolve(callback());
          } else {
            resolve(std::move(status));
          }
        });
      });
    }

    template <typename NextValue, typename Callback>
    Promise<NextValue> ThenPromiseNext(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback),
              child = Promise<void>()](typename Traits::StatusOrValue status_or_value) mutable {
          child = callback(std::move(status_or_value)).Then(std::move(resolve));
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenPromiseNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback),
              child = Promise<void>()](typename Traits::StatusOrValue status_or_value) mutable {
          if (status_or_value.ok()) {
            child = callback(std::move(status_or_value).value()).Then(std::move(resolve));
          } else {
            resolve(std::move(status_or_value).status());
          }
        });
      });
    }

    template <typename NextValue, typename Callback, typename ValueAlias = Value,
              std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
    Promise<NextValue> ThenPromiseNextSkipError(Callback&& callback) {
      return Promise<NextValue>([this, &callback](auto resolve) {
        Then([self = absl::WrapUnique(this), resolve = std::move(resolve),
              callback = std::move(callback),
              child = Promise<void>()](absl::Status status) mutable {
          if (status.ok()) {
            child = callback().Then(std::move(resolve));
          } else {
            resolve(std::move(status));
          }
        });
      });
    }

   private:
    Impl(Impl const&) = delete;
    Impl& operator=(Impl const&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void Resolve(typename Traits::StatusOrValue status_or_value) ABSL_LOCKS_EXCLUDED(mutex_) {
      absl::MutexLock lock{&mutex_};
      if (then_) {
        then_(std::move(status_or_value));
      } else {
        value_ = std::move(status_or_value);
      }
    }

    void Then(ThenFn then) ABSL_LOCKS_EXCLUDED(mutex_) {
      absl::MutexLock lock{&mutex_};
      then_ = std::move(then);
      if (value_.has_value()) {
        typename Traits::Maybe value;
        using std::swap;  // ensure ADL
        swap(value, value_);
        then_(std::move(value).value());
      }
    }

    absl::Mutex mutable mutex_;
    typename Traits::Maybe value_ ABSL_GUARDED_BY(mutex_);
    ThenFn then_ ABSL_GUARDED_BY(mutex_);
  };

  // This is a partially specialized struct that we use to dispatch a `Then` call to the correct
  // `Impl::Then*` method. We need this intermediate struct because we can't partially specialize
  // the `Promise::Then` method itself (partial specialization is not allowed on functions).
  template <typename ThisValue, typename NextValue>
  struct Chainer;

  template <typename NextValue>
  struct Chainer<typename Traits::StatusOrValue, NextValue> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenNext<NextValue>(std::forward<Callback>(callback));
    }
  };

  template <typename NextValue>
  struct Chainer<Value, NextValue> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenNextSkipError<NextValue>(std::forward<Callback>(callback));
    }
  };

  template <>
  struct Chainer<typename Traits::StatusOrValue, void> {
    template <typename Callback>
    Promise<void> operator()(Impl* const parent, Callback&& callback) const {
      return parent->ThenVoid(std::forward<Callback>(callback));
    }
  };

  template <>
  struct Chainer<Value, void> {
    template <typename Callback>
    Promise<void> operator()(Impl* const parent, Callback&& callback) const {
      return parent->ThenVoidSkipError(std::forward<Callback>(callback));
    }
  };

  template <>
  struct Chainer<typename Traits::StatusOrValue, absl::Status> {
    template <typename Callback>
    Promise<void> operator()(Impl* const parent, Callback&& callback) const {
      return parent->ThenStatus(std::forward<Callback>(callback));
    }
  };

  template <>
  struct Chainer<Value, absl::Status> {
    template <typename Callback>
    Promise<void> operator()(Impl* const parent, Callback&& callback) const {
      return parent->ThenStatusSkipError(std::forward<Callback>(callback));
    }
  };

  template <typename NextValue>
  struct Chainer<typename Traits::StatusOrValue, absl::StatusOr<NextValue>> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenStatusOrNext<NextValue>(std::forward<Callback>(callback));
    }
  };

  template <typename NextValue>
  struct Chainer<Value, absl::StatusOr<NextValue>> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenStatusOrNextSkipError<NextValue>(
          std::forward<Callback>(callback));
    }
  };

  template <typename NextValue>
  struct Chainer<typename Traits::StatusOrValue, Promise<NextValue>> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenPromiseNext<NextValue>(std::forward<Callback>(callback));
    }
  };

  template <typename NextValue>
  struct Chainer<Value, Promise<NextValue>> {
    template <typename Callback>
    Promise<NextValue> operator()(Impl* const parent, Callback&& callback) const {
      return parent->template ThenPromiseNextSkipError<NextValue>(std::forward<Callback>(callback));
    }
  };

 public:
  using ResolveFn = ThenFn;

  explicit Promise() = default;

  explicit Promise(absl::FunctionRef<void(ResolveFn)> const callback)
      : impl_(std::make_unique<Impl>(callback)) {}

  Promise(Promise&&) noexcept = default;
  Promise& operator=(Promise&&) noexcept = default;

  void swap(Promise& other) noexcept { std::swap(impl_, other.impl_); }
  friend void swap(Promise& lhs, Promise& rhs) noexcept { lhs.swap(rhs); }

  // Creates a promise that resolves immediately to the specified value.
  template <typename... Args>
  static Promise<Value> Resolve(Args&&... args) {
    return Promise<Value>::template ResolveHelper<Value>(std::forward<Args>(args)...);
  }

  // Creates a promise of type `Value` that rejects immediately with the specified error status.
  //
  // REQUIRES: `status` must be an error.
  static Promise<Value> Reject(absl::Status status) {
    return Promise<Value>([&](auto resolve) { resolve(std::move(status)); });
  }

  template <typename Callback>
  auto Then(Callback&& callback) && {
    return std::move(*this).ThenHelper(std::forward<Callback>(callback));
  }

 private:
  Promise(Promise const&) = delete;
  Promise& operator=(Promise const&) = delete;

  template <typename ValueAlias = Value, std::enable_if_t<!std::is_void_v<ValueAlias>, bool> = true>
  static Promise<ValueAlias> ResolveHelper(ValueAlias&& value) {
    return Promise<ValueAlias>([&](auto resolve) { resolve(std::move(value)); });
  }

  template <typename ValueAlias = Value, std::enable_if_t<std::is_void_v<ValueAlias>, bool> = true>
  static Promise<ValueAlias> ResolveHelper() {
    return Promise<ValueAlias>([&](auto resolve) { resolve(absl::OkStatus()); });
  }

  template <typename Callback, typename ValueAlias = Value,
            std::enable_if_t<std::is_invocable_v<Callback, typename Traits::StatusOrValue> &&
                                 !std::is_void_v<ValueAlias>,
                             bool> = true>
  auto ThenHelper(Callback&& callback) && {
    return Chainer<typename Traits::StatusOrValue,
                   std::remove_cv_t<std::invoke_result_t<Callback, ValueAlias>>>{}(
        impl_.release(), std::forward<Callback>(callback));
  }

  template <typename Callback, typename ValueAlias = Value,
            std::enable_if_t<std::is_invocable_v<Callback, typename Traits::StatusOrValue> &&
                                 std::is_void_v<ValueAlias>,
                             bool> = true>
  auto ThenHelper(Callback&& callback) && {
    return Chainer<typename Traits::StatusOrValue,
                   std::remove_cv_t<std::invoke_result_t<Callback, absl::Status>>>{}(
        impl_.release(), std::forward<Callback>(callback));
  }

  template <typename Callback,
            std::enable_if_t<!std::is_invocable_v<Callback, absl::Status> && !std::is_void_v<Value>,
                             bool> = true>
  auto ThenHelper(Callback&& callback) && {
    return Chainer<Value, std::remove_cv_t<std::invoke_result_t<Callback, Value>>>{}(
        impl_.release(), std::forward<Callback>(callback));
  }

  template <typename Callback,
            std::enable_if_t<!std::is_invocable_v<Callback, absl::Status> && std::is_void_v<Value>,
                             bool> = true>
  auto ThenHelper(Callback&& callback) && {
    return Chainer<void, std::remove_cv_t<std::invoke_result_t<Callback>>>{}(
        impl_.release(), std::forward<Callback>(callback));
  }

  std::unique_ptr<Impl> impl_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_PROMISE_H__
