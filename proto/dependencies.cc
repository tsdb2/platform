#include "proto/dependencies.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

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

bool DependencyManager::HasNode(PathView const path) const {
  if (path.size() > 1) {
    auto const it = inner_dependencies_.find(path.front());
    if (it != inner_dependencies_.end()) {
      return it->second.HasNode(path.subspan(1));
    } else {
      return false;
    }
  } else if (path.empty()) {
    return false;
  } else {
    return dependencies_.contains(path.front());
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
    if (dependee.front() != dependent.front() || dependee.size() == 1) {
      std::string field_path;
      if (dependent.size() > 1) {
        field_path = absl::StrCat(absl::StrJoin(dependent.subspan(1), "."), ".", edge_name);
      } else {
        field_path = std::string(edge_name);
      }
      it->second.insert_or_assign(std::move(field_path), dependee.front());
    }
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
  return CycleFinder(*manager, base_path).Run();
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

std::vector<DependencyManager::Cycle> DependencyManager::CycleFinder::Run() && {
  for (auto const& [node, unused] : parent_.dependencies_) {
    if (!visited_.contains(node)) {
      RunInternal(node);
    }
  }
  return std::move(cycles_);
}

bool DependencyManager::CycleFinder::PushPathFrame(std::string_view const node) {
  auto const [it, inserted] = path_.try_emplace(node, stack_.size());
  if (!inserted) {
    cycles_.emplace_back(stack_.begin() + it->second, stack_.end());
    return false;
  }
  visited_.emplace(node);
  return true;
}

void DependencyManager::CycleFinder::PopPathFrame(std::string_view const node) {
  path_.erase(node);
}

void DependencyManager::CycleFinder::PushStackFrame(std::string_view const node,
                                                    std::string_view const edge) {
  stack_.emplace_back(MakePath(base_path_, node), edge);
}

void DependencyManager::CycleFinder::PopStackFrame() { stack_.pop_back(); }

void DependencyManager::CycleFinder::RunInternal(std::string_view const node) {
  auto const maybe_path_frame = CyclePathFrame::Create(this, node);
  if (maybe_path_frame) {
    auto const it = parent_.dependencies_.find(node);
    if (it != parent_.dependencies_.end()) {
      for (auto const& [edge, dependee] : it->second) {
        CycleStackFrame stack_frame{this, node, edge};
        RunInternal(dependee);
      }
    }
  }
}

std::optional<DependencyManager::CyclePathFrame> DependencyManager::CyclePathFrame::Create(
    CycleFinder* const parent, std::string_view const node) {
  if (parent->PushPathFrame(node)) {
    return std::make_optional<CyclePathFrame>(parent, node);
  } else {
    return std::nullopt;
  }
}

DependencyManager::CyclePathFrame::~CyclePathFrame() {
  if (parent_ != nullptr) {
    parent_->PopPathFrame(node_);
  }
}

DependencyManager::CyclePathFrame::CyclePathFrame(CyclePathFrame&& other) noexcept
    : parent_(other.parent_), node_(other.node_) {
  other.parent_ = nullptr;
}

DependencyManager::CycleStackFrame::CycleStackFrame(CycleFinder* const parent,
                                                    std::string_view const node,
                                                    std::string_view const edge)
    : parent_(parent) {
  parent_->PushStackFrame(node, edge);
}

DependencyManager::CycleStackFrame::~CycleStackFrame() {
  if (parent_ != nullptr) {
    parent_->PopStackFrame();
  }
}

DependencyManager::CycleStackFrame::CycleStackFrame(CycleStackFrame&& other) noexcept
    : parent_(other.parent_) {
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
  Path path;
  path.reserve(base_path.size() + 1);
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
