#pragma once
#include "BufferPool.hpp"
namespace bplus_tree {

class InteranlPage : public Page {
public:
  InteranlPage() {}
  ~InteranlPage() {}
};

class LeafPage : public Page {
public:
  LeafPage() {}
  ~LeafPage() {}
};

template <BufferPoolTraits BufferPoolType> class BPlusTree {
public:
private:
  std::unique_ptr<BufferPoolType> bfp_;
};

} // namespace bplus_tree