#include "../buffer_pool.hpp"
#include "../logger.hpp"
#include "pure_test.hpp"
#include <cstdio>
#include <random>
#include <set>
#include <system_error>
#include <variant>
#include <vector>

#include <cstdio>
#include <cstring>

PURE_TEST_INIT();

void lru_test() {
  // write the LruReplacer test here by pure_test.
  LruReplacer<int> replacer{4};
  replacer.put(1);
  replacer.put(2);
  replacer.put(3);
  replacer.put(4);
  replacer.put(5);
  // lru list: 5 4 3 2

  int victim;

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 2);

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 3);

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 4);

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 5);

  // lru list: empty
  PURE_TEST_FALSE_REPORT(replacer.victim(victim));

  replacer.put(1);
  replacer.put(2);
  replacer.put(3);
  // lru list: 3 2 1
  replacer.touch(2);
  // lru list: 2 3 1
  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 1);

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 3);

  replacer.victim(victim);
  PURE_TEST_EQ_REPORT(victim, 2);
}

void disk_test() {
  // write the DiskManager test here by pure_test.
  DiskManager dsk{"test.db", 1};
  char cur_buf[PAGE_SIZE] = {0};
  PURE_TEST_FALSE_REPORT(dsk.read_page(1, cur_buf));
  PURE_TEST_TRUE_REPORT(dsk.write_page(1, cur_buf));
  PageId pgid = dsk.alloc_page();
  PURE_TEST_EQ_REPORT(pgid, 1);

  std::string_view hello = "hello world";
  std::copy(hello.begin(), hello.end(), cur_buf);

  PURE_TEST_TRUE_REPORT(dsk.write_page(2, cur_buf));
  // clean cur_buf
  std::fill(cur_buf, cur_buf + PAGE_SIZE, 0);
  PURE_TEST_TRUE_REPORT(dsk.read_page(2, cur_buf));
  // cmp cur_buf with hello
  // PURE_TEST_EQ_REPORT(std::string_view{cur_buf, hello.size()}, hello);
  PURE_TEST_EQ_REPORT(std::string_view{cur_buf}, hello);

  // new 300 pages
  for (int i = 0; i < 300; ++i) {
    dsk.alloc_page();
  }

  size_t loop = 0;
  std::set<int> already_tested;

  while (loop < 200) {
    // get a random page id between 1 and 300
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 300);
    PageId random_page_id = dis(gen);
    if (already_tested.find(random_page_id) != already_tested.end()) {
      continue;
    }
    // get a random string
    std::uniform_int_distribution<> dis2(1, 100);
    std::string random_string(dis2(gen), 'a');
    std::copy(random_string.begin(), random_string.end(), cur_buf);
    // write the random string to the random page
    PURE_TEST_TRUE_REPORT(dsk.write_page(random_page_id, cur_buf));
    // clean cur_buf
    std::fill(cur_buf, cur_buf + PAGE_SIZE, 0);
    // read the random page
    PURE_TEST_TRUE_REPORT(dsk.read_page(random_page_id, cur_buf));
    // cmp cur_buf with random_string
    std::string_view cur_buf_view{cur_buf, random_string.size()};
    PURE_TEST_TRUE_REPORT(cur_buf_view == random_string);
    already_tested.insert(random_page_id);
    ++loop;
  }

  dsk.close();
  remove("test.db");
}

void meta_page_test() {
  char *buf = new char[PAGE_SIZE];
  BfpMetaPage meta_page{buf};
  meta_page.serliaze();
  for (auto i = 0;; ++i) {
    bool ok = meta_page.push_free_page(i);
    if (!ok) {
      PURE_TEST_EQ(meta_page.free_list_size, BfpMetaPage::MAX_FREE_LIST_SIZE);
      break;
    }
  }

  for (auto i = 0; i < meta_page.free_list_size; ++i) {
    PURE_TEST_EQ((meta_page)[i], i);
  }
}

class BufferPoolTest {
public:
  void write2page(Page *p, std::string_view s) {
    std::copy(s.begin(), s.end(), p->get_data());
  }

  // internal api test
  void internal_test() {
    // test the alloc free list
  }

  void basic_test() {
    // test the basic api
    const char *filename = "test.db";
    BufferPool bp{filename, 4};
    pure_assert(bp.open() == std::error_code{});
    pure_assert(bp.page_count() == 1);
    // alloc 20 pages
    std::vector<Page *> pages;
    for (int i = 0; i < 20; ++i) {
      auto p = bp.new_page();
      if (!p)
        break;
      sprintf(p->get_data(), "hello world%ld", p->id);
      pure_assert(p->id == i + 1);
      pages.push_back(p);
    }

    LOG_DEBUG << "alloc page count" << pages.size();

    // unpin all pages
    for (auto p : pages) {
      bp.unpin(p->id, true);
    }

    bp.flush_all();
    pure_assert(std::filesystem::file_size(filename) == 5 * PAGE_SIZE);
    bp.close();

    BufferPool bp2{filename, 1};
    bp2.open();
    for (auto p : pages) {
      auto p2 = bp2.fetch(p->id);
      pure_assert(p2) << "p2  is nullptr";
      std::string data_should_be = "hello world" + std::to_string(p->id);
      PURE_TEST_EQ(std::string_view{p2->get_data()},
                   std::string_view{p->get_data()});

      PURE_TEST_EQ(std::string_view{p2->get_data()}, data_should_be);
      bp2.unpin(p2->id, false);
    }
    bp2.close();
  }

  // alloc 5000 pages, and write every page with a string "hello world" +
  // rand_number, and read then equal
  void huge_test() {
    // test the huge data
    const char *filename = "test_huge.db";
    BufferPool bp{filename, 4};
    bp.open();
    // alloc 5000 pages
    std::vector<int> rands(5000);
    for (auto i = 0; i < 5000; ++i) {
      auto p = bp.new_page();
      pure_assert(p) << "alloc page failed";
      int randn = rand();
      sprintf(p->get_data(), "hello world%d", randn);
      rands[i] = randn;
      pure_assert(p->id == i + 1);
      bp.unpin(p->id, true);
    }

    pure_assert(bp.page_count() == 5001);

    bp.flush_all();
    pure_assert(std::filesystem::file_size(filename) == 5001 * PAGE_SIZE);

    pure_assert(bp.page_count() == 5001) << "page count : " << bp.page_count();
    pure_assert(bp.free_page_count() == 0);

    bp.close();

    BufferPool bp2{filename, 1};
    bp2.open();
    // check meta data
    pure_assert(bp2.page_count() == 5001);
    pure_assert(bp2.free_page_count() == 0);

    for (auto i = 0; i < 5000; ++i) {
      auto p = bp2.fetch(i + 1);
      pure_assert(p);
      std::string data_should_be = "hello world" + std::to_string(rands[i]);
      PURE_TEST_EQ(std::string_view{p->get_data()}, data_should_be);
      bp2.unpin(p->id, false);
    }
  }
};

void test_serilze() {
  BufferPool bfp{"test_ser.db", 1};
  bfp.open();
  auto p = bfp.new_page();
  p->page_type = kInternalPageType;
  bfp.unpin(p->id, true);
  bfp.flush_all();
  bfp.close();

  BufferPool bfp2{"test_ser.db", 1};
  bfp2.open();
  auto p2 = bfp2.fetch(p->id);
  pure_assert(p2);
  pure_assert(p2->page_type == kInternalPageType);
}

int main(int argc, char **argv) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(lru_test);
  PURE_TEST_CASE(disk_test);
  PURE_TEST_CASE(meta_page_test);
  PURE_TEST_CASE([] {
    BufferPoolTest().basic_test();
    remove("test.db");
  });

  PURE_TEST_CASE([] {
    BufferPoolTest().huge_test();
    remove("test_huge.db");
  });

  PURE_TEST_CASE(test_serilze);

  PURE_TEST_RUN();
}
