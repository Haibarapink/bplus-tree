#include "../bplus_tree.hpp"
#include "pure_test.hpp"
#include "convert.hpp"

#include <vector>
#include <algorithm>

PURE_TEST_INIT();

void internal_node_rm() {
    BufferPool bfp{"leaf_node_rm.db", 1};
    bfp.open();

    auto page = bfp.new_page();
    page->page_type = kInternalPageType;
    auto internal = InternalNode();
    internal.read(page);

    std::vector<bytes> datas;
    size_t count = 1000;

    for (auto i = 0; i < count; ++i) {
        int r = rand();
        std::string num = std::to_string(r);
        key_type k(num.begin(), num.end());
        internal.insert(k, i);
        datas.push_back(k);
    }

    std::sort(datas.begin(), datas.end(), [](const bytes& l, const bytes& r){
        int cmp = key_cmp(l, r);
        return cmp <= 0;
    });

    for (auto i = 0; i < 100; ++i) {
        // get rand number between 0~999
        int r = std::abs(rand()) % datas.size();
        auto k = datas[r];
        internal.remove(k);
        datas.erase(datas.begin() + r);
    }

    for (auto i = 0; i < datas.size(); ++i) {
        auto k = datas[i];
        auto id = internal.key(i);
        std::string_view k_str = std::string_view(k.data(), k.size());
        std::string_view id_str = std::string_view(id.data(), id.size());
        pure_assert(key_cmp(k, id) == 0) << "key: " << k_str << " id: " << id_str;
         
    }

    remove("leaf_node_rm.db");
}

void leaf_node_rm() {
BufferPool bfp{"leaf_node_rm.db", 1};
    bfp.open();

    auto page = bfp.new_page();
    page->page_type = kLeafPageType;
    auto leaf = LeafNode();
    leaf.read(page);

    std::vector<bytes> datas;
    size_t count = 1000;

    for (auto i = 0; i < count; ++i) {
        int r = rand();
        std::string num = std::to_string(r);
        key_type k(num.begin(), num.end());
        leaf.insert(k, k);
        datas.push_back(k);
    }

    std::sort(datas.begin(), datas.end(), [](const bytes& l, const bytes& r){
        int cmp = key_cmp(l, r);
        return cmp <= 0;
    });

    for (auto i = 0; i < 100; ++i) {
        // get rand number between 0~999
        int r = std::abs(rand()) % datas.size();
        auto k = datas[r];
        leaf.remove(k);
        datas.erase(datas.begin() + r);
    }

    for (auto i = 0; i < datas.size(); ++i) {
        auto k = datas[i];
        auto id = leaf.key(i);
        std::string_view k_str = std::string_view(k.data(), k.size());
        std::string_view id_str = std::string_view(id.data(), id.size());
        pure_assert(key_cmp(k, id) == 0) << "key: " << k_str << " id: " << id_str;
         
    }

    remove("leaf_node_rm.db");
}

int main(int argc, char* argv[]) {
    PURE_TEST_PREPARE();
    PURE_TEST_CASE(leaf_node_rm);
    PURE_TEST_CASE(internal_node_rm);
    PURE_TEST_RUN();
}