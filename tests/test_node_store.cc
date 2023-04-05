#include "../bplus_tree.hpp"
#include "convert.hpp"
#include "pure_test.hpp"
#include <sys/types.h>

PURE_TEST_INIT();

// internal
void internal_store() {
  char *file = "internal.db";
  BufferPool bfp{file, 1};
  bfp.open();
  auto page = bfp.new_page();
  PURE_TEST_NE(page, nullptr);

  InternalNode node;
  page->page_type = kInternalPageType;

  node.read(page);
  node.set_parent(page->id);

  // insert 100 items
  for (int i = 0; i < 100; ++i) {
    auto key = std::to_string(i);
    key_type k_as_bytes = key_type{key.begin(), key.end()};
    node.insert(k_as_bytes, i);
    PageId id = node.child(k_as_bytes);
    pure_assert(id == i);
  }

  node.write(page);
  bfp.unpin(page->id, true);
  bfp.close();

  // read from disk
  BufferPool bfp2{file, 1};
  bfp2.open();
  auto page2 = bfp2.fetch(page->id);
  PURE_TEST_NE(page2, nullptr);

  InternalNode node2;
  node2.read(page2);
  pure_assert(node2 == node);

  remove(file);
}

void leaf_store() {
  char *file = "leaf.db";
  BufferPool bfp{file, 1};
  bfp.open();
  auto page = bfp.new_page();
  PURE_TEST_NE(page, nullptr);

  LeafNode node;
  page->page_type = kLeafPageType;

  node.read(page);
  node.set_parent(page->id);
  for (auto i = 0; i < 100; ++i) {
    key_type k, v;
    k.push_back(i);
    v.push_back(i);
    node.insert(k, v);
  }

  node.write(page);
  bfp.unpin(page->id, true);
  bfp.close();

  // read from disk
  BufferPool bfp2{file, 1};
  bfp2.open();
  auto page2 = bfp2.fetch(page->id);
  PURE_TEST_NE(page2, nullptr);
  LeafNode node2;
  node2.read(page2);
  pure_assert(node2 == node);
  for (auto i = 0; i < 100; ++i) {
    key_type k;
    k.push_back(i);
    key_type v;
    // std::cout << (int)node2.fetch(i)[0] << std::endl;
    pure_assert(node2.get(k, v)) << (int)node2.fetch(i)[0] << " " << i;
  }

  remove(file);
}

int main(int, char **) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(internal_store);
  PURE_TEST_CASE(leaf_store);
  PURE_TEST_RUN();
}