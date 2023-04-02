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
    return p->data_offset() + sizeof num_keys_ + sizeof parent_;
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

  int find_idx(const key_type &key);
  auto find(const key_type &key) -> std::pair<bool, int>;
  void insert(key_type key, value_type val);
  void remove(const key_type &key);
  void remove(int idx);
  void read(Page *p);
  void write(Page *p);
  bool less_than(size_t page_size);

  void move_half_to(LeafNode &new_node);

private:
  size_t meta_size() const {
    return p->data_offset() + sizeof num_keys_ + sizeof parent_;
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
  bool insert(key_type key, value_type val);

private:
  Page *find_leaf(const key_type &key);

private:
  PageId root_ = INVALID_PAGE_ID;
};

#include "impl/internal_impl.ipp"
#include "impl/leaf_impl.ipp"