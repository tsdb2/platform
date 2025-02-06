#include "proto/dependencies.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"

namespace tsdb2 {
namespace proto {
namespace internal {

void DependencyManager::AddDependency(PathView const dependent, PathView const dependee,
                                      std::string_view const edge_name) {
  CHECK_GT(dependent.size(), 0);
  CHECK_GT(dependee.size(), 0);
  if (dependent.size() > 1 && dependee.size() > 1 && dependent[0] == dependee[0]) {
    std::string_view const component = dependent[0];
    dependencies_.try_emplace(component);
    auto const [it, unused] = inner_dependencies_.try_emplace(component);
    return it->second.AddDependency(dependent.subspan(1), dependee.subspan(1), edge_name);
  } else {
    auto const [it, unused] = dependencies_.try_emplace(dependent[0]);
    it->second.insert_or_assign(edge_name, dependee[0]);
  }
}

std::vector<DependencyManager::Cycle> DependencyManager::FindCycles(
    PathView const base_path) const {
  DependencyManager const* manager = this;
  for (auto const& component : base_path) {
    auto const it = manager->inner_dependencies_.find(component);
    if (it != manager->inner_dependencies_.end()) {
      manager = &(it->second);
    } else {
      return {};
    }
  }
  std::vector<Cycle> cycles;
  absl::flat_hash_set<std::string> visited;
  for (auto const& root : manager->GetRoots()) {
    auto const it = manager->dependencies_.find(root);
    if (it != manager->dependencies_.end()) {
      for (auto const& [edge_name, unused] : it->second) {
        CycleFinder(*manager, base_path, &cycles, &visited).Run(root, edge_name);
      }
    }
  }
  return cycles;
}

// TODO

void DependencyManager::CycleFinder::Run(std::string_view const node,
                                         std::string_view const edge_name) && {
  auto const [unused_it, inserted] = visited_->emplace(node);
  if (inserted) {
    RunInternal(node, edge_name);
  }
}

void DependencyManager::CycleFinder::RunInternal(std::string_view const node,
                                                 std::string_view const edge_name) {
  auto const [unused_it, inserted] = path_.emplace(node);
  if (!inserted) {
    cycles_->emplace_back(stack_);
    return;
  }
  stack_.emplace_back(MakePath(base_path_, node), edge_name);
  auto const it = parent_.dependencies_.find(node);
  if (it != parent_.dependencies_.end()) {
    for (auto const& [edge, dependee] : it->second) {
      RunInternal(dependee, edge);
    }
  }
  stack_.pop_back();
  path_.erase(node);
}

DependencyManager::Path DependencyManager::MakePath(PathView const base_path,
                                                    std::string_view const component) {
  Path path{base_path.size() + 1};
  path.insert(path.end(), base_path.begin(), base_path.end());
  path.emplace_back(component);
  return path;
}

std::vector<std::string> DependencyManager::GetRoots() const {
  absl::flat_hash_set<std::string> nodes{dependencies_.size()};
  for (auto const& [node, unused] : dependencies_) {
    nodes.emplace(node);
  }
  for (auto const& [unused_node, dependencies] : dependencies_) {
    for (auto const& [unused_edge, dependee] : dependencies) {
      nodes.erase(dependee);
    }
  }
  return {nodes.begin(), nodes.end()};
}

}  // namespace internal
}  // namespace proto
}  // namespace tsdb2
