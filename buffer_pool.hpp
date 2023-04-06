#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#include <variant>
#include <vector>

#include "bplus_tree.hpp"
#include "logger.hpp"
#include "replacer.hpp"

constexpr int kInternalPageType = 1;
constexpr int kLeafPageType = 2;

// defines the page id type and the invalid page id
using PageId = long;
constexpr PageId INVALID_PAGE_ID = -1;
constexpr size_t PAGE_SIZE = 1024;

class DiskManager {
public:
  DiskManager(std::string_view filename, PageId next_start_id)
      : db_filename_(filename), next_page_id_(next_start_id) {

    std::filesystem::path file_path(db_filename_.data());
    if (std::filesystem::exists(file_path) &&
        std::filesystem::is_regular_file(file_path)) {
      db_io_ = fopen(filename.data(), "rb+");
    } else {
      // create file
      db_io_ = fopen(filename.data(), "wb+");
    }

    if (db_io_ == nullptr) {
      throw std::runtime_error("open file failed");
    }
  }

  ~DiskManager() { close(); }

  // @brief: set the next page id
  void set_pid(PageId id) {
    std::unique_lock<std::mutex> lock{mutex_};
    next_page_id_ = id;
  }

  bool read_page(PageId id, char *dst);
  bool write_page(PageId id, char *src);

  void close();

  PageId alloc_page() {
    std::unique_lock<std::mutex> lock{mutex_};
    return next_page_id_++;
  }

  PageId current_page_id() const { return next_page_id_; }

private:
  std::mutex mutex_;
  std::string db_filename_;

  bool close_ = false;
  FILE *db_io_ = nullptr;

  PageId next_page_id_ = 1; // default 1
};

// | page id | data |
class Page {
public:
  Page(char *d) : data(d) {}
  ~Page() {}

  std::int8_t dirty = 0;
  size_t pin_count = 0;

  // need store in disk
  PageId id = INVALID_PAGE_ID;
  int page_type = 0;

  std::unique_ptr<char> data = nullptr;

  bool is_dirty() const { return dirty == 1; }
  void set_dirty() { dirty = 1; }
  void clear_dirty() { dirty = 0; }

  virtual void deserialize() {
    std::memcpy(&id, data.get(), sizeof(PageId));
    std::memcpy(&page_type, data.get() + sizeof(PageId), sizeof(int));
  }

  virtual void serliaze() {
    if (data == nullptr) {
      //  data = std::make_unique<char>(PAGE_SIZE);
      assert(false);
    }
    std::memcpy(data.get(), &id, sizeof(PageId));
    std::memcpy(data.get() + sizeof(PageId), &page_type, sizeof(int));
  }

  static size_t offset() { return sizeof(PageId) + sizeof(int); }

  virtual size_t data_offset() const { return sizeof(PageId) + sizeof(int); }
  virtual char *get_data() {
    if (!data) {
      return nullptr;
    }
    return (data.get() + data_offset());
  }
};

// |id| page count | free list size | free list | page 1 | page 2 | ... |
class BfpMetaPage : public Page {
public:
  BfpMetaPage(char *d) : Page(d) {}

  size_t page_count = 1;
  size_t free_list_size = 0;
  PageId next = 0;  // next free list page id
  PageId prev = -1; // prev free list page id

  constexpr static size_t offset =
      sizeof(PageId) + sizeof(size_t) * 2 + sizeof(PageId) * 2; // 24
  constexpr static size_t MAX_FREE_LIST_SIZE =
      (PAGE_SIZE - offset) / sizeof(PageId);

  PageId operator[](size_t idx) {
    assert(idx < free_list_size);
    PageId id;
    std::memcpy(&id, data.get() + offset + idx * sizeof(PageId),
                sizeof(PageId));
    return id;
  }

  bool push_free_page(PageId id) {
    if (free_list_size >= MAX_FREE_LIST_SIZE) {
      return false;
    }
    std::memcpy(data.get() + offset + free_list_size * sizeof(PageId), &id,
                sizeof(PageId));
    free_list_size++;
    return true;
  }

  PageId pop_free_page() {
    if (free_list_size == 0) {
      return INVALID_PAGE_ID;
    }
    PageId id;
    std::memcpy(&id,
                data.get() + offset + (free_list_size - 1) * sizeof(PageId),
                sizeof(PageId));
    free_list_size--;
    return id;
  }

  void deserialize() override {
    Page::deserialize();
    std::memcpy(&page_count, data.get() + sizeof(PageId), sizeof(size_t));
    std::memcpy(&free_list_size, data.get() + sizeof(PageId) + sizeof(size_t),
                sizeof(size_t));
    std::memcpy(&next, data.get() + sizeof(PageId) + sizeof(size_t) * 2,
                sizeof(PageId));
    std::memcpy(&prev,
                data.get() + sizeof(PageId) + sizeof(size_t) * 2 +
                    sizeof(PageId),
                sizeof(PageId));
  }

  // serialize the meta page all the data
  void serliaze() override {
    Page::serliaze();
    std::memcpy(data.get() + sizeof(PageId), &page_count, sizeof(size_t));
    std::memcpy(data.get() + sizeof(PageId) + sizeof(size_t), &free_list_size,
                sizeof(size_t));
    std::memcpy(data.get() + sizeof(PageId) + sizeof(size_t) * 2, &next,
                sizeof(PageId));
    std::memcpy(data.get() + sizeof(PageId) + sizeof(size_t) * 2 +
                    sizeof(PageId),
                &prev, sizeof(PageId));
  }

  // This data not include the meta data
  size_t data_offset() const override { return offset; }

  // This data not include the meta data
  char *get_data() override {
    if (!data) {
      return nullptr;
    }
    return data.get() + data_offset();
  }
};

template <ReplacerTraits<size_t> ReplacerType> class DefaultBufferPool {
public:
  friend class BufferPoolTest;
  friend class BPlusTreeTest;

  using PagePtr = std::unique_ptr<Page>;

  DefaultBufferPool(std::string_view db, size_t bfp_size)
      : name_(db), bfp_size_(bfp_size) {}

  std::error_code open() {
    PageId next_id = 1;

    bool file_exists = std::filesystem::exists(name_) &&
                       std::filesystem::is_regular_file(name_);

    if (!file_exists && std::filesystem::is_directory(name_)) {
      return std::make_error_code(std::errc::is_a_directory);
    }

    open_ = true;
    disk_manager_ = std::make_unique<DiskManager>(name_, next_id);

    char *meta_data = new char[PAGE_SIZE];
    std::memset(meta_data, 0, PAGE_SIZE);

    if (file_exists) {
      // Serialize the meta page
      disk_manager_->read_page(0, meta_data);
      meta_page_ = std::make_unique<BfpMetaPage>(meta_data);
      meta_page_->deserialize();

      disk_manager_->set_pid(meta_page_->page_count + 1);
    } else {
      // Allocate a new meta page
      meta_page_ = std::make_unique<BfpMetaPage>(meta_data);
      meta_page_->id = 0;
      meta_page_->page_count = 1;
      meta_page_->serliaze();

      disk_manager_->set_pid(1);
    }

    for (size_t i = 0; i < bfp_size_; ++i) {
      char *buf = new char[PAGE_SIZE];
      std::memset(buf, 0, PAGE_SIZE);
      PagePtr page = std::make_unique<Page>(buf);
      pages_.emplace_back(std::move(page));
      replacer_.put(i);
    }

    LOG_DEBUG << "meta page"
              << "page count " << meta_page_->page_count << "free list size "
              << meta_page_->free_list_size << "next " << meta_page_->next
              << "prev " << meta_page_->prev;

    return std::error_code();
  }

  //@brief contain the page on the disk file
  bool contain(PageId pid) {
    assert(open_);
    if (pid == INVALID_PAGE_ID) {
      return false;
    }
    return pid < meta_page_->page_count;
  }

  Page *new_page() {
    assert(open_);
    PageId id = disk_manager_->alloc_page();

    Page *page = fetch(id);
    if (page == nullptr) {
      return nullptr;
    }

    page->id = id;
    page->pin_count = 1;
    page->dirty = 1;
    page->serliaze();

    meta_page_->page_count++;
    meta_page_->serliaze();
    meta_page_->dirty = 1;
    // disk_manager_->write_page(0, meta_page_->data.get());

    return page;
  }

  Page *fetch(PageId page_id) {
    assert(open_);

    if (page_id == INVALID_PAGE_ID) {
      assert(false);
    }

    // check if the page is in the buffer pool
    auto it = page_map_.find(page_id);
    if (it != page_map_.end()) {
      // if the page is in the buffer pool, return the page
      assert(it->second->id == page_id);
      it->second->pin_count++;
      auto idx = page_index_[page_id];
      replacer_.remove(idx);
      return it->second;
    }

    // fetch from replacer
    size_t idx;
    bool found = replacer_.victim(idx);
    if (!found) {
      LOG_DEBUG << "replacer is empty";
      return nullptr;
    }

    Page *new_page = pages_[idx].get();
    if (new_page->dirty == 1) {
      flush(new_page->id);
      new_page->dirty = false;
    }

    change_page(new_page, page_id);
    page_index_[page_id] = idx;
    page_map_[page_id] = new_page;

    auto ok = disk_manager_->read_page(page_id, new_page->data.get());
    new_page->pin_count = 1;
    new_page->deserialize();
    new_page->id = page_id;
    // TODO need to handle the error ?
    return new_page;
  }

  void pin(PageId page_id) {
    assert(open_);
    auto it = page_map_.find(page_id);
    if (it != page_map_.end()) {
      assert(it->second->id == page_id);
      it->second->pin_count++;
      replacer_.remove(page_index_[page_id]);
    }
  }

  void unpin(PageId page_id, bool is_dirty = false) {
    assert(open_);
    auto it = page_map_.find(page_id);
    if (it != page_map_.end()) {
      assert(it->second->id == page_id);
      it->second->pin_count--;
      if (is_dirty) {
        it->second->dirty = 1;
      }
      if (it->second->pin_count == 0) {
        assert(page_index_.find(page_id) != page_index_.end());
        replacer_.put(page_index_[page_id]);
      }
    }
  }

  void flush(PageId page_id) {
    assert(open_);

    auto it = page_map_.find(page_id);
    if (it != page_map_.end()) {
      assert(page_id == it->second->id);
      LOG_DEBUG << "flush page " << page_id;
      it->second->serliaze();
      bool ok = disk_manager_->write_page(page_id, it->second->data.get());
      if (ok) {
        it->second->clear_dirty();
      }
    }
  }

  void flush_all() {
    assert(open_);
    for (auto &page : pages_) {
      if (page->dirty == 1) {
        page->serliaze();
        bool ok = disk_manager_->write_page(page->id, page->data.get());
        LOG_DEBUG << "flush page " << page->id;
        if (!ok) {
          LOG_DEBUG << page->id << " flush failed";
        } else {
          page->clear_dirty();
        }
      }
    }
  }

  bool write_meta() {
    assert(open_);
    if (meta_page_ == nullptr) {
      return false;
    }
    return disk_manager_->write_page(0, meta_page_->data.get());
  }

  void close() {
    assert(open_);
    if (meta_page_ && meta_page_->dirty == 1) {
      write_meta();
    }
    flush_all();
    disk_manager_->close();
  }

  size_t page_count() const {
    assert(open_);
    return meta_page_->page_count;
  }
  size_t free_page_count() const {
    assert(open_);
    return meta_page_->free_list_size;
  }
  size_t buffer_size() const {
    assert(open_);
    return pages_.size();
  }

private:
  // @brief: change the page id and reset the dirty bit
  void change_page(Page *page, PageId page_id) {
    page_index_.erase(page->id);
    page_map_.erase(page->id);
    page->id = page_id;
    page->dirty = 0;
    page->pin_count = 0;
    page->serliaze();
  }

  // new a free list page
  PageId new_free_list_page() { throw std::runtime_error("unimplemented"); }

  bool open_ = false;

  ReplacerType replacer_;
  std::unique_ptr<BfpMetaPage> meta_page_ = nullptr;
  std::unique_ptr<BfpMetaPage> free_list_page_ =
      nullptr; // just like a linked list
  std::vector<std::unique_ptr<Page>> pages_;

  std::unique_ptr<DiskManager> disk_manager_;

  std::map<PageId, size_t> page_index_;
  std::map<PageId, Page *> page_map_;

  std::string name_;
  size_t bfp_size_;
};

inline bool DiskManager::read_page(PageId id, char *dst) {
  std::unique_lock<std::mutex> lock{mutex_};
  size_t file_size = std::filesystem::file_size(db_filename_);
  unsigned long offset = id * PAGE_SIZE;
  if (offset <= file_size) {
    fseek(db_io_, offset, SEEK_SET);
    size_t read_size =
        file_size - offset > PAGE_SIZE ? PAGE_SIZE : file_size - offset;
    auto count = fread(dst, read_size, 1, db_io_);
    if (count != 1) {
      return false;
    }
    return true;
  }
  return false;
}

inline bool DiskManager::write_page(PageId id, char *src) {
  std::unique_lock<std::mutex> lock{mutex_};

  size_t offset = id * PAGE_SIZE;

  int seek = fseek(db_io_, offset, SEEK_SET);
  if (seek != 0) {
    LOG_DEBUG << "fseek fail!"
              << "Reason maybe is file size is not enough. "
              << "Page id : " << id << ". Seek offset : " << offset
              << ". File size : " << std::filesystem::file_size(db_filename_)
              << ". Write will still continue";
    // return false;
  }

  size_t count = fwrite(src, PAGE_SIZE, 1, db_io_);
  if (count != 1) {
    LOG_DEBUG << "fwrite fail!";
    return false;
  }

  return fflush(db_io_) == 0;
}

inline void DiskManager::close() {
  std::unique_lock<std::mutex> lock{mutex_};
  if (db_io_ && close_ == false) {
    fclose(db_io_);
    close_ = true;
  }
}

using BufferPool = DefaultBufferPool<LruReplacer<size_t>>;