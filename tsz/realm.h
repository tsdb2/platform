#ifndef __TSDB2_TSZ_REALM_H__
#define __TSDB2_TSZ_REALM_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "gtest/gtest_prod.h"

namespace tsz {

// Represents a realm with the given name.
//
// See the documentation for `tsz::Options::realm` for more information about realms.
//
// WARNING: realm names MUST be unique, so the `Realm` constructor check-fails if another realm with
// the same name already exists. The very goal of this class is to enforce a single `Realm` object
// (and corresponding linker symbol) with a given name in the whole program.
//
// `Realm`s are reference-counted and are managed by `reffed_ptr`. The `~Realm` destructor blocks
// until the reference count drops to zero, similarly to `BlockingRefCounted` (the implementation
// cannot use `BlockingRefCounted` itself for technical reasons). This pattern safe-guards against
// destroying a `Realm` while some metrics still refer to it, but the safest usage of `Realm`s is to
// always instantiate them in global scope with `NoDestructor`. Example:
//
//   tsz::NoDestructor<tsz::Realm> const foobar_realm{"foobar"};
//
//   tsz::NoDestructor<tsz::Counter> counter{"/foo/bar", tsz::Options{
//       .realm{foobar_realm->get_ref()},
//   }};
//
// Note that the metric itself also uses `NoDestructor` for similar reasons.
//
// It's okay to use realms in other compilation units. In that case it may be better to export a
// function that uses the localized static initialization pattern for the realm:
//
//   // unit 1:
//   tsdb2::common::reffed_ptr<Realm const> GetFoobarRealm() {
//     static tsz::NoDestructor<Realm> const kRealm{"foobar"};
//     return kRealm->get_ref();
//   }
//
//   // unit 2:
//   tsz::NoDestructor<tsz::Counter> counter{"/foo/bar", tsz::Options{
//       .realm = GetFoobarRealm(),
//   }};
//
class Realm {
 public:
  // Default realm for most metrics.
  static tsdb2::common::reffed_ptr<Realm const> Default();

  // Realm for metamonitoring metrics.
  static tsdb2::common::reffed_ptr<Realm const> Meta();

  // Realm for metrics with very large cardinality that pose a risk of dropping write RPCs.
  static tsdb2::common::reffed_ptr<Realm const> Huge();

  explicit Realm(std::string_view name) ABSL_LOCKS_EXCLUDED(mutex_);
  ~Realm() ABSL_LOCKS_EXCLUDED(mutex_);

  intptr_t ref_count() const ABSL_LOCKS_EXCLUDED(mutex_);
  void Ref() const ABSL_LOCKS_EXCLUDED(mutex_);
  bool Unref() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the name of the realm.
  std::string_view name() const { return name_; }

  // Returns a `reffed_ptr` pointing to this `Realm`.
  tsdb2::common::reffed_ptr<Realm const> get_ref() const {
    return tsdb2::common::reffed_ptr<Realm const>(this);
  }

 private:
  struct Hash {
    using is_transparent = void;
    size_t operator()(Realm const *const realm) const { return absl::HashOf(realm->name_); }
    size_t operator()(std::string_view const name) const { return absl::HashOf(name); }
  };

  struct Eq {
    using is_transparent = void;
    static std::string_view GetName(Realm const *const realm) { return realm->name_; }
    static std::string_view GetName(std::string_view const name) { return name; }
    template <typename LHS, typename RHS>
    bool operator()(LHS &&lhs, RHS &&rhs) const {
      return GetName(std::forward<LHS>(lhs)) == GetName(std::forward<RHS>(rhs));
    }
  };

  Realm(Realm const &) = delete;
  Realm &operator=(Realm const &) = delete;
  Realm(Realm &&) = delete;
  Realm &operator=(Realm &&) = delete;

  bool is_not_referenced() const ABSL_SHARED_LOCKS_REQUIRED(mutex_) { return ref_count_ == 0; }

  // Retrieves a reference to a `Realm` from the realm name.
  //
  // This is not part of the public API because we require that whenever a piece of user code refers
  // to a realm it must do so by its linker symbol. In other words, the compilation unit defining
  // that realm must somehow export it, e.g. by declaring it as `extern` in the corresponding
  // header.
  //
  // This function is only used internally by friend classes to implement features like specifying
  // realm names in command line flags.
  static absl::StatusOr<tsdb2::common::reffed_ptr<Realm const>> GetByName(std::string_view name);

  static absl::Mutex set_mutex_;
  static absl::flat_hash_set<Realm const *, Hash, Eq> realms_ ABSL_GUARDED_BY(set_mutex_);

  std::string const name_;

  absl::Mutex mutable mutex_;
  intptr_t mutable ref_count_ ABSL_GUARDED_BY(mutex_) = 0;

  FRIEND_TEST(RealmTest, GetByName);
  FRIEND_TEST(RealmTest, GetMissingByName);
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_REALM_H__
