#pragma once
#include <cstddef>
#include <list>
#include <map>

constexpr size_t DEFAULT_LRU_CAPACITY = 16;

template <typename T, typename D>
concept ReplacerTraits = requires(T replacer, D put_d, const D &const_d,
                                  D &victim_d) {
  { replacer.put(put_d) } -> std::same_as<void>;
  { replacer.touch(const_d) } -> std::same_as<bool>;
  { replacer.remove(const_d) } -> std::same_as<void>;
  { replacer.victim(victim_d) } -> std::same_as<bool>;
};

template <typename T> class LruReplacer {
public:
  LruReplacer() : capacity_(DEFAULT_LRU_CAPACITY) {}

  LruReplacer(size_t cap) : capacity_(cap) {}

  void put(T t) {
    if (auto iter = dir_.find(t); iter != dir_.end()) {
      auto list_iter = iter->second;
      lru_list_.erase(list_iter);
      dir_.erase(iter);
    } else {
      if (capacity_ == lru_list_.size()) {
        auto &&back_value = lru_list_.back();
        dir_.erase(back_value);
        lru_list_.pop_back();
      }
    }
    lru_list_.push_front(t);
    dir_.emplace(std::move(t), lru_list_.begin());
  }

  // @brief If replacer contain t return true and update the t in replacer, else
  // return false,
  bool touch(const T &t) {
    if (auto iter = dir_.find(t); iter != dir_.end()) {
      auto list_iter = iter->second;
      lru_list_.erase(list_iter);
      dir_[t] = lru_list_.begin();
      lru_list_.push_front(t);
      return true;
    }
    return false;
  }

  void remove(const T &t) {
    if (auto iter = dir_.find(t); iter != dir_.end()) {
      lru_list_.erase(iter->second);
      dir_.erase(iter);
    }
  }

  bool victim(T &t) {
    if (!lru_list_.empty()) {
      t = lru_list_.back();
      dir_.erase(t);
      lru_list_.pop_back();
      return true;
    }
    return false;
  }

private:
  std::list<T> lru_list_;
  std::map<T, typename std::list<T>::iterator> dir_;

  size_t capacity_;
};

template <typename T> class FIFOReplacer {
public:
  FIFOReplacer() = default;

  void put(T t) {
    if (auto iter = dir_.find(t); iter != dir_.end()) {
      fifo_list_.erase(iter->second);
      dir_.erase(iter);
    }
    fifo_list_.push_front(t);
    dir_.emplace(std::move(t), fifo_list_.begin());
  }

  void remove(const T &t) {
    if (auto iter = dir_.find(t); iter != dir_.end()) {
      fifo_list_.erase(iter->second);
      dir_.erase(iter);
    }
  }

  bool victim(T &t) {
    if (!fifo_list_.empty()) {
      t = fifo_list_.back();
      dir_.erase(t);
      fifo_list_.pop_back();
      return true;
    }
    return false;
  }

  bool touch(const T &t) { return dir_.count(t) > 0; }

private:
  std::map<T, typename std::list<T>::iterator> dir_;
  std::list<T> fifo_list_;
};

// TODO lfu replacer