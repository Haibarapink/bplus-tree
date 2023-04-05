#pragma once

#include "../bplus_tree.hpp"
#include <sys/types.h>

// set type = kLeafPageType
// set parent = INVALID_PAGE_ID
// insert key, value
// write to page
// unpin page
inline bool BPlusTree::make_tree(key_type k, value_type v) {
  auto root = buffer_pool_.new_page();
  if (!root) {
    LOG_DEBUG << "new page failed";
    return false;
  }

  root->page_type = kLeafPageType;

  auto leaf_node = LeafNode();
  leaf_node.set_parent(INVALID_PAGE_ID);
  leaf_node.insert(std::move(k), std::move(v));
  leaf_node.write(root);
  root_ = root->id;
  buffer_pool_.unpin(root->id, true);

  LOG_DEBUG << "make tree : " << root->id;

  return true;
}

// set type = kInternalPageType
// set parent = INVALID_PAGE_ID
// insert empty key, left
// insert key, right
// write to page
// unpin page
inline bool BPlusTree::make_root(key_type k, PageId left, PageId right) {
  auto root = buffer_pool_.new_page();
  if (!root) {
    LOG_DEBUG << "new page failed";
    return false;
  }

  root->page_type = kInternalPageType;

  auto internal_node = InternalNode();
  internal_node.set_parent(INVALID_PAGE_ID);

  key_type empty_k;
  internal_node.insert(std::move(empty_k), left);
  internal_node.insert(std::move(k), right);

  internal_node.write(root);
  root_ = root->id;
  buffer_pool_.unpin(root->id, true);

  set_parent(left, root->id);
  set_parent(right, root->id);

  LOG_DEBUG << "make root : " << root->id << " left " << left << " right "
            << right;
  return true;
}

inline bool BPlusTree::insert(key_type key, value_type val) {
  if (root_ == INVALID_PAGE_ID) {
    return make_tree(std::move(key), std::move(val));
  }

  auto p = find_leaf(key);
  auto leaf_node = LeafNode();
  leaf_node.read(p);
  leaf_node.insert(key, val);

  if (leaf_node.less_than(PAGE_SIZE)) {
    leaf_node.write(p);
    buffer_pool_.unpin(p->id, true);
    return true;
  }

  auto new_page = buffer_pool_.new_page();
  if (!new_page) {
    leaf_node.remove(key);
    LOG_DEBUG << "new page failed";
    buffer_pool_.unpin(p->id, false);
    return false;
  }
  new_page->page_type = kLeafPageType;

  auto new_leaf_node = LeafNode();
  new_leaf_node.set_parent(leaf_node.parent());
  leaf_node.move_half_to(new_leaf_node);
  LOG_DEBUG << "move half to new leaf page " << new_page->id;

  size_t left_id = p->id, right_id = new_page->id;

  leaf_node.write(p);
  new_leaf_node.write(new_page);

  buffer_pool_.unpin(left_id, true);
  buffer_pool_.unpin(right_id, true);

  LOG_DEBUG << "insert parent " << leaf_node.parent() << " left " << left_id
            << " right " << right_id;
  return insert_parent(leaf_node.parent(), p->id, new_page->id,
                       new_leaf_node.key(0));
}

inline bool BPlusTree::insert_parent(PageId parent, PageId left, PageId right,
                                     key_type key) {
  if (parent == INVALID_PAGE_ID || parent == 0) {
    LOG_DEBUG << "make root"
              << " left" << left << " right" << right;
    return make_root(std::move(key), left, right);
  }

  auto page = buffer_pool_.fetch(parent);
  assert(page);
  auto node = InternalNode();
  if (page->page_type == kLeafPageType) {
    LOG_DEBUG << "here";
    auto page2 = buffer_pool_.fetch(parent);
  }
  node.read(page);
  node.insert(std::move(key), right);
  if (node.less_than(PAGE_SIZE)) {
    node.write(page);
    buffer_pool_.unpin(page->id, true);
    return true;
  }

  auto new_page = buffer_pool_.new_page();
  if (!new_page) {
    LOG_DEBUG << "new page failed";
    buffer_pool_.unpin(page->id, false);
    return false;
  }
  new_page->page_type = kInternalPageType;

  auto new_node = InternalNode();
  new_node.set_parent(node.parent());
  node.move_half_to(new_node);
  LOG_DEBUG << "move half to new internal page " << new_page->id;
  // set parent
  for (auto i = 0; i < node.size(); ++i) {
    auto child_id = node.item(i).child;
    assert(child_id != INVALID_PAGE_ID);
    set_parent(child_id, new_page->id);
    LOG_DEBUG << "set parent " << page->id << " to child " << child_id;
  }

  parent = node.parent();
  left = page->id;
  right = new_page->id;
  key_type k = new_node.key(0);

  node.write(page);
  new_node.write(new_page);

  buffer_pool_.unpin(page->id, true);
  buffer_pool_.unpin(new_page->id, true);

  return insert_parent(parent, left, right, std::move(k));
}