#pragma once
#include "buffer_pool.hpp"
#include <cstdint>
#include <string_view>

using bytes = std::vector<char>;
using key_type = bytes;
using value_type = bytes;

class InternalNode {
public:
  struct Item {
    int key_size;
    int key_pos;
    PageId child; // pointer to right child
  };

  // read from page
  void read(Page *p) {
    assert(p->page_type == kInternalPageType);
    // read key nums
    char *data = p->get_data();
    std::memcpy(&num_keys, data, sizeof(int));
    data += sizeof(int);

    // read items
    for (int i = 0; i < num_keys; ++i) {
      Item item;
      std::memcpy(&item.key_size, data, sizeof(int));
      data += sizeof(int);
      std::memcpy(&item.key_pos, data, sizeof(int));
      data += sizeof(int);
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
      keys_.push_back(key);
    }
  }

  // write to page
  // Clean the page and write the data to the page
  void write(Page *p) {
    char *data = p->get_data();
    std::memcpy(data, &num_keys, sizeof(int));
    data += sizeof(int);
    for (int i = 0; i < num_keys; ++i) {
      std::memcpy(data, &items_[i].key_size, sizeof(int));
      data += sizeof(int);
      std::memcpy(data, &items_[i].key_pos, sizeof(int));
      data += sizeof(int);
      std::memcpy(data, &items_[i].child, sizeof(PageId));
      data += sizeof(PageId);
    }
    for (int i = 0; i < num_keys; ++i) {
      std::memcpy(data, keys_[i].data(), keys_[i].size());
      data += keys_[i].size();
    }
  }

  const Item &item(size_t idx) { return items_[idx]; }

  const key_type &key(size_t idx) { return keys_[idx]; }

private:
  Page *p;
  int num_keys;
  std::vector<Item> items_;
  std::vector<key_type> keys_;
};

class LeafNode {
public:
  void read(Page *p) {}
  void write(Page *p) {}
};

class BPlusTree {};