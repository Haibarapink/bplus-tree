#pragma once

#include "../bplus_tree.hpp"
#include <cassert>

inline bool LeafNode::get(const key_type &key, value_type &val) {
  auto [exist, idx] = find(key);
  if (exist) {
    val = kvs_[idx].second;
  }
  return exist;
}

inline int LeafNode::find_idx(const key_type &key) {
  int l = -1, r = num_keys_;
  while (l + 1 != r) {
    int mid = (l + r) / 2;
    int cmp = key_cmp(key, kvs_[mid].first);
    if (cmp > 0) {
      l = mid;
    } else {
      r = mid;
    }
  }
  return r;
}

inline auto LeafNode::find(const key_type &key) -> std::pair<bool, int> {
  int l = -1, r = num_keys_;
  bool exist = false;
  while (l + 1 != r) {
    int mid = (l + r) / 2;
    int cmp = key_cmp(key, kvs_[mid].first);
    if (cmp > 0) {
      l = mid;
    } else {
      // r >= key
      r = mid;
    }
  }

  if (r < num_keys_ && r >= 0 && key_cmp(key, kvs_[r].first) == 0) {
    exist = true;
  }
  return {exist, r};
}

inline void LeafNode::insert(key_type key, value_type val) {
  auto idx = find_idx(key);

  Element item;
  item.key_size = key.size();
  item.val_size = val.size();
  if (idx == num_keys_) {
    items_.push_back(item);
    kvs_.push_back({std::move(key), std::move(val)});
  } else {
    items_.insert(items_.begin() + idx, item);
    kvs_.insert(kvs_.begin() + idx, {std::move(key), std::move(val)});
  }
  ++num_keys_;
}

inline void LeafNode::remove(const key_type &key) {
  auto idx = find_idx(key);
  if (idx == num_keys_ || kvs_[idx].first != key) {
    return;
  }
  remove(idx);
}

inline void LeafNode::remove(int idx) {
  items_.erase(items_.begin() + idx);
  kvs_.erase(kvs_.begin() + idx);
  --num_keys_;
}

inline void LeafNode::read(Page *p) {
  assert(p->page_type == kLeafPageType);
  this->p = p;
  char *data = p->get_data();
  std::memcpy(&num_keys_, data, sizeof(int));
  data += sizeof(int);
  std::memcpy(&parent_, data, sizeof(PageId));
  data += sizeof(PageId);
  std::memcpy(&next_, data, sizeof next_);
  data += sizeof next_;

  for (int i = 0; i < num_keys_; ++i) {
    Element item;
    std::memcpy(&item.key_size, data, sizeof(int));
    data += sizeof(int);
    std::memcpy(&item.val_size, data, sizeof(int));
    data += sizeof(int);
    items_.push_back(item);
  }
  for (int i = 0; i < num_keys_; ++i) {
    std::pair<bytes, bytes> kv;
    kv.first.resize(items_[i].key_size);
    kv.second.resize(items_[i].val_size);
    std::memcpy(kv.first.data(), data, items_[i].key_size);
    data += items_[i].key_size;
    std::memcpy(kv.second.data(), data, items_[i].val_size);
    data += items_[i].val_size;
    kvs_.push_back(std::move(kv));
  }
}

inline void LeafNode::write(Page *p) {
  assert(p->page_type == kLeafPageType);
  assert(less_than(PAGE_SIZE) && "page size overflow");

  this->p = p;
  char *data = p->get_data();
  std::memcpy(data, &num_keys_, sizeof(int));
  data += sizeof(int);
  std::memcpy(data, &parent_, sizeof(PageId));
  data += sizeof(PageId);
  std::memcpy(data, &next_, sizeof next_);
  data += sizeof next_;

  for (int i = 0; i < num_keys_; ++i) {
    std::memcpy(data, &items_[i].key_size, sizeof(int));
    data += sizeof(int);
    std::memcpy(data, &items_[i].val_size, sizeof(int));
    data += sizeof(int);
  }
  for (int i = 0; i < num_keys_; ++i) {
    std::memcpy(data, kvs_[i].first.data(), items_[i].key_size);
    data += items_[i].key_size;
    std::memcpy(data, kvs_[i].second.data(), items_[i].val_size);
    data += items_[i].val_size;
  }
}

inline bool LeafNode::less_than(size_t page_size) {
  size_t size = meta_size();
  for (int i = 0; i < num_keys_; ++i) {
    size += sizeof(int) * 2 + items_[i].key_size + items_[i].val_size;
  }
  return size < page_size;
}

inline void LeafNode::move_half_to(LeafNode &new_node) {
  int mid = num_keys_ / 2;
  new_node.num_keys_ = num_keys_ - mid;

  for (auto i = 0; i < new_node.num_keys_; ++i) {
    new_node.items_.push_back(items_[mid + i]);
    new_node.kvs_.push_back(std::move(kvs_[mid + i]));
  }

  items_.erase(items_.begin() + mid, items_.end());
  kvs_.erase(kvs_.begin() + mid, kvs_.end());
  num_keys_ = mid;
}