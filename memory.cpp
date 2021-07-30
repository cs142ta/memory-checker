#include "memory.h"
#undef new
#undef delete

#include <iostream>
#include <string>
#include <vector>

// for malloc and free
#include <cstdlib>

struct Record {
  size_t size;
  void *ptr;
  AllocInfo info;
  AllocInfo free_info;
  bool complete = false;

  Record(size_t size, void *ptr) : size(size), ptr(ptr) {}

  void leak_msg() {
    std::cout << "  " << info;
    std::cout << ": " << size << " bytes allocated here were never freed."
              << std::endl;
  }
};

// track all allocations and frees
struct MemTrack {
  size_t alloced;

  std::vector<Record> allocated_records;
  std::vector<Record> freed_records;
  bool lock;

  AllocInfo last_delete;

  MemTrack() {
    alloced = 0;
    lock = false;
  }

  ~MemTrack() {
    if (!alloced) {
      // all memory was freed
      print_header();
      std::cout << "no issues detected" << std::endl;
      print_footer();
      return;
    }

    print_header();
    std::cout << "LEAK SUMMARY:" << std::endl;
    std::cout << "  leaked: " << alloced << " bytes in "
              << allocated_records.size() << " allocations" << std::endl;
    std::cout << std::endl;
    std::cout << "LEAK DETAILS:" << std::endl;
    for (Record r : allocated_records) {
      if (r.complete) {
        r.leak_msg();
      }
    }
    print_footer();
  }

  void print_header() {
    std::cout << std::endl;
    std::cout << "---------- memory checker ----------" << std::endl;
    std::cout << std::endl;
  }

  /* TODO: This may not be necessary with zybooks... */
  void print_footer() {
    std::cout << std::endl;
    std::cout << "------------------------------------" << std::endl;
    std::cout << std::endl;
  }

  bool check_double_free(void *ptr) {
    for (size_t i = 0; i < freed_records.size(); ++i) {
      if (freed_records.at(i).ptr == ptr) {
        // double free found

        Record r = freed_records.at(i);
        print_header();
        std::cout << "DOUBLE-FREE SUMMARY:" << std::endl;
        std::cout << "  attempted to free pointer " << ptr << " twice."
                  << std::endl;
        std::cout << std::endl;
        std::cout << "DOUBLE-FREE DETAILS:" << std::endl;
        std::cout << "  " << r.free_info << ": first freed here" << std::endl;
        std::cout << "  " << last_delete << ": freed again here" << std::endl;
        std::cout << "  " << r.info << ": allocated here" << std::endl;
        print_footer();

        // _Exit must be used because exit(1) still performs cleanup and the
        // memory leak statements also print. We want to preserve the "abort
        // on double-free" behavior from normal use of delete.
        _Exit(1);
      }
    }

    return true;
  }

  void remove_freed(void *ptr) {
    if (lock) {
      return;
    }
    lock = true;

    for (size_t i = 0; i < allocated_records.size(); ++i) {
      if (allocated_records.at(i).ptr == ptr) {
        // track this record as freed for double-free warnings
        allocated_records.at(i).free_info = last_delete;
        freed_records.push_back(allocated_records.at(i));

        alloced -= allocated_records.at(i).size;
        allocated_records.erase(allocated_records.begin() + i);
        break;
      }
    }

    lock = false;
  }

  void check_address_reuse(void *ptr) {
    for (size_t i = 0; i < freed_records.size(); ++i) {
      if (freed_records.at(i).ptr == ptr) {
        /* std::cout << "Removing already used pointer " << ptr << std::endl; */
        freed_records.erase(freed_records.begin() + i);
        break;
      }
    }
  }

  void add_record(size_t size, void *ptr) {
    if (lock) {
      return;
    }
    lock = true;

    check_address_reuse(ptr);
    allocated_records.push_back(Record(size, ptr));
    alloced += size;

    lock = false;
  }

  void extend_record_info(const AllocInfo &info, void *ptr) {
    for (size_t i = 0; i < allocated_records.size(); ++i) {
      if (allocated_records.at(i).ptr == ptr) {
        allocated_records.at(i).info = info;
        allocated_records.at(i).complete = true;
      }
    }
  }

  void track_delete(AllocInfo info) {
    last_delete = info;
  }
};

static MemTrack tracker;

// ptr has already been allocated and is in the tracker
// this function exists to attach additional info to the record
void SetAllocInfo(const AllocInfo &info, void *ptr) {
  tracker.extend_record_info(info, ptr);
}

void track_delete(std::string filename, std::string function, int line) {
  tracker.track_delete(AllocInfo(filename, function, line));
}

void *operator new(size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    throw std::bad_alloc();
  }
  tracker.add_record(size, ptr);
  return ptr;
}

void *operator new[](size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    throw std::bad_alloc();
  }
  tracker.add_record(size, ptr);
  return ptr;
}

void operator delete(void *ptr) noexcept {
  tracker.check_double_free(ptr);
  tracker.remove_freed(ptr);
}

void operator delete[](void *ptr) noexcept {
  tracker.check_double_free(ptr);
  tracker.remove_freed(ptr);
}
