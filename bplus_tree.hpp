#pragma once
#include "buffer_pool.hpp"
#include "logger.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

using bytes = std::vector<char>;
using key_type = bytes;
using value_type = bytes;

class InternalNode {
public:
  struct Element {
    int key_size;
    // int key_pos;
    PageId child; // pointer to right child
  };

  int insert_idx(const key_type &key) {
    int l = 0, r = num_keys - 1;
    while (l <= r) {
      int mid = (l + r) / 2;
      if (key < keys_[mid]) {
        r = mid - 1;
      } else {
        l = mid + 1;
      }
    }
    return l;
  }

  void insert(key_type key, PageId child) {
    int idx = insert_idx(key);
    Element item;
    item.key_size = key.size();
    // item.key_pos = 0; // TODO
    item.child = child;
    items_.insert(items_.begin() + idx, item);
    keys_.insert(keys_.begin() + idx, std::move(key));
    ++num_keys;
  }

  void remove(const key_type &key) {
    int idx = insert_idx(key);
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

  int insert_idx(const key_type &key) {
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

  void insert(kv_type kv) {
    int idx = insert_idx(kv.first);
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
    int idx = insert_idx(k);
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

class BPlusTree {};