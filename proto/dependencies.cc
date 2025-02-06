#include "proto/dependencies.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"

namespace tsdb2 {
namespace proto {
namespace internal {

void DependencyManager::AddNode(PathView const path) {
  CHECK(!path.empty());
  std::string_view const front = path.front();
  if (path.size() > 1) {
    dependencies_.try_emplace(front);
    auto const [it, unused] = inner_dependencies_.try_emplace(front);
    it->second.AddNode(path.subspan(1));
  } else {
    dependencies_.try_emplace(front);
    inner_dependencies_.try_emplace(front);
  }
}

void DependencyManager::AddDependency(PathView const dependent, PathView const dependee,
                                      std::string_view const edge_name) {
  CHECK(!dependent.empty());
  CHECK(!dependee.empty());
  if (dependent.size() > 1 && dependee.size() > 1 && dependent.front() == dependee.front()) {
    std::string_view const ancestor = dependent.front();
    dependencies_.try_emplace(ancestor);
    auto const [it, unused] = inner_dependencies_.try_emplace(ancestor);
    return it->second.AddDependency(dependent.subspan(1), dependee.subspan(1), edge_name);
  } else {
    auto const [it, unused] = dependencies_.try_emplace(dependent.front());
    it->second.insert_or_assign(edge_name, dependee.front());
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

std::vector<std::string> DependencyManager::MakeOrder(PathView const base_path) const {
  DependencyManager const* manager = this;
  for (auto const& component : base_path) {
    auto const it = manager->inner_dependencies_.find(component);
    if (it != manager->inner_dependencies_.end()) {
      manager = &(it->second);
    } else {
      return {};
    }
  }
  return OrderMaker(*manager).Run();
}

void DependencyManager::CycleFinder::Run(std::string_view const node,
                                         std::string_view const edge_name) && {
  auto const [unused_it, inserted] = visited_->emplace(node);
  if (inserted) {
    RunInternal(node, edge_name);
  }
}

bool DependencyManager::CycleFinder::PushFrame(std::string_view const node,
                                               std::string_view const edge) {
  auto const [unused_it, inserted] = path_.emplace(node);
  if (!inserted) {
    cycles_->emplace_back(stack_);
    return false;
  }
  stack_.emplace_back(MakePath(base_path_, node), edge);
  return true;
}

void DependencyManager::CycleFinder::PopFrame(std::string_view const node) {
  stack_.pop_back();
  path_.erase(node);
}

void DependencyManager::CycleFinder::RunInternal(std::string_view const node,
                                                 std::string_view const edge_name) {
  auto const maybe_frame = CycleFrame::Create(this, node, edge_name);
  if (maybe_frame) {
    auto const it = parent_.dependencies_.find(node);
    if (it != parent_.dependencies_.end()) {
      for (auto const& [edge, dependee] : it->second) {
        RunInternal(dependee, edge);
      }
    }
  }
}

std::optional<DependencyManager::CycleFrame> DependencyManager::CycleFrame::Create(
    CycleFinder* const parent, std::string_view const node, std::string_view const edge) {
  if (parent->PushFrame(node, edge)) {
    return std::make_optional<CycleFrame>(parent, node);
  } else {
    return std::nullopt;
  }
}

DependencyManager::CycleFrame::~CycleFrame() {
  if (parent_ != nullptr) {
    parent_->PopFrame(node_);
  }
}

DependencyManager::CycleFrame::CycleFrame(CycleFrame&& other) noexcept
    : parent_(other.parent_), node_(other.node_) {
  other.parent_ = nullptr;
}

std::vector<std::string> DependencyManager::OrderMaker::Run() && {
  for (auto const& root : parent_.GetRoots()) {
    RunInternal(root);
  }
  return std::move(order_);
}

void DependencyManager::OrderMaker::RunInternal(std::string_view const node) {
  auto const [unused_it, inserted] = visited_.emplace(node);
  if (!inserted) {
    return;
  }
  auto const it = parent_.dependencies_.find(node);
  if (it != parent_.dependencies_.end()) {
    for (auto const& [unused_edge, dependee] : it->second) {
      RunInternal(dependee);
    }
  }
  order_.emplace_back(node);
}

DependencyManager::Path DependencyManager::MakePath(PathView const base_path,
                                                    std::string_view const component) {
  Path path{base_path.size() + 1};
  path.insert(path.end(), base_path.begin(), base_path.end());
  path.emplace_back(component);
  return path;
}

std::vector<std::string> DependencyManager::GetRoots() const {
  absl::btree_set<std::string> nodes;
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
