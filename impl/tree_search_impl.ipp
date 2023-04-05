#pragma once

#include "../bplus_tree.hpp"

inline Page *BPlusTree::find_leaf(const key_type &key) {
  PageId page_id = root_;
  Page *p = buffer_pool_.fetch(page_id);
  while (p->page_type == kInternalPageType) {
    auto internal_node = InternalNode();
    internal_node.read(p);
    page_id = internal_node.child(key);
    p = buffer_pool_.fetch(page_id);
  }
  return p;
}

inline bool BPlusTree::search(const key_type &key, value_type &val) {
  auto p = find_leaf(key);
  auto leaf_node = LeafNode();
  leaf_node.read(p);
  buffer_pool_.unpin(p->id, false);
  return leaf_node.get(key, val);
}
