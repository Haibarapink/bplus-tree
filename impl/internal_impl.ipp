#pragma once
#include "../bplus_tree.hpp"
#include <cassert>

// find the first key that is greater than or equal to the argument key
inline int InternalNode::find_idx(const key_type &key) {
  int l = -1, r = num_keys_;
  while (l + 1 != r) {
    int mid = (l + r) / 2;
    int cmp = key_cmp(key, keys_[mid]);
    if (cmp > 0) {
      l = mid;
    } else {
      r = mid;
    }
  }
  return r;
}

// find the first key that is greater than or equal to the argument key
inline auto InternalNode::find(const key_type &key) -> std::pair<bool, int> {
  bool exist = false;
  int l = -1, r = num_keys_;

  while (l + 1 != r) {
    int mid = (l + r) / 2;
    int cmp = key_cmp(key, keys_[mid]);
    if (cmp > 0) {
      l = mid;
    } else {
      r = mid;
    }
  }
  if (r < num_keys_ && r >= 0 && keys_[r] == key) {
    exist = true;
  }
  return {exist, r};
}

inline PageId InternalNode::child(const key_type &key) {
  assert(num_keys_);
  // return greater than or equal to key
  int idx = find_idx(key);
  // key is greater than all keys, maybe it is at the end,
  // then if key is not equal to keys_[idx], idx--, because the key of
  // keys_[idx] is greater than the argument key
  if (idx == num_keys_ || key_cmp(key, keys_[idx]) != 0)
    idx--;
  return items_[idx].child;
}

inline void InternalNode::insert(key_type key, PageId child) {
  auto idx = find_idx(key);

  Element item;
  item.key_size = key.size();
  // item.key_pos = 0; // TODO
  item.child = child;
  if (idx == num_keys_) {
    items_.push_back(item);
    keys_.push_back(std::move(key));
  } else {
    items_.insert(items_.begin() + idx, item);
    keys_.insert(keys_.begin() + idx, std::move(key));
  }
  ++num_keys_;
}

inline void InternalNode::remove(const key_type &key) {
  int idx = find_idx(key);
  if (idx == num_keys_ || keys_[idx] != key) {
    return;
  }
  remove(idx);
}

inline void InternalNode::remove(int idx) {
  assert(idx < num_keys_ && idx >= 0);
  items_.erase(items_.begin() + idx);
  keys_.erase(keys_.begin() + idx);
  --num_keys_;
}

inline void InternalNode::read(Page *p) {
  if (p->page_type != kInternalPageType) {
    LOG_DEBUG << "page type : " << p->page_type << " id : " << p->id;
    assert(p->page_type == kInternalPageType);
  }

  this->p = p;
  // read key nums
  char *data = p->get_data();
  std::memcpy(&num_keys_, data, sizeof(int));
  data += sizeof(int);
  // read parent
  std::memcpy(&parent_, data, sizeof(PageId));
  data += sizeof(PageId);
  // read items
  for (int i = 0; i < num_keys_; ++i) {
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
  for (int i = 0; i < num_keys_; ++i) {
    key_type key;
    key.resize(items_[i].key_size);
    std::memcpy(key.data(), data, items_[i].key_size);
    data += items_[i].key_size;
    keys_.push_back(std::move(key));
  }
}

inline void InternalNode::write(Page *p) const {
  assert(less_than(PAGE_SIZE));
  char *data = p->get_data();
  std::memcpy(data, &num_keys_, sizeof(int));
  data += sizeof(int);

  std::memcpy(data, &parent_, sizeof(PageId));
  data += sizeof(PageId);

  for (int i = 0; i < num_keys_; ++i) {
    std::memcpy(data, &items_[i].key_size, sizeof(int));
    data += sizeof(int);
    // std::memcpy(data, &items_[i].key_pos, sizeof(int));
    //  data += sizeof(int);
    std::memcpy(data, &items_[i].child, sizeof(PageId));
    data += sizeof(PageId);
  }
  for (int i = 0; i < num_keys_; ++i) {
    std::memcpy(data, keys_[i].data(), keys_[i].size());
    data += keys_[i].size();
  }
}

// inline const key_type &InternalNode::split(InternalNode &new_node) {
//   int mid = num_keys_ / 2;
//   new_node.num_keys_ = num_keys_ - mid;

//   for (auto i = 0; i < new_node.num_keys_; ++i) {
//     new_node.items_.push_back(items_[mid + i]);
//     new_node.keys_.push_back(std::move(keys_[mid + i]));
//   }

//   items_.erase(items_.begin() + mid, items_.end());
//   keys_.erase(keys_.begin() + mid, keys_.end());
//   num_keys_ = mid;
//   return new_node.keys_[0];
// }

inline void InternalNode::move_half_to(InternalNode &new_node) {
  int mid = num_keys_ / 2;
  new_node.num_keys_ = num_keys_ - mid;

  for (auto i = 0; i < new_node.num_keys_; ++i) {
    new_node.items_.push_back(items_[mid + i]);
    new_node.keys_.push_back(std::move(keys_[mid + i]));
  }

  items_.erase(items_.begin() + mid, items_.end());
  keys_.erase(keys_.begin() + mid, keys_.end());
  num_keys_ = mid;
}