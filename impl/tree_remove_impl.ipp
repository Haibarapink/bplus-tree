#pragma once

#include "../bplus_tree.hpp"

inline bool BPlusTree::remove(const key_type &key) {
    auto page = find_leaf(key);
    if (page == nullptr) {
        return false;
    }

    auto leaf = LeafNode();
    leaf.read(page);
    bool leaf_remove_ok = leaf.remove(key);

    if (!leaf_remove_ok) {
        buffer_pool_.unpin(page->id);
        return false;
    }

    bool need_coalesce = leaf.less_than(coalesce_size());
    if (!need_coalesce) {
        write_node(leaf, page);
        return true;
    }

    // coalesce
    

}