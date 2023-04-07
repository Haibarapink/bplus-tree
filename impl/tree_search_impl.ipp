#pragma once

//#include "../bplus_tree.hpp"

inline Page *BPlusTree::find_leaf(const key_type &key) {
  PageId page_id = root_;
  Page *p = buffer_pool_.fetch(page_id);
  while (p->page_type == kInternalPageType) {
    auto internal_node = InternalNode();
    internal_node.read(p);
    buffer_pool_.unpin(p->id);
    page_id = internal_node.child(key);
    // internal_node.print();
    p = buffer_pool_.fetch(page_id);
  }
  return p;
}

inline bool BPlusTree::search(const key_type &key, value_type &val) {
  auto p = find_leaf(key);
  auto leaf_node = LeafNode();
  leaf_node.read(p);
  buffer_pool_.unpin(p->id, false);
  // leaf_node.print();
  return leaf_node.get(key, val);
}

inline void BPlusTree::print() {
  PageId page_id = root_;
  key_type k;
  Page *p = find_leaf(k);
  while (p) {
    auto leaf_node = LeafNode();
    leaf_node.read(p);

    std::cout << "page " << p->id << " parent : " << leaf_node.parent()
              << " : \n";
    for (auto i = 0; i < leaf_node.size(); ++i) {
      auto k = leaf_node.key(i);
      std::string_view s = std::string_view{k.data(), k.size()};
      std::cout << " key " << s << " || ";
    }
    std::cout << std::endl;

    page_id = leaf_node.next();
    buffer_pool_.unpin(p->id, false);
    if (page_id == 0 || page_id == INVALID_PAGE_ID) {
      break;
    }
    p = buffer_pool_.fetch(page_id);
  }
}