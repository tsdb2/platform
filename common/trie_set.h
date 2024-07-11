#ifndef __TSDB2_COMMON_TRIE_SET_H__
#define __TSDB2_COMMON_TRIE_SET_H__

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "common/raw_trie.h"
#include "common/re.h"

namespace tsdb2 {
namespace common {

// trie_set is a set of strings implemented as a compressed trie, aka a radix tree. The provided API
// is similar to `std::set<std::string>`.
//
// Notable differences between std::set<std::string> and trie_set:
//
//  * Node handles are not supported. That is because, by definition, a trie node doesn't have all
//    the information about its key, so most of the node API wouldn't make sense.
//  * The worst-case space complexity of our iterators is linear in the length of the stored string.
//    trie_set iterators are cheap to move but relatively expensive to copy.
//  * Iterators are not bidirectional. Monodirectional reverse iterators are still provided, but
//    providing fully bidirectional ones would entail significant additional complexity.
//  * An `emplace` method is not provided because in order to be inserted in the trie a string must
//    be split, so it cannot be emplaced.
//
template <typename Allocator = std::allocator<std::string>>
class trie_set {
 private:
  using Node = internal::TrieNode</*Label=*/bool, Allocator>;
  using NodeSet = typename Node::NodeSet;

 public:
  using key_type = std::string;
  using value_type = key_type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using allocator_type = typename Node::EntryAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
  using iterator = typename Node::ConstIterator;
  using const_iterator = typename Node::ConstIterator;
  using reverse_iterator = typename Node::ConstReverseIterator;
  using const_reverse_iterator = typename Node::ConstReverseIterator;
  using filtered_iterator = typename Node::ConstFilteredIterator;
  using const_filtered_iterator = typename Node::ConstFilteredIterator;
  using reverse_filtered_iterator = typename Node::ConstReverseFilteredIterator;
  using const_reverse_filtered_iterator = typename Node::ConstReverseFilteredIterator;

  class filtered_view : public Node::FilteredView {
   public:
    using key_type = std::string;
    using value_type = key_type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using allocator_type = typename Node::EntryAllocator;
    using allocator_traits = std::allocator_traits<allocator_type>;
    using reference = value_type&;
    using const_reference = value_type const&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
    using iterator = typename Node::ConstFilteredIterator;
    using const_iterator = typename Node::ConstFilteredIterator;
    using reverse_iterator = typename Node::ConstReverseFilteredIterator;
    using const_reverse_iterator = typename Node::ConstReverseFilteredIterator;

    filtered_view(filtered_view const&) = default;
    filtered_view& operator=(filtered_view const&) = default;
    filtered_view(filtered_view&&) noexcept = default;
    filtered_view& operator=(filtered_view&&) noexcept = default;

    using Node::FilteredView::begin;
    using Node::FilteredView::cbegin;
    using Node::FilteredView::cend;
    using Node::FilteredView::crbegin;
    using Node::FilteredView::crend;
    using Node::FilteredView::end;
    using Node::FilteredView::rbegin;
    using Node::FilteredView::rend;

   private:
    friend class trie_set;
    explicit filtered_view(typename Node::FilteredView&& view)
        : Node::FilteredView(std::move(view)) {}
  };

  trie_set() = default;

  explicit trie_set(allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {}

  template <typename InputIt>
  trie_set(InputIt first, InputIt last, allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    insert(first, last);
  }

  trie_set(trie_set const& other)
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_set(trie_set const& other, allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_set(trie_set&& other) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_set(trie_set&& other, allocator_type const& alloc) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_set(std::initializer_list<std::string> const init,
           allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    insert(init);
  }

  trie_set& operator=(trie_set const& other) {
    alloc_ = allocator_traits::select_on_container_copy_construction(other.alloc_);
    roots_ = other.roots_;
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(trie_set&& other) noexcept {
    alloc_ = std::move(other.alloc_);
    roots_ = std::move(other.roots_);
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(std::initializer_list<std::string> const init) {
    clear();
    insert(init);
    return *this;
  }

  allocator_type get_allocator() const noexcept {
    return allocator_traits::select_on_container_copy_construction(alloc_);
  }

  iterator begin() noexcept { return Node::cbegin(roots_); }
  const_iterator begin() const noexcept { return Node::cbegin(roots_); }
  const_iterator cbegin() const noexcept { return Node::cbegin(roots_); }
  iterator end() noexcept { return Node::cend(); }
  const_iterator end() const noexcept { return Node::cend(); }
  const_iterator cend() const noexcept { return Node::cend(); }

  reverse_iterator rbegin() noexcept { return Node::crbegin(roots_); }
  const_reverse_iterator rbegin() const noexcept { return Node::crbegin(roots_); }
  const_reverse_iterator crbegin() const noexcept { return Node::crbegin(roots_); }
  reverse_iterator rend() noexcept { return Node::crend(); }
  const_reverse_iterator rend() const noexcept { return Node::crend(); }
  const_reverse_iterator crend() const noexcept { return Node::crend(); }

  template <typename H>
  friend H AbslHashValue(H h, trie_set const& set) {
    return H::combine(std::move(h), set.roots_);
  }

  friend bool operator==(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ == rhs.roots_;
  }

  friend bool operator!=(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ != rhs.roots_;
  }

  friend bool operator<(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ < rhs.roots_;
  }

  friend bool operator<=(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ <= rhs.roots_;
  }

  friend bool operator>(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ > rhs.roots_;
  }

  friend bool operator>=(trie_set const& lhs, trie_set const& rhs) {
    return lhs.roots_ >= rhs.roots_;
  }

  [[nodiscard]] bool empty() const noexcept { return root().IsEmpty(); }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return root().children_.max_size(); }

  void clear() noexcept {
    root().Clear();
    size_ = 0;
  }

  std::pair<iterator, bool> insert(std::string_view const key) {
    auto result = Node::Insert(&roots_, key, true);
    if (result.second) {
      ++size_;
    }
    return result;
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<std::string> const init) {
    for (auto const& value : init) {
      insert(value);
    }
  }

  iterator erase(const_iterator pos) {
    auto it = Node::Remove(&roots_, std::move(pos));
    --size_;
    return it;
  }

  void erase_fast(const_iterator const& pos) {
    Node::RemoveFast(&roots_, pos);
    --size_;
  }

  iterator erase(const_iterator first, const_iterator const& last) {
    if (last.is_end()) {
      while (first != last) {
        first = erase(std::move(first));
      }
    } else {
      // The `erase` calls will invalidate `last` if it's not the end iterator, so we must compare
      // the keys.
      auto const last_key = *last;
      while (*first != last_key) {
        first = erase(std::move(first));
      }
    }
    return first;
  }

  size_type erase(std::string_view const key) {
    if (root().Remove(key)) {
      --size_;
      return 1;
    } else {
      return 0;
    }
  }

  void swap(trie_set& other) {
    if (allocator_traits::propagate_on_container_swap::value) {
      std::swap(alloc_, other.alloc_);
    }
    std::swap(roots_, other.roots_);
    std::swap(size_, other.size_);
  }

  size_type count(std::string_view const key) const { return root().Contains(key) ? 1 : 0; }

  const_iterator find(std::string_view const key) const { return Node::FindConst(roots_, key); }

  // Provides a view of the trie filtered by a regular expression, allowing the user to enumerate
  // only the elements whose key matches the regular expression.
  //
  // Example:
  //
  //   trie_set const ts{"lorem", "ipsum", "dolor", "color"};
  //   for (auto const& key : ts.filter(RE::Create(".*lor"))) {
  //     std::cout << key << std::endl;
  //   }
  //
  // The above example will print "dolor" and "color".
  //
  // Under the hood `filtered_view` uses efficient algorithms that can entirely skip mismatching
  // subtrees, so it's much more efficient than just iterating over all elements and checking each
  // one against the regular expression.
  //
  // NOTE: the `filtered_view` refers to the parent trie internally, so the trie must not be moved
  // or destroyed while one or more `filtered_view` instances exist. It is okay to move and copy the
  // `filtered_view` itself.
  filtered_view filter(RE re) const { return filtered_view(Node::Filter(roots_, std::move(re))); }

  bool contains(std::string_view const key) const { return root().Contains(key); }

  // Checks the presence of any strings that match the provided regular expression.
  bool contains(RE const& re) const { return root().Contains(re); }

  iterator lower_bound(std::string_view const key) { return Node::LowerBoundConst(roots_, key); }

  const_iterator lower_bound(std::string_view const key) const {
    return Node::LowerBoundConst(roots_, key);
  }

  iterator upper_bound(std::string_view const key) { return Node::UpperBound(roots_, key); }

  const_iterator upper_bound(std::string_view const key) const {
    return Node::UpperBoundConst(roots_, key);
  }

  std::pair<iterator, iterator> equal_range(std::string_view const key) {
    return std::make_pair(lower_bound(key), upper_bound(key));
  }

  std::pair<const_iterator, const_iterator> equal_range(std::string_view const key) const {
    return std::make_pair(lower_bound(key), upper_bound(key));
  }

 private:
  // Returns the root node of the tree (see the comment on `roots_` below).
  Node& root() { return roots_.begin()->second; }
  Node const& root() const { return roots_.begin()->second; }

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS allocator_type alloc_;

  // To facilitate the implementation of the iterator advancement algorithm we maintain a list of
  // roots rather than a single root so that we can always rely on `NodeSet` iterators at every
  // level of recursion, but in reality `roots_` must always contain exactly one element, the real
  // root. The empty string we're using here as a key is irrelevant.
  NodeSet roots_{{"", Node(alloc_, /*leaf=*/false)}};

  // Number of strings in the trie.
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TRIE_SET_H__
