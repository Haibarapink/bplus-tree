#include "../buffer_pool.hpp"
#include "pure_test.hpp"
#include <cstring>

PURE_TEST_INIT();

void store_test() {
  BufferPool p{"abc", 32};
  p.open();
  auto page = p.new_page();
  int id = page->id;
  pure_assert(page != nullptr);
  page->page_type = 1;
  memcpy(page->get_data(), "hello world", 11);
  p.unpin(page->id, true);
  p.flush(id);
  p.close();

  BufferPool p2{"abc", 32};
  p2.open();
  auto page2 = p2.fetch(id);
  pure_assert(page2 != nullptr);
  pure_assert(page2->page_type == 1);
  pure_assert(memcmp(page2->get_data(), "hello world", 11) == 0);
  p2.unpin(page2->id, false);
  p2.close();
}

void rand_test() {
  BufferPool p{"hello", 32};
  p.open();
  std::vector<PageId> pids;
  for (auto i = 0; i < 1000; ++i) {
    auto page = p.new_page();
    pids.push_back(page->id);
    pure_assert(page != nullptr);
    PURE_TEST_EQ(page->pin_count, 1);

    std::string s = "hello world" + std::to_string(i);
    memcpy(page->get_data(), s.data(), s.size());
    page->page_type = kInternalPageType;
    p.unpin(page->id, true);

    PURE_TEST_EQ(page->pin_count, 0);
  }

  p.flush_all();
  p.close();

  BufferPool p2{"hello", 32};
  p2.open();

  for (auto i = 0; i < 1000; ++i) {
    auto page = p2.fetch(pids[i]);
    pure_assert(page != nullptr);
    PURE_TEST_EQ(page->pin_count, 1);

    std::string s = "hello world" + std::to_string(i);
    pure_assert(memcmp(page->get_data(), s.data(), s.size()) == 0)
        << "s : " << s << " data : " << page->get_data();

    p2.unpin(page->id, false);
    PURE_TEST_EQ(page->pin_count, 0);
    PURE_TEST_EQ(page->page_type, kInternalPageType);
  }

  auto page = p2.fetch(pids[0]);
  PURE_TEST_EQ(page->id, pids[0]);
  PURE_TEST_EQ(page->pin_count, 1);
  PURE_TEST_EQ(page->page_type, kInternalPageType);

  remove("hello");
}

int main(int argc, char *argv[]) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(store_test);
  PURE_TEST_CASE(rand_test);
  PURE_TEST_RUN();
}