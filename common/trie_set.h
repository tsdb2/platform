#ifndef __TSDB2_COMMON_TRIE_SET_H__
#define __TSDB2_COMMON_TRIE_SET_H__

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace common {

// trie_set is a set of strings implemented as a compressed trie, aka a radix tree. The provided API
// is similar to std::set<std::string>.
template <typename Allocator = std::allocator<std::string>>
class trie_set {
 private:
  class Node;

  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;

  struct NodeDeleter {
    explicit NodeDeleter() = default;
    NodeDeleter(NodeDeleter const&) = default;
    NodeDeleter& operator=(NodeDeleter const&) = default;
    NodeDeleter(NodeDeleter&&) noexcept = default;
    NodeDeleter& operator=(NodeDeleter&&) noexcept = default;
    void operator()(Node* const ptr) const { ptr->Delete(); }
  };

  using NodePtr = std::unique_ptr<Node, NodeDeleter>;

 public:
  using key_type = std::string;
  using value_type = key_type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using allocator_type = NodeAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

  // TODO: iterator type aliases

  class node_type {
   public:
    explicit node_type() = default;

    node_type(node_type&&) noexcept = default;
    node_type& operator=(node_type&&) noexcept = default;

    [[nodiscard]] bool empty() const { return !ptr_; }
    explicit operator bool() const noexcept { return ptr_; }

    allocator_type get_allocator() const { ptr_->parent_->get_allocator(); }

    // TODO: value() getter

    void swap(node_type& other) noexcept { ptr_.swap(other.ptr_); }
    friend void swap(node_type& lhs, node_type& rhs) noexcept { lhs.swap(rhs); }

   private:
    node_type(node_type const&) = delete;
    node_type& operator=(node_type const&) = delete;

    NodePtr ptr_;
  };

  // TODO: insert_return_type

  trie_set() = default;

  explicit trie_set(Allocator const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {}

  template <typename InputIt>
  trie_set(InputIt first, InputIt last, Allocator const& alloc = Allocator())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    for (auto it = first; it != last; ++it) {
      // TODO
    }
  }

  trie_set(trie_set const& other)
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        root_(other.root_),
        size_(other.size_) {}

  trie_set(trie_set const& other, Allocator const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        root_(other.root_),
        size_(other.size_) {}

  trie_set(trie_set&& other) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        root_(std::move(other.root_)),
        size_(other.size_) {}

  trie_set(trie_set&& other, Allocator const& alloc) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        root_(std::move(other.root_)),
        size_(other.size_) {}

  trie_set(std::initializer_list<std::string> const init, Allocator const& alloc = Allocator())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    for (auto const& element : init) {
      // TODO
    }
  }

  trie_set& operator=(trie_set const& other) {
    alloc_ = allocator_traits::select_on_container_copy_construction(other.alloc_);
    root_ = other.root_;
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(trie_set&& other) {
    alloc_ = std::move(other.alloc_);
    root_ = std::move(other.root_);
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(std::initializer_list<std::string> const init) {
    for (auto const& element : init) {
      // TODO
    }
    return *this;
  }

  allocator_type get_allocator() const noexcept {
    return allocator_traits::select_on_container_copy_construction(alloc_);
  }

  // TODO: iterations

  [[nodiscard]] bool empty() const noexcept { return !size_; }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return allocator_traits::max_size(alloc_); }

  void clear() noexcept {
    root_.Clear();
    size_ = 0;
  }

  // TODO

 private:
  class Node {
   public:
    explicit Node(trie_set* const parent) : parent_(parent) {}

    Node(Node const& other) { *this = other; }

    Node& operator=(Node const& other) {
      children_.clear();
      children_.reserve(other.children_.size());
      for (auto const& other_child : other.children_) {
        auto child = parent_->MakeNode(parent_);
        *child = other_child;
        children_.emplace(std::move(child));
      }
      leaf_ = other.leaf_;
      return *this;
    }

    Node(Node&& other) noexcept { *this = std::move(other); }

    Node& operator=(Node&& other) noexcept {
      children_ = std::move(other.children_);
      leaf_ = other.leaf_;
      return *this;
    }

    bool is_leaf() const { return leaf_; }
    void set_leaf(bool const value) { leaf_ = value; }

    void Clear() {
      children_.clear();
      leaf_ = false;
    }

    void Delete() { parent_->DeleteNode(this); }

   private:
    friend class node_type;

    trie_set* const parent_;
    flat_map<std::string, NodePtr> children_;
    bool leaf_ = false;
  };

  NodePtr MakeNode() {
    auto const ptr = std::allocator_traits<NodeAllocator>::allocate(alloc_, 1);
    std::allocator_traits<NodeAllocator>::construct(alloc_, ptr);
    return ptr;
  }

  void DeleteNode(Node* const ptr) {
    std::allocator_traits<NodeAllocator>::destroy(alloc_, ptr);
    std::allocator_traits<NodeAllocator>::deallocate(alloc_, ptr, 1);
  }

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS NodeAllocator alloc_;

  Node root_{this};
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  //
