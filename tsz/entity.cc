#include "tsz/entity.h"

#include "common/no_destructor.h"

namespace tsz {

Entity<> const& GetDefaultEntity() {
  static tsdb2::common::NoDestructor<Entity<>> const kDefaultEntity;
  return *kDefaultEntity;
}

}  // namespace tsz
