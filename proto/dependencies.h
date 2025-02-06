#ifndef __TSDB2_PROTO_DEPENDENCIES_H__
#define __TSDB2_PROTO_DEPENDENCIES_H__

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"

namespace tsdb2 {
namespace proto {
namespace internal {

// Keeps track of the dependencies between messages and allows inferring a definition order (for
// both global scope message and nested messages) that doesn't require forward declarations. Also
// allows erroring out when a cycle is detected.
class DependencyManager {
 public:
  using Path = absl::InlinedVector<std::string, 2>;
  using PathView = absl::Span<std::string const>;
  using Cycle = std::vector<std::pair<Path, std::string>>;

  explicit DependencyManager() = default;
  ~DependencyManager() = default;

  DependencyManager(DependencyManager const&) = default;
  DependencyManager& operator=(DependencyManager const&) = default;

  DependencyManager(DependencyManager&&) noexcept = default;
  DependencyManager& operator=(DependencyManager&&) noexcept = default;

  void AddDependency(PathView dependent, PathView dependee, std::string_view edge_name);

  std::vector<Cycle> FindCycles(PathView base_path) const;

  std::vector<std::string> MakeOrder(PathView base_path) const;

 private:
  class CycleFinder {
   public:
    explicit CycleFinder(DependencyManager const& parent, PathView const base_path,
                         std::vector<Cycle>* const cycles,
                         absl::flat_hash_set<std::string>* const visited)
        : parent_(parent), base_path_(base_path), cycles_(cycles), visited_(visited) {}

    ~CycleFinder() = default;

    void Run(std::string_view node, std::string_view edge_name) &&;

   private:
    CycleFinder(CycleFinder const&) = delete;
    CycleFinder& operator=(CycleFinder const&) = delete;
    CycleFinder(CycleFinder&&) = delete;
    CycleFinder& operator=(CycleFinder&&) = delete;

    void RunInternal(std::string_view node, std::string_view edge_name);

    DependencyManager const& parent_;
    PathView const base_path_;
    std::vector<Cycle>* const cycles_;
    absl::flat_hash_set<std::string>* const visited_;
    absl::flat_hash_set<std::string_view> path_;
    Cycle stack_;
  };

  static Path MakePath(PathView base_path, std::string_view component);

  std::vector<std::string> GetRoots() const;

  absl::btree_map<std::string, absl::flat_hash_map<std::string, std::string>> dependencies_;
  absl::flat_hash_map<std::string, DependencyManager> inner_dependencies_;
};

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_DEPENDENCIES_H__
