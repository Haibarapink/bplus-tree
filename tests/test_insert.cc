#include "../bplus_tree.hpp"
#include "pure_test.hpp"
PURE_TEST_INIT();

class BPlusTreeTest {
public:
  void make_test() {
    std::string_view db_name{"test"};
    BPlusTree tree{db_name, 32};
    for (auto i = 0; i < 10000; ++i) {
      bool insert_ok = tree.insert(std::to_string(i), std::to_string(i));
      PURE_TEST_EQ(insert_ok, true);
    }

    for (auto i = 0; i < 10000; ++i) {
      std::string val;

      bool search_ok = tree.search(std::to_string(i), val);
      PURE_TEST_EQ(search_ok, true) << "i " << i;
      PURE_TEST_EQ(val, std::to_string(i)) << val;
    }
    remove("test");
  }

  void check_buffer_pool_clean() {
    std::string_view db_name{"test"};
    BPlusTree tree{db_name, 32};
    for (auto i = 0; i < 10000; ++i) {
      bool insert_ok = tree.insert(std::to_string(i), std::to_string(i));
      PURE_TEST_EQ(insert_ok, true);
    }

    for (auto &&p : tree.buffer_pool_.pages_) {
      auto page = p.get();
      pure_assert(page->pin_count == 0)
          << "page " << page->id << " pin_count " << page->pin_count;
    }
    remove("test");
  }
};

void make_test() {
  BPlusTreeTest test;
  test.make_test();
}

void check_buffer_pool_clean() {
  BPlusTreeTest test;
  test.check_buffer_pool_clean();
}

int main(int argc, char **) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(make_test);
  PURE_TEST_CASE(check_buffer_pool_clean);
  PURE_TEST_RUN();
}