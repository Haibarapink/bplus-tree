#pragma once
#include "buffer_pool.hpp"
#include "logger.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <system_error>

using bytes = std::vector<char>;
using key_type = bytes;
using value_type = bytes;

// @brief get key at index
inline int key_cmp(const key_type &left, const key_type &right) {
  const char *l_d = left.data(), *r_d = right.data();
  int res = strncmp(l_d, r_d, std::min(left.size(), right.size()));
  if (res == 0) {
    if (left.size() < right.size()) {
      res = -1;
    } else if (left.size() > right.size()) {
      res = 1;
    }
  }
  return res;
}

// If modfied this class, please modify insert and remove in BPlusTree
// setting children's parent
class InternalNode {
public:
  friend class BPlusTree;

  bool operator==(const InternalNode &other) const {
    return items_ == other.items_ && keys_ == other.keys_ &&
           parent_ == other.parent_;
  }

  struct Element {
    int key_size;
    // int key_pos;
    PageId child; // pointer to right child

    bool operator==(const Element &other) const {
      return key_size == other.key_size && child == other.child;
    }
  };

  bool contains(const key_type &key) {
    auto [exist, idx] = find(key);
    return exist;
  }

  int find_idx(const key_type &key);
  auto find(const key_type &key) -> std::pair<bool, int>;
  PageId child(const key_type &key);
  void insert(key_type key, PageId child);
  void remove(const key_type &key);
  void remove(int idx);
  size_t size() const { return items_.size(); }
  // read from page
  void read(Page *p);
  // write to page
  void write(Page *p) const;
  void move_half_to(InternalNode &internal);

  const Element &item(size_t idx) { return items_[idx]; }
  const key_type &key(size_t idx) { return keys_[idx]; }
  bool less_than(size_t page_size) const {
    size_t size = sizeof(Element) * items_.size() + meta_size();
    for (auto i = 0; i < items_.size(); ++i) {
      size += items_[i].key_size;
    }
    return size < page_size;
  }

  // // caller is right node, and new_node is left node
  // const key_type &split(InternalNode &new_node);

  void set_parent(PageId parent) { parent_ = parent; }
  PageId parent() const { return parent_; }

private:
  size_t meta_size() const {
    return Page::offset() + sizeof num_keys_ + sizeof parent_;
  }

  Page *p = nullptr;
  int num_keys_ = 0;
  PageId parent_ = INVALID_PAGE_ID;
  std::vector<Element> items_;
  std::vector<key_type> keys_;
};

class LeafNode {
  using kv_type = std::pair<bytes, bytes>;

public:
  bool operator==(const LeafNode &other) const {
    return items_ == other.items_ && kvs_ == other.kvs_ &&
           parent_ == other.parent_;
  }

  struct Element {
    int key_size;
    int val_size;

    bool operator==(const Element &other) const {
      return key_size == other.key_size && val_size == other.val_size;
    }
  };

  bool get(const key_type &key, value_type &val);

  value_type fetch(int idx) {
    assert(idx < items_.size());
    return kvs_[idx].second;
  }

  void set_parent(PageId parent) { parent_ = parent; }
  PageId parent() const { return parent_; }

  int find_idx(const key_type &key);
  auto find(const key_type &key) -> std::pair<bool, int>;
  void insert(key_type key, value_type val);
  void remove(const key_type &key);
  void remove(int idx);
  void read(Page *p);
  void write(Page *p);
  bool less_than(size_t page_size);

  void move_half_to(LeafNode &new_node);

  size_t size() const { return items_.size(); }
  key_type key(int idx) { return kvs_[idx].first; }

private:
  size_t meta_size() const {
    return Page::offset() + sizeof num_keys_ + sizeof parent_;
  }

private:
  Page *p = nullptr;
  int num_keys_ = 0;
  PageId parent_ = INVALID_PAGE_ID;
  std::vector<Element> items_;
  std::vector<std::pair<bytes, bytes>> kvs_;
};

class BPlusTree {
public:
  friend class BPlusTreeTest;

  BPlusTree(std::string_view db_name, size_t pool_size = 32)
      : buffer_pool_(db_name, pool_size) {
    buffer_pool_.open();
  }

  ~BPlusTree() {
    buffer_pool_.flush_all();
    buffer_pool_.close();
  }

  template <typename K, typename V> bool insert(const K &key, const V &val) {
    return insert(bytes(key.begin(), key.end()), bytes(val.begin(), val.end()));
  }

  template <typename K, typename V> bool search(const K &key, V &val) {
    bytes v;
    bool res = search(bytes(key.begin(), key.end()), v);
    val = V(v.begin(), v.end());
    return res;
  }

  bool insert(key_type key, value_type val);
  bool search(const key_type &key, value_type &val);

private:
  Page *find_leaf(const key_type &key);
  bool insert_parent(PageId parent, PageId left, PageId right, key_type key);
  bool make_tree(key_type k, value_type v);
  bool make_root(key_type k, PageId left, PageId right);

  bool set_parent(PageId p, PageId parent) {
    auto page = buffer_pool_.fetch(p);
    if (!page) {
      return false;
    }
    if (page->page_type == kLeafPageType) {
      auto leaf = LeafNode();
      leaf.read(page);
      leaf.set_parent(parent);
      leaf.write(page);
    } else {
      auto internal = InternalNode();
      internal.read(page);
      internal.set_parent(parent);
      internal.write(page);
    }
    buffer_pool_.unpin(p, true);
    return true;
  }

private:
  PageId root_ = INVALID_PAGE_ID;
  BufferPool buffer_pool_;
};

#include "impl/internal_impl.ipp"
#include "impl/leaf_impl.ipp"
#include "impl/tree_impl.ipp"
#include "impl/tree_insert_impl.ipp"
#include "impl/tree_search_impl.ipp"