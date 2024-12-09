// This header provides `flat_map`, a drop-in replacement for std::map backed by a specified data
// structure. When backed by an std::vector, flat_map behaves like a sorted array and is well suited
// for read-mostly use cases and/or small-ish data structures. In those cases, being allocated in a
// single heap block makes the data much more cache-friendly and efficient. For any uses cases with
// large (but still read-mostly) datasets, you may want to use std::deque instead of std::vector.
//
// NOTE: `flat_map` is not fully compliant with `std::map` because the elements of the underlying
// container must be mutable pairs (otherwise reallocations wouldn't be possible) but the API must
// disallow changing the keys, so all `flat_map` iterators are always constant. Modifications to the
// mapped values are still possible by other means, e.g. the subscript operator, `insert_or_assign`,
// etc.
//
// NOTE: C++23 has `std::flat_map` but we don't necessarily want to replace our own implementation
// with that because `std::flat_map` uses two separate underlying containers, one for the keys and
// one for the values, resulting in lower cache friendliness.

#ifndef __TSDB2_COMMON_FLAT_MAP_H__
#define __TSDB2_COMMON_FLAT_MAP_H__

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/log.h"
#include "common/flat_container_internal.h"
#include "common/to_array.h"

namespace tsdb2 {
namespace common {

template <typename Key, typename T, typename Compare = internal::DefaultCompareT<Key>,
          typename Representation = std::vector<std::pair<Key, T>>>
class flat_map {
 public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<Key, T>;
  using representation_type = Representation;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using key_compare = Compare;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename Representation::pointer;
  using const_pointer = typename Representation::const_pointer;
  using iterator = typename Representation::iterator;
  using const_iterator = typename Representation::const_iterator;
  using reverse_iterator = typename Representation::reverse_iterator;
  using const_reverse_iterator = typename Representation::const_reverse_iterator;

  class node {
   public:
    using key_type = Key;
    using mapped_type = T;

    constexpr node() noexcept : value_(std::nullopt) {}
    explicit node(value_type&& value) : value_(std::move(value)) {}

    node(node&&) noexcept = default;
    node& operator=(node&&) noexcept = default;

    [[nodiscard]] bool empty() const noexcept { return !value_.has_value(); }
    explicit operator bool() const noexcept { return value_.has_value(); }

    key_type& key() const { return const_cast<key_type&>(value_->first); }
    mapped_type& mapped() const { return const_cast<mapped_type&>(value_->second); }

    value_type& value() const& { return const_cast<value_type&>(*value_); }
    value_type&& value() && { return *std::move(value_); }

    void swap(node& other) noexcept { value_.swap(other.value_); }
    friend void swap(node& lhs, node& rhs) noexcept { lhs.swap(rhs); }

   private:
    node(node const&) = delete;
    node& operator=(node const&) = delete;

    std::optional<value_type> value_;
  };

  using node_type = node;

  struct insert_return_type {
    iterator position;
    bool inserted;
    node_type node;
  };

  class value_compare {
   public:
    explicit constexpr value_compare(Compare const& comp) : comp_(comp) {}

    constexpr bool operator()(value_type const& lhs, value_type const& rhs) const {
      return comp_(lhs.first, rhs.first);
    }

   private:
    Compare const& comp_;
  };

  template <typename Arg>
  using key_arg_t =
      typename internal::flat_container::key_arg<Compare>::template type<key_type, Arg>;

  class generic_compare {
   public:
    explicit constexpr generic_compare(Compare const& comp) : comp_(comp) {}

    template <typename LHS, typename RHS>
    constexpr bool operator()(LHS&& lhs, RHS&& rhs) const {
      return comp_(to_key(std::forward<LHS>(lhs)), to_key(std::forward<RHS>(rhs)));
    }

   private:
    static constexpr key_type const& to_key(value_type const& value) { return value.first; }

    template <typename KeyArg = key_type>
    static constexpr auto to_key(key_arg_t<KeyArg> const& key) {
      return key;
    }

    Compare const& comp_;
  };

  constexpr flat_map() : flat_map(Compare()) {}

  explicit constexpr flat_map(SortedDeduplicatedContainer, Representation rep,
                              Compare const& comp = Compare())
      : comp_(comp), rep_(std::move(rep)) {}

  explicit constexpr flat_map(Compare const& comp) : comp_(comp) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  explicit flat_map(Compare const& comp, typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  explicit flat_map(typename Alias::allocator_type const& alloc) : rep_(alloc) {}

  template <class InputIt>
  flat_map(InputIt first, InputIt last, Compare const& comp = Compare()) : comp_(comp) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  template <class InputIt, typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_map(InputIt first, InputIt last, Compare const& comp,
           typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  flat_map(flat_map const& other) = default;
  flat_map& operator=(flat_map const& other) = default;
  flat_map(flat_map&& other) noexcept = default;
  flat_map& operator=(flat_map&& other) noexcept = default;

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_map(flat_map const& other, typename Alias::allocator_type const& alloc)
      : comp_(other.comp_), rep_(other.rep_, alloc) {}

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_map(flat_map&& other, typename Alias::allocator_type const& alloc)
      : comp_(std::move(other.comp_)), rep_(std::move(other.rep_), alloc) {}

  flat_map(std::initializer_list<value_type> const init, Compare const& comp = Compare())
      : comp_(comp) {
    for (auto& [key, value] : init) {
      try_emplace(key, value);
    }
  }

  template <typename Alias = Representation,
            std::enable_if_t<internal::HasAllocatorV<Alias>, bool> = true>
  flat_map(std::initializer_list<value_type> const init, Compare const& comp,
           typename Alias::allocator_type const& alloc)
      : comp_(comp), rep_(alloc) {
    for (auto& [key, value] : init) {
      try_emplace(key, value);
    }
  }

  flat_map& operator=(std::initializer_list<value_type> const init) {
    rep_.clear();
    for (auto& [key, value] : init) {
      try_emplace(key, value);
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

  friend bool operator==(flat_map const& lhs, flat_map const& rhs) {
    return lhs.rep() == rhs.rep();
  }

  friend bool operator!=(flat_map const& lhs, flat_map const& rhs) {
    return lhs.rep() != rhs.rep();
  }

  friend bool operator<(flat_map const& lhs, flat_map const& rhs) { return lhs.rep() < rhs.rep(); }
  friend bool operator<=(flat_map const& lhs, flat_map const& rhs) { return !(rhs < lhs); }
  friend bool operator>(flat_map const& lhs, flat_map const& rhs) { return rhs < lhs; }
  friend bool operator>=(flat_map const& lhs, flat_map const& rhs) { return !(lhs < rhs); }

  T& at(Key const& key) {
    auto const it = find(key);
    if (it != end()) {
      return it->second;
    } else {
      LOG(FATAL) << "tsdb2::common::flat_map::at(): key not found";
      std::abort();
    }
  }

  T const& at(Key const& key) const {
    auto const it = find(key);
    if (it != end()) {
      return it->second;
    } else {
      LOG(FATAL) << "tsdb2::common::flat_map::at(): key not found";
      std::abort();
    }
  }

  T& operator[](Key const& key) {
    auto const [it, unused] = try_emplace(key);
    return it->second;
  }

  T& operator[](Key&& key) {
    auto const [it, unused] = try_emplace(std::move(key));
    return it->second;
  }

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
  size_t size() const noexcept { return rep_.size(); }
  size_t capacity() const noexcept { return rep_.capacity(); }
  size_t max_size() const noexcept { return rep_.max_size(); }

  template <typename H>
  friend H AbslHashValue(H h, flat_map const& fm) {
    return H::combine(std::move(h), fm.rep_);
  }

  template <typename H>
  friend H Tsdb2FingerprintValue(H h, flat_map const& fm) {
    return H::Combine(std::move(h), fm.rep_);
  }

  void clear() noexcept { rep_.clear(); }

  std::pair<iterator, bool> insert(value_type const& value) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), value, comp);
    if (it == rep_.end() || comp(value, *it)) {
      it = rep_.insert(it, value);
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  template <typename P>
  std::pair<iterator, bool> insert(P&& value) {
    return emplace(std::forward<P>(value));
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), value, comp);
    if (it == rep_.end() || comp(value, *it)) {
      it = rep_.insert(it, std::move(value));
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  iterator insert(const_iterator const pos, value_type const& value) {
    auto [it, unused] = insert(value);
    return it;
  }

  template <class P>
  iterator insert(const_iterator const pos, P&& value) {
    auto [it, unused] = insert(std::forward<P>(value));
    return it;
  }

  iterator insert(const_iterator const pos, value_type&& value) {
    auto [it, unused] = insert(std::move(value));
    return it;
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const init) {
    for (auto& [key, value] : init) {
      try_emplace(key, value);
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

  template <typename Mapped>
  std::pair<iterator, bool> insert_or_assign(Key const& key, Mapped&& mapped) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      return insert(value_type(key, std::forward<Mapped>(mapped)));
    } else {
      it->second = std::move(mapped);
      return {it, false};
    }
  }

  template <typename Mapped>
  std::pair<iterator, bool> insert_or_assign(Key&& key, Mapped&& mapped) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      return insert(value_type(std::move(key), std::forward<Mapped>(mapped)));
    } else {
      it->second = std::move(mapped);
      return {it, false};
    }
  }

  template <typename Mapped>
  iterator insert_or_assign(const_iterator const hint, Key const& key, Mapped&& mapped) {
    return insert_or_assign(key, std::forward<Mapped>(mapped));
  }

  template <typename Mapped>
  iterator insert_or_assign(const_iterator const hint, Key&& key, Mapped&& mapped) {
    return insert_or_assign(std::move(key), std::forward<Mapped>(mapped));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return insert(value_type(std::forward<Args>(args)...));
  }

  template <typename... Args>
  iterator emplace_hint(const_iterator const hint, Args&&... args) {
    auto [it, unused] = emplace(std::forward<Args>(args)...);
    return it;
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(Key const& key, Args&&... args) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      it = rep_.emplace(it, std::piecewise_construct, std::forward_as_tuple(key),
                        std::forward_as_tuple(std::forward<Args>(args)...));
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args) {
    generic_compare comp{comp_};
    auto it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      it = rep_.emplace(it, std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                        std::forward_as_tuple(std::forward<Args>(args)...));
      return std::make_pair(it, true);
    } else {
      return std::make_pair(it, false);
    }
  }

  template <typename... Args>
  iterator try_emplace(const_iterator const hint, Key const& key, Args&&... args) {
    auto [it, unused] = try_emplace(key, std::forward<Args>(args)...);
    return it;
  }

  template <typename... Args>
  iterator try_emplace(const_iterator const hint, Key&& key, Args&&... args) {
    auto [it, unused] = try_emplace(std::move(key), std::forward<Args>(args)...);
    return it;
  }

  iterator erase(iterator const pos) { return rep_.erase(pos); }
  iterator erase(const_iterator const pos) { return rep_.erase(pos); }

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

  void swap(flat_map& other) noexcept {
    using std::swap;  // ensure ADL
    swap(comp_, other.comp_);
    swap(rep_, other.rep_);
  }

  friend void swap(flat_map& lhs, flat_map& rhs) noexcept { lhs.swap(rhs); }

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
  size_t count(key_arg_t<KeyArg> const& key) const {
    return std::binary_search(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  iterator find(key_arg_t<KeyArg> const& key) {
    generic_compare comp{comp_};
    auto const it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      return rep_.end();
    } else {
      return it;
    }
  }

  template <typename KeyArg = key_type>
  const_iterator find(key_arg_t<KeyArg> const& key) const {
    generic_compare comp{comp_};
    auto const it = std::lower_bound(rep_.begin(), rep_.end(), key, comp);
    if (it == rep_.end() || comp(key, it->first)) {
      return rep_.end();
    } else {
      return it;
    }
  }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const& key) const {
    return std::binary_search(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  std::pair<iterator, iterator> equal_range(key_arg_t<KeyArg> const& key) {
    return std::equal_range(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  std::pair<const_iterator, const_iterator> equal_range(key_arg_t<KeyArg> const& key) const {
    return std::equal_range(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  iterator lower_bound(key_arg_t<KeyArg> const& key) {
    return std::lower_bound(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  const_iterator lower_bound(key_arg_t<KeyArg> const& key) const {
    return std::lower_bound(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  iterator upper_bound(key_arg_t<KeyArg> const& key) {
    return std::upper_bound(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  template <typename KeyArg = key_type>
  const_iterator upper_bound(key_arg_t<KeyArg> const& key) const {
    return std::upper_bound(rep_.begin(), rep_.end(), key, generic_compare(comp_));
  }

  key_compare key_comp() const { return comp_; }
  value_compare value_comp() const { return value_compare(comp_); }

 private:
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Compare comp_;
  Representation rep_;
};

template <typename Key, typename T, size_t N, typename Compare = std::less<Key>>
using fixed_flat_map = flat_map<Key, T, Compare, std::array<std::pair<Key, T>, N>>;

template <typename Key, typename T, typename Compare = std::less<Key>, size_t N>
constexpr auto fixed_flat_map_of(std::array<std::pair<Key, T>, N> array,
                                 Compare&& comp = Compare()) {
  typename fixed_flat_map<Key, T, N, Compare>::generic_compare gen_comp{comp};
  internal::ConstexprSort(array, gen_comp);
  internal::ConstexprCheckDuplications(array, gen_comp);
  return fixed_flat_map<Key, T, N, Compare>(kSortedDeduplicatedContainer, std::move(array),
                                            std::forward<Compare>(comp));
}

template <typename Key, typename T, typename Compare = std::less<Key>, size_t N>
constexpr auto fixed_flat_map_of(std::pair<Key, T> const (&values)[N],  // NOLINT(*-avoid-c-arrays)
                                 Compare&& comp = Compare()) {
  return fixed_flat_map_of<Key, T, Compare, N>(to_array(values), std::forward<Compare>(comp));
}

template <typename Key, typename T, typename Compare = std::less<Key>>
constexpr auto fixed_flat_map_of(internal::EmptyInitializerList, Compare&& comp = Compare()) {
  return fixed_flat_map<Key, T, 0, Compare>(std::forward<Compare>(comp));
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FLAT_MAP_H__
