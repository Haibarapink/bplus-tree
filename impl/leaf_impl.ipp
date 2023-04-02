#pragma once

#include "../bplus_tree.hpp"
#include <cassert>

void LeafNode::read(Page *p) {
  assert(p->page_type == kLeafPageType);
  this->p = p;
  char *data = p->get_data();
  std::memcpy(&num_keys_, data, sizeof(int));
  data += sizeof(int);
  std::memcpy(&parent_, data, sizeof(PageId));
  data += sizeof(PageId);
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

void LeafNode::write(Page *p) {
  assert(p->page_type == kLeafPageType);
  assert(less_than(PAGE_SIZE));

  this->p = p;
  char *data = p->get_data();
  std::memcpy(data, &num_keys_, sizeof(int));
  data += sizeof(int);
  std::memcpy(data, &parent_, sizeof(PageId));
  data += sizeof(PageId);
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

bool LeafNode::less_than(size_t page_size) {
  size_t size = meta_size();
  for (int i = 0; i < num_keys_; ++i) {
    size += sizeof(int) * 2 + items_[i].key_size + items_[i].val_size;
  }
  return size < page_size;
}