#pragma once
#include "buffer_pool.hpp"
#include "logger.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <sys/types.h>
#include <system_error>

using bytes = std::vector<char>;
using key_type = bytes;
using value_type = bytes;

// @brief get key at index
inline int key_cmp(const key_type &left, const key_type &right) {
  if (left == right) {
    return 0;
  }
  if (left < right) {
    return -1;
  }
  return 1;
}

class InternalNode {
public:
  struct Element {
    int key_size;
    // int key_pos;
    PageId child; // pointer to right child
  };

  bool contains(const key_type &key) {
    auto [exist, idx] = search(key);
    return exist;
  }

  int search_idx(const key_type &key) {
    int l = 0, r = num_keys - 1;
    while (l <= r) {
      int mid = (l + r) / 2;
      if (key < keys_[mid]) {
        r = mid - 1;
      } else {
        l = mid + 1;
      }
    }
    // key(l) <= key
    return l;
  }

  auto search(const key_type &key) -> std::pair<bool, int> {
    bool exist = false;
    int l = 0, r = num_keys - 1;

    while (l <= r) {
      int mid = (l + r) / 2;
      if (key < keys_[mid]) {
        r = mid - 1;
      } else if (key > keys_[mid]) {
        l = mid + 1;
      } else {
        exist = true;
        l = mid;
        break;
      }
    }

    return {exist, l};
  }

  PageId child(const key_type &key) {
    int idx = search_idx(key);
    if (idx == num_keys) {
      return INVALID_PAGE_ID;
    }

    return items_[idx].child;
  }

  void insert(key_type key, PageId child) {
    int idx = search_idx(key);
    Element item;
    item.key_size = key.size();
    // item.key_pos = 0; // TODO
    item.child = child;
    items_.insert(items_.begin() + idx, item);
    keys_.insert(keys_.begin() + idx, std::move(key));
    ++num_keys;
  }

  void remove(const key_type &key) {
    int idx = search_idx(key);
    if (idx == num_keys || keys_[idx] != key) {
      return;
    }
    remove(idx);
  }

  void remove(int idx) {
    assert(idx < num_keys && idx >= 0);
    items_.erase(items_.begin() + idx);
    keys_.erase(keys_.begin() + idx);
    --num_keys;
  }

  size_t size() const { return items_.size(); }

  // read from page
  void read(Page *p) {
    assert(p->page_type == kInternalPageType);
    // read key nums
    char *data = p->get_data();
    std::memcpy(&num_keys, data, sizeof(int));
    data += sizeof(int);

    // read items
    for (int i = 0; i < num_keys; ++i) {
      Element item;
      std::memcpy(&item.key_size, data, sizeof(int));
      data += sizeof(int);
      // std::memcpy(&item.key_pos, data, sizeof(int));
      // data += sizeof(int);
      std::memcpy(&item.child, data, sizeof(PageId));
      data += sizeof(PageId);
      items_.push_back(item);
    }

    // read keys
    for (int i = 0; i < num_keys; ++i) {
      key_type key;
      key.resize(items_[i].key_size);
      std::memcpy(key.data(), data, items_[i].key_size);
      data += items_[i].key_size;
      keys_.push_back(std::move(key));
    }
  }

  // write to page
  void write(Page *p) {
    char *data = p->get_data();
    std::memcpy(data, &num_keys, sizeof(int));
    data += sizeof(int);
    for (int i = 0; i < num_keys; ++i) {
      std::memcpy(data, &items_[i].key_size, sizeof(int));
      data += sizeof(int);
      // std::memcpy(data, &items_[i].key_pos, sizeof(int));
      //  data += sizeof(int);
      std::memcpy(data, &items_[i].child, sizeof(PageId));
      data += sizeof(PageId);
    }
    for (int i = 0; i < num_keys; ++i) {
      std::memcpy(data, keys_[i].data(), keys_[i].size());
      data += keys_[i].size();
    }
  }

  const Element &item(size_t idx) { return items_[idx]; }

  const key_type &key(size_t idx) { return keys_[idx]; }

  bool less_than(size_t page_size) {
    size_t size = sizeof(Element) * items_.size() + meta_size();
    for (auto i = 0; i < items_.size(); ++i) {
      size += items_[i].key_size;
    }
    return size < page_size;
  }

private:
  size_t meta_size() const { return p->data_offset() + sizeof(int); }

  Page *p;
  int num_keys;
  std::vector<Element> items_;
  std::vector<key_type> keys_;
};

class LeafNode {
public:
  using kv_type = std::pair<key_type, value_type>;

  struct Element {
    int key_size;
    int value_size;
    // int key_pos;
    // int value_pos;
  };

  bool contain(const key_type &key) {
    auto [exist, idx] = search(key);
    return exist;
  }

  int search_idx(const key_type &key) {
    int l = 0, r = num_kv_ - 1;
    while (l <= r) {
      int mid = (l + r) / 2;
      if (key < kvs_[mid].first) {
        r = mid - 1;
      } else {
        l = mid + 1;
      }
    }
    return l;
  }

  auto search(const key_type &key) -> std::pair<bool, int> {
    int idx = search_idx(key);
    if (idx < num_kv_ && kvs_[idx].first == key) {
      return std::make_pair(true, idx);
    }
    return std::make_pair(false, idx);
  }

  void insert(key_type k, value_type v) {
    insert(std::make_pair(std::move(k), std::move(v)));
  }

  void insert(kv_type kv) {
    int idx = search_idx(kv.first);
    Element item;
    item.key_size = kv.first.size();
    item.value_size = kv.second.size();
    // item.key_pos = 0; // TODO
    // item.value_pos = 0; // TODO
    items_.insert(items_.begin() + idx, item);
    kvs_.insert(kvs_.begin() + idx, std::move(kv));
    ++num_kv_;
  }

  void remove(const key_type &k) {
    int idx = search_idx(k);
    if (idx < num_kv_ && kvs_[idx].first == k) {
      items_.erase(items_.begin() + idx);
      kvs_.erase(kvs_.begin() + idx);
      --num_kv_;
    }
  }

  void remove(int idx) {
    assert(idx < num_kv_ && idx >= 0);
    items_.erase(items_.begin() + idx);
    kvs_.erase(kvs_.begin() + idx);
    --num_kv_;
  }

  const kv_type &get(int idx) { return kvs_[idx]; }
  size_t size() const { return kvs_.size(); }

  void read(Page *p) {
    assert(p->page_type == kLeafPageType);
    // read key nums
    char *data = p->get_data();
    std::memcpy(&num_kv_, data, sizeof(int));
    data += sizeof(int);

    // read items
    for (int i = 0; i < num_kv_; ++i) {
      Element item;
      std::memcpy(&item.key_size, data, sizeof(int));
      data += sizeof(int);
      std::memcpy(&item.value_size, data, sizeof(int));
      data += sizeof(int);
      // std::memcpy(&item.key_pos, data, sizeof(int));
      // data += sizeof(int);
      // std::memcpy(&item.value_pos, data, sizeof(int));
      // data += sizeof(int);
      items_.push_back(item);
    }

    // read kv
    for (int i = 0; i < num_kv_; ++i) {
      kv_type kv;
      kv.first.resize(items_[i].key_size);
      std::memcpy(kv.first.data(), data, items_[i].key_size);
      data += items_[i].key_size;
      kv.second.resize(items_[i].value_size);
      std::memcpy(kv.second.data(), data, items_[i].value_size);
      data += items_[i].value_size;
      kvs_.push_back(std::move(kv));
    }
  }

  void write(Page *p) {
    char *data = p->get_data();
    std::memcpy(data, &num_kv_, sizeof(int));
    data += sizeof(int);
    for (int i = 0; i < num_kv_; ++i) {
      std::memcpy(data, &items_[i].key_size, sizeof(int));
      data += sizeof(int);
      std::memcpy(data, &items_[i].value_size, sizeof(int));
      data += sizeof(int);
      // std::memcpy(data, &items_[i].key_pos, sizeof(int));
      // data += sizeof(int);
      // std::memcpy(data, &items_[i].value_pos, sizeof(int));
      // data += sizeof(int);
    }
    for (int i = 0; i < num_kv_; ++i) {
      std::memcpy(data, kvs_[i].first.data(), kvs_[i].first.size());
      data += kvs_[i].first.size();
      std::memcpy(data, kvs_[i].second.data(), kvs_[i].second.size());
      data += kvs_[i].second.size();
    }
  }

private:
  int num_kv_;
  std::vector<Element> items_;
  std::vector<kv_type> kvs_;
};

class BPlusTree {
public:
  friend class BPlusTreeTest;

  BPlusTree(std::string_view db, size_t defalt_pool_size = 32)
      : bfp_(new BufferPool(db, defalt_pool_size)) {}

  std::error_code open() {
    init();
    return std::error_code();
  }

  // If key is exist, the value will be rewrited.
  std::error_code insert(key_type k, value_type v) {
    if (root_ == INVALID_PAGE_ID) {
      return build_new_tree(std::move(k), std::move(v));
    }

    return insert_leaf(std::move(k), std::move(v));
  }

  // TODO If I can't fetch the meta page, allocate a new page, and write into db
  // file.
  // If close fail, return std::errc::not_enough_memory
  std::error_code close() {
    auto meta_page = bfp_->fetch(1);
    if (!meta_page) {
      return std::make_error_code(std::errc::not_enough_memory);
    }
    meta_.root = root_;
    meta_.write(meta_page);
    bfp_->close();
    return std::error_code();
  }

private:
  struct BPlusTreeMeta {
    // write on the first page
    PageId root;
    size_t leaf_size;
    size_t internal_size;

    void update(PageId id, size_t leaf_size, size_t internal_size) {
      this->leaf_size = leaf_size;
      this->internal_size = internal_size;
    }

    void read(Page *p) {
      assert(p);
      char *data = p->get_data();
      std::memcpy(&root, data, sizeof(PageId));
      data += sizeof(PageId);
      std::memcpy(&leaf_size, data, sizeof(size_t));
      data += sizeof(size_t);
      std::memcpy(&internal_size, data, sizeof(size_t));
    }

    void write(Page *p) {
      assert(p);
      char *data = p->get_data();
      std::memcpy(data, &root, sizeof(PageId));
      data += sizeof(PageId);
      std::memcpy(data, &leaf_size, sizeof(size_t));
      data += sizeof(size_t);
      std::memcpy(data, &internal_size, sizeof(size_t));
    }
  };

private:
  std::error_code init() {
    // read meta
    assert(bfp_);
    Page *meta_page = bfp_->fetch(1);

    if (meta_page == nullptr) {
      return std::make_error_code(std::errc::io_error);
    }

    meta_.read(meta_page);
    bfp_->unpin(meta_page->id);
    root_ = meta_.root;
    return std::error_code();
  }

private:
  // tree op
  std::error_code build_new_tree(key_type k, value_type v) {
    auto leaf_page = bfp_->new_page();
    if (!leaf_page) {
      return std::make_error_code(std::errc::not_enough_memory);
    }

    LeafNode leaf;
    leaf.insert(std::move(k), std::move(v));
    leaf.write(leaf_page);
    bfp_->unpin(leaf_page->id, true);

    root_ = leaf_page->id;
    return std::error_code();
  }

  std::error_code insert_leaf(key_type k, value_type v) {
    Page *leaf_page = find_leaf(k);
    if (!leaf_page) {
      return std::make_error_code(std::errc::io_error);
    }

    LeafNode leaf;
    leaf.read(leaf_page);

    // TODO
  }

  Page *find_leaf(const key_type &key) {
    PageId pid = root_;
    while (pid != INVALID_PAGE_ID) {
      Page *p = bfp_->fetch(pid);
      if (p->page_type == kLeafPageType) {
        return p;
      } else {
        InternalNode node;
        node.read(p);
        bfp_->unpin(pid);

        auto [exist, idx] = node.search(key);
        if (!exist && idx > 0) {
          --idx;
        }
        pid = node.item(idx).child;
      }
    }
    return nullptr;
  }

  std::unique_ptr<BufferPool> bfp_ = nullptr;
  BPlusTreeMeta meta_;
  PageId root_ = INVALID_PAGE_ID;
};