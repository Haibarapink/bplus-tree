#pragma once

#include "../bplus_tree.hpp"
#include <sys/types.h>

// set type = kLeafPageType
// set parent = INVALID_PAGE_ID
// insert key, value
// write to page
// unpin page
bool BPlusTree::make_tree(key_type k, value_type v) {
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
  return true;
}

// set type = kInternalPageType
// set parent = INVALID_PAGE_ID
// insert empty key, left
// insert key, right
// write to page
// unpin page
bool BPlusTree::make_root(key_type k, PageId left, PageId right) {
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
  return true;
}

bool BPlusTree::insert(key_type key, value_type val) {
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
  LOG_DEBUG << "move half to new page  left size " << leaf_node.size()
            << " new leaf size " << new_leaf_node.size();

  size_t left_id = p->id, right_id = new_page->id;
  buffer_pool_.unpin(left_id, true);
  buffer_pool_.unpin(right_id, true);

  return insert_parent(leaf_node.parent(), p->id, new_page->id,
                       new_leaf_node.key(0));
}

bool BPlusTree::insert_parent(PageId parent, PageId left, PageId right,
                              key_type key) {
  if (parent == INVALID_PAGE_ID || parent == 0) {
    LOG_DEBUG << "make root"
              << " left" << left << " right" << right;
    return make_root(std::move(key), left, right);
  }

  auto parent_page = buffer_pool_.fetch(parent);
  auto internal_node = InternalNode();
  internal_node.read(parent_page);
  internal_node.insert(std::move(key), right);
  if (internal_node.less_than(PAGE_SIZE)) {
    internal_node.write(parent_page);
    buffer_pool_.unpin(parent_page->id, true);
    return true;
  }

  auto new_page = buffer_pool_.new_page();
  if (!new_page) {
    LOG_DEBUG << "new page failed";
    buffer_pool_.unpin(parent_page->id, false);
    return false;
  }
  new_page->page_type = kInternalPageType;

  auto new_internal_node = InternalNode();
  new_internal_node.set_parent(internal_node.parent());
  internal_node.move_half_to(new_internal_node);
  // set parent
  for (auto i = 0; i < internal_node.size(); ++i) {
    auto child_id = internal_node.item(i).child;
    assert(child_id != INVALID_PAGE_ID);
    set_parent(child_id, new_page->id);
    LOG_DEBUG << "set parent " << parent_page->id << " to child " << child_id;
  }

  buffer_pool_.unpin(parent_page->id, true);
  buffer_pool_.unpin(new_page->id, true);

  parent = internal_node.parent();
  left = internal_node.item(0).child;
  right = new_internal_node.item(0).child;
  key_type k = new_internal_node.key(0);

  return insert_parent(parent, left, right, std::move(k));
}