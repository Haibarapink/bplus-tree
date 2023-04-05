#include "../bplus_tree.hpp"
#include "pure_test.hpp"
PURE_TEST_INIT();

class BPlusTreeTest {
public:
  void make_test() {
    std::string_view db_name{"test"};
    BPlusTree tree{db_name, 32};
    for (auto i = 0; i < 5000; ++i) {
      bool insert_ok = tree.insert(std::to_string(i), std::to_string(i));
      PURE_TEST_EQ(insert_ok, true);
    }

    for (auto i = 0; i < 1000; ++i) {
      std::string val;

      bool search_ok = tree.search(std::to_string(i), val);
      PURE_TEST_EQ(search_ok, true);
      PURE_TEST_EQ(val, std::to_string(i));
    }
  }
};

void make_test() {
  BPlusTreeTest test;
  test.make_test();
}

int main(int argc, char **) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(make_test);
  PURE_TEST_RUN();
}