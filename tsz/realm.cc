#include "tsz/realm.h"

#include <cstdint>
#include <string_view>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/synchronization/mutex.h"
#include "common/no_destructor.h"
#include "common/reffed_ptr.h"

namespace tsz {

namespace {

// Realm name for most metrics.
inline std::string_view constexpr kDefaultRealmName = "default";

// Realm name for metamonitoring metrics.
inline std::string_view constexpr kMetaRealmName = "meta";

// Realm name for metrics whose cardinality is too high.
inline std::string_view constexpr kHugeRealmName = "huge";

}  // namespace

ABSL_CONST_INIT absl::Mutex Realm::set_mutex_{absl::kConstInit};
absl::flat_hash_set<Realm const*, Realm::Hash, Realm::Eq> Realm::realms_;

tsdb2::common::reffed_ptr<Realm const> Realm::Default() {
  static tsdb2::common::NoDestructor<Realm> const kDefaultRealm{kDefaultRealmName};
  return kDefaultRealm->get_ref();
}

tsdb2::common::reffed_ptr<Realm const> Realm::Meta() {
  static tsdb2::common::NoDestructor<Realm> const kMetaRealm{kMetaRealmName};
  return kMetaRealm->get_ref();
}

tsdb2::common::reffed_ptr<Realm const> Realm::Huge() {
  static tsdb2::common::NoDestructor<Realm> const kHugeRealm{kHugeRealmName};
  return kHugeRealm->get_ref();
}

Realm::Realm(std::string_view const name) : name_(name) {
  absl::MutexLock lock{&set_mutex_};
  auto const [it, inserted] = realms_.emplace(this);
  CHECK(inserted) << "tsz realm \"" << absl::CEscape(name_) << "\" is already defined!";
}

Realm::~Realm() {
  absl::MutexLock(  // NOLINT(bugprone-unused-raii)
      &mutex_, absl::Condition(this, &Realm::is_not_referenced));
  absl::MutexLock lock{&set_mutex_};
  realms_.erase(this);
}

intptr_t Realm::ref_count() const {
  absl::MutexLock lock{&mutex_};
  return ref_count_;
}

void Realm::Ref() const {
  absl::MutexLock lock{&mutex_};
  ++ref_count_;
}

bool Realm::Unref() const {
  absl::MutexLock lock{&mutex_};
  return --ref_count_ == 0;
}

absl::StatusOr<tsdb2::common::reffed_ptr<Realm const>> Realm::GetByName(
    std::string_view const name) {
  {
    absl::MutexLock lock{&set_mutex_};
    auto const it = realms_.find(name);
    if (it != realms_.end()) {
      return (*it)->get_ref();
    }
  }
  return absl::NotFoundError(name);
}

}  // namespace tsz
