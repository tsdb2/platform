// This header provides `flat_set`, a drop-in replacement for std::set backed by a specified data
// structure. When backed by an std::vector, flat_set behaves like a sorted array and is well suited
// for read-mostly use cases and/or small-ish data structures. In those cases, being allocated in a
// single heap block makes the data much more cache-friendly and efficient. For any uses cases with
// large (but still read-mostly) datasets, you may want to use std::deque instead of std::vector.
//
// TODO: replace with `std::flat_set` when we have C++23.

#ifndef __TSDB2_COMMON_FLAT_SET_H__
#define __TSDB2_COMMON_FLAT_SET_H__

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "common/flat_container_internal.h"
#include "common/to_array.h"

namespace tsdb2 {
namespace common {

template <typename Key, typename Compare = internal::DefaultCompareT<Key>,
          typename Representation = std::vector<Key>>
class flat_set {
 public:
  class node {
   public:
    using value_type = Key;

    constexpr node() noexcept : value_(std::nullopt) {}
    explicit node(value_type&& value) : value_(std::move(value)) {}

    node(node&&) noexcept = default;
    node& operator=(node&&) noexcept = default;

    [[nodiscard]] bool empty() const noexcept { return !value_.has_value(); }
    explicit operator bool() const noexcept { return value_.has_value(); }

    value_type& value() const& { return const_cast<value_type&>(*value_); }
    value_type&& value() && { return *std::move(value_); }

    void swap(node& other) noexcept { value_.swap(other.value_); }
    friend void swap(node& lhs, node& rhs) noexcept { lhs.swap(rhs); }

   private:
    node(node const&) = delete;
    node& operator=(node const&) = delete;

    std::optional<value_type> value_;
  };

  using key_type = Key;
  using value_type = Key;
  using representation_type = Representation;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using key_compare = Compare;
  using value_compare = Compare;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename Representation::pointer;
  using const_pointer = typename Representation::const_pointer;
  using iterator = typename Representation::iterator;
  using const_iterator = typename Representation::const_iterator;
  using reverse_iterator = typename Representation::reverse_iterator;
  using const_reverse_iterator = typename Representation::const_reverse_iterator;
  using node_type = node;

  struct insert_return_type {
    iterator position;
    bool inserted;
    node_type node;
  };

  template <typename Arg>
  using key_arg_t =
      typename internal::flat_container::key_arg<Compare>::template type<key_type, Arg>;

  constexpr flat_set() : flat_set(Compare()) {}

  explicit constexpr flat_set(SortedDeduplicatedContainer, Representation rep,
                              Compare const& comp = Compare())
      : comp_(comp), rep_(std::move(rep)) {}

  explicit constexpr flat_set(Compare const& comp) : comp_(comp) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  explicit flat_set(Compare const& comp, typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  explicit flat_set(typename Alias::allocator_type const& alloc) : rep_(alloc) {}

  template <typename InputIt>
  explicit flat_set(InputIt first, InputIt last, Compare const& comp = Compare()) : comp_(comp) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <typename InputIt, typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  explicit flat_set(InputIt first, InputIt last, Compare const& comp,
                    typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  flat_set(flat_set const& other) = default;
  flat_set& operator=(flat_set const& other) = default;
  flat_set(flat_set&& other) noexcept = default;
  flat_set& operator=(flat_set&& other) noexcept = default;

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_set(flat_set const& other, typename Alias::allocator_type const& alloc)
      : comp_(other.comp_), rep_(other.rep_, alloc) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_set(flat_set&& other, typename Alias::allocator_type const& alloc)
      : comp_(std::move(other.comp_)), rep_(std::move(other.rep_), alloc) {}

  flat_set(std::initializer_list<value_type> const init, Compare const& comp = Compare())
      : comp_(comp) {
    for (auto& value : init) {
      insert(value);
    }
  }

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_set(std::initializer_list<value_type> const init, Compare const& comp,
           typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {
    for (auto& value : init) {
      insert(value);
    }
  }

  flat_set& operator=(std::initializer_list<value_type> const init) {
    rep_.clear();
    for (auto& value : init) {
      insert(value);
    }
    return *this;
  }

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  typename Alias::allocator_type get_allocator() const {
    return std::allocator_traits<typename Alias::allocator_type>::
        select_on_container_copy_construction(rep_.get_allocator());
  }

  // As specified by the standard, all comparison operators intentionally ignore the user-provided
  // comparator. For operators `==` and `!=` this allows for faster comparisons, as using a
  // less-than comparator would require comparing each pair twice (A!=B iff (A<B)||(B<A)). Other
  // operators ignore the user-provided comparator for consistency with `==` and `!=`.

  friend bool operator==(flat_set const& lhs, flat_set const& rhs) {
    return lhs.rep() == rhs.rep();
  }

  friend bool operator!=(flat_set const& lhs, flat_set const& rhs) {
    return lhs.rep() != rhs.rep();
  }

  friend bool operator<(flat_set const& lhs, flat_set const& rhs) { return lhs.rep() < rhs.rep(); }
  friend bool operator<=(flat_set const& lhs, flat_set const& rhs) { return !(rhs < lhs); }
  friend bool operator>(flat_set const& lhs, flat_set const& rhs) { return rhs < lhs; }
  friend bool operator>=(flat_set const& lhs, flat_set const& rhs) { return !(lhs < rhs); }

  iterator begin() noexcept { return rep_.begin(); }
  const_iterator begin() const noexcept { return rep_.begin(); }
  const_iterator cbegin() const noexcept { return rep_.cbegin(); }
  iterator end() noexcept { return rep_.end(); }
  const_iterator end() const noexcept { return rep_.end(); }
  const_iterator cend() const noexcept { return rep_.cend(); }

  reverse_iterator rbegin() noexcept { return rep_.rbegin(); }
  const_reverse_iterator rbegin() const noexcept { return rep_.rbegin(); }
  const_reverse_iterator crbegin() const noexcept { return rep_.crbegin(); }
  reverse_iterator rend() noexcept { return rep_.rend(); }
  const_reverse_iterator rend() const noexcept { return rep_.rend(); }
  const_reverse_iterator crend() const noexcept { return rep_.crend(); }

  [[nodiscard]] bool empty() const noexcept { return rep_.empty(); }
  size_type size() const noexcept { return rep_.size(); }
  size_type capacity() const noexcept { return rep_.capacity(); }
  size_type max_size() const noexcept { return rep_.max_size(); }

  template <typename H>
  friend H AbslHashValue(H h, flat_set const& fs) {
    return H::combine(std::move(h), fs.rep_);
  }

  template <typename H>
  friend H Tsdb2FingerprintValue(H h, flat_set const& fs) {
    return H::Combine(std::move(h), fs.rep_);
  }

  void clear() noexcept { rep_.clear(); }

  std::pair<iterator, bool> insert(value_type const& value) {
    auto it = std::lower_bound(rep_.begin(), rep_.end(), value, comp_);
    if (it == rep_.end() || comp_(value, *it)) {
      it = rep_.insert(it, value);
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    auto it = std::lower_bound(rep_.begin(), rep_.end(), value, comp_);
    if (it == rep_.end() || comp_(value, *it)) {
      it = rep_.insert(it, std::move(value));
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  iterator insert(const_iterator pos, value_type const& value) {
    auto [it, unused] = insert(value);
    return it;
  }

  iterator insert(const_iterator pos, value_type&& value) {
    auto [it, unused] = insert(std::move(value));
    return it;
  }

  template <typename InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const init) {
    for (auto& value : init) {
      insert(value);
    }
  }

  insert_return_type insert(node_type&& node) {
    if (node) {
      auto [it, inserted] = insert(std::move(node).value());
      if (inserted) {
        return {it, true};
      } else {
        return {it, false, std::move(node)};
      }
    } else {
      return {rep_.end(), false, std::move(node)};
    }
  }

  iterator insert(const_iterator pos, node_type&& node) {
    auto [it, unused_inserted, unused_node] = insert(std::move(node));
    return it;
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return insert(value_type(std::forward<Args>(args)...));
  }

  iterator erase(const_iterator pos) { return rep_.erase(pos); }

  iterator erase(const_iterator const first, const_iterator const last) {
    return rep_.erase(first, last);
  }

  template <typename KeyArg = key_type>
  size_type erase(key_arg_t<KeyArg> const& key) {
    auto const it = find(key);
    if (it != end()) {
      rep_.erase(it);
      return 1;
    } else {
      return 0;
    }
  }

  void swap(flat_set& other) noexcept {
    using std::swap;  // ensure ADL
    swap(comp_, other.comp_);
    swap(rep_, other.rep_);
  }

  friend void swap(flat_set& lhs, flat_set& rhs) noexcept { lhs.swap(rhs); }

  node_type extract(iterator position) {
    node_type node{std::move(*position)};
    rep_.erase(position);
    return node;
  }

  template <typename KeyArg = key_type>
  node_type extract(key_arg_t<KeyArg> const& key) {
    auto it = find(key);
    if (it != end()) {
      return extract(it);
    } else {
      return {};
    }
  }

  // TODO: merge methods.

  void reserve(size_type const count) { rep_.reserve(count); }

  Representation const& rep() const& { return rep_; }

  Representation&& ExtractRep() && { return std::move(rep_); }

  template <typename KeyArg = key_type>
  size_type count(key_arg_t<KeyArg> const& key) const {
    return std::binary_search(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  iterator find(key_arg_t<KeyArg> const& key) {
    auto const it = std::lower_bound(rep_.begin(), rep_.end(), key, comp_);
    if (it == rep_.end() || comp_(key, *it)) {
      return rep_.end();
    } else {
      return it;
    }
  }

  template <typename KeyArg = key_type>
  const_iterator find(key_arg_t<KeyArg> const& key) const {
    auto const it = std::lower_bound(rep_.begin(), rep_.end(), key, comp_);
    if (it == rep_.end() || comp_(key, *it)) {
      return rep_.end();
    } else {
      return it;
    }
  }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const& key) const {
    return std::binary_search(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  std::pair<iterator, iterator> equal_range(key_arg_t<KeyArg> const& key) {
    return std::equal_range(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  std::pair<const_iterator, const_iterator> equal_range(key_arg_t<KeyArg> const& key) const {
    return std::equal_range(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  iterator lower_bound(key_arg_t<KeyArg> const& key) {
    return std::lower_bound(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  const_iterator lower_bound(key_arg_t<KeyArg> const& key) const {
    return std::lower_bound(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  iterator upper_bound(key_arg_t<KeyArg> const& key) {
    return std::upper_bound(rep_.begin(), rep_.end(), key, comp_);
  }

  template <typename KeyArg = key_type>
  const_iterator upper_bound(key_arg_t<KeyArg> const& key) const {
    return std::upper_bound(rep_.begin(), rep_.end(), key, comp_);
  }

  key_compare key_comp() const { return comp_; }
  value_compare value_comp() const { return comp_; }

 private:
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Compare comp_;
  Representation rep_;
};

template <typename T, size_t N, typename Compare = std::less<T>>
using fixed_flat_set = flat_set<T, Compare, std::array<T, N>>;

template <typename T, typename Compare = std::less<std::decay_t<T>>, size_t N>
constexpr auto fixed_flat_set_of(std::array<T, N> array, Compare&& comp = Compare()) {
  internal::ConstexprSort(array, comp);
  internal::ConstexprCheckDuplications(array, comp);
  return fixed_flat_set<T, N, Compare>(kSortedDeduplicatedContainer, std::move(array),
                                       std::forward<Compare>(comp));
}

template <typename T, typename Compare = std::less<std::decay_t<T>>, size_t N>
constexpr auto fixed_flat_set_of(T const (&values)[N],  // NOLINT(*-avoid-c-arrays)
                                 Compare&& comp = Compare()) {
  return fixed_flat_set_of<T, Compare, N>(to_array(values), std::forward<Compare>(comp));
}

template <typename T, typename Compare = std::less<T>>
constexpr auto fixed_flat_set_of(internal::EmptyInitializerList, Compare&& comp = Compare()) {
  return fixed_flat_set<T, 0, Compare>(std::forward<Compare>(comp));
}

}  // namespace common
}  // namespace tsdb2

#endif  //__TSDB2_COMMON_FLAT_SET_H__
