#pragma once
#include "buffer_pool.hpp"
#include <cstdint>

class InternalNode {
public:
  void read(Page *p) {}
  void write(Page *p) {}
};

class LeafNode {
public:
  void read(Page *p) {}
  void write(Page *p) {}
};

class BPlusTree {};