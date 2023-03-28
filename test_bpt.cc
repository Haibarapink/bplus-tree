#include "bplus_tree.hpp"
#include "buffer_pool.hpp"
#include "pure_test.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <sys/types.h>

PURE_TEST_INIT();

void test_internal_node_op() {
  BufferPool bfp{"test_internal_node.db", 1};

  auto page = bfp.new_page();
  pure_assert(page->id == 1) << "page->id: " << page->id;
  page->deserialize();
  InternalNode node = InternalNode{};
  page->page_type = kInternalPageType;
  page->serliaze();
  page->set_dirty();
  node.read(page);

  pure_assert(page->id == 1);
  pure_assert(page->page_type == kInternalPageType);

  std::vector<key_type> sorted_keys;

  for (auto i = 0; i < 100; ++i) {
    std::string key = "hello" + std::to_string(i);
    key_type kb(key.begin(), key.end());
    sorted_keys.push_back(kb);
    node.insert(kb, i);
  }

  node.write(page);
  pure_assert(page->page_type == kInternalPageType);
  pure_assert(page->id == 1) << "page->id: " << page->id;
  bfp.unpin(page->id, true);
  bfp.flush_all();
  bfp.close();

  BufferPool bfp2{"test_internal_node.db", 1};
  auto page2 = bfp2.fetch(1);
  InternalNode node2 = InternalNode{};
  node2.read(page2);

  PURE_TEST_EQ(node2.size(), 100);

  std::sort(sorted_keys.begin(), sorted_keys.end());

  for (auto i = 0; i < 100; ++i) {
    auto key = node2.key(i);
    pure_assert(key == sorted_keys[i]);
  }

  remove("test_internal_node.db");
}

void test_leaf_node_op() {
  BufferPool bfp{"test_leaf_node.db", 1};

  auto page = bfp.new_page();
  pure_assert(page->id == 1) << "page->id: " << page->id;
  auto node = LeafNode{};
  page->page_type = kLeafPageType;
  node.read(page);

  std::vector<key_type> sorted_keys;

  for (auto i = 0; i < 200; ++i) {
    LeafNode::kv_type kv;
    // kv = "hello" , "world"
    std::string k = "hello" + std::to_string(i);
    std::string v = "world" + std::to_string(i);

    sorted_keys.push_back(key_type(k.begin(), k.end()));

    kv.first = key_type(k.begin(), k.end());
    kv.second = value_type(v.begin(), v.end());
    node.insert(kv);
  }

  node.write(page);
  // check meta data
  pure_assert(page->page_type == kLeafPageType);
  pure_assert(page->id == 1) << "page->id: " << page->id;

  bfp.unpin(page->id, true);
  bfp.flush_all();
  bfp.close();

  BufferPool bfp2{"test_leaf_node.db", 1};
  auto page2 = bfp2.fetch(1);
  auto node2 = LeafNode{};
  node2.read(page2);

  std::sort(sorted_keys.begin(), sorted_keys.end());

  for (auto i = 0; i < 200; ++i) {
    // kv = "hello" , "world"
    auto &kv2 = node2.get(i);
    auto kv = node2.get(i);
    pure_assert(kv == kv2);
  }

  remove("test_leaf_node.db");
}

int main() {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(test_internal_node_op);
  PURE_TEST_RUN();
}