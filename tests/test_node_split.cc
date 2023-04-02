#include "../bplus_tree.hpp"
#include "convert.hpp"
#include "pure_test.hpp"

#include <map>

PURE_TEST_INIT()

void internal_sorted_test() {
  InternalNode internal_node;
  std::vector<int> v;
  for (auto i = 0; i < 10000; ++i) {
    key_type k;

    int n = std::abs(rand() % 100);

    k.push_back(n);
    v.push_back(n);
    internal_node.insert(k, i);
  }

  std::sort(v.begin(), v.end());

  size_t p = 0;

  for (; p < v.size(); ++p) {
    pure_assert(internal_node.key(p)[0] == v[p]);
  }
}

void internal_split_test() {}

int main(int, char **) {
  PURE_TEST_PREPARE();
  PURE_TEST_CASE(internal_sorted_test);
  PURE_TEST_RUN();
  return 0;
}