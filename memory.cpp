#include "memory.h"

// undefine new and delete macros from memory.h so defining new and delete
// below doesn't create conflicts with the text replacements.
#undef new
#undef delete

#include <iostream>
#include <string>
#include <vector>

// for malloc and free
#include <cstdlib>

// A Record stores information for each allocated block of memory.
struct Record {
  size_t size;
  void *ptr;
  // where was this allocated?
  FileInfo alloc_info;
  // where was this first freed?
  FileInfo free_info;

  Record(size_t size, void *ptr) : size(size), ptr(ptr) {}
};

// There is a global MemTrack object declared below (tracker)
// which is created at the beginning of the program's execution,
// and automaticlly destructed at the end of the program.
struct MemTrack {
  // total allocated bytes
  size_t alloced = 0;

  std::vector<Record> allocated_records;
  std::vector<Record> freed_records;

  // track the most recent delete's line info (part of the delete
  // macro in memory.h)
  FileInfo last_delete;

  // unfortunately, overriding the global new and delete operators means
  // that any calls to new or delete in the standard library will also
  // use the overridden operators.
  // That means uses of vectors in this class must be protected, otherwise
  // the uses of new and delete would create recursion where the allocation
  // would be tracked (add_record), which calls push_back, which may call
  // new. Any function that could potentially allocate should be guarded.
  bool lock = false;

  MemTrack() {}

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

  // this destructor is automatically run at the end of the program.
  ~MemTrack() {
    // all memory was freed
    if (alloced == 0) {
      print_header();
      std::cout << "no issues detected" << std::endl;
      print_footer();
      return;
    }

    // memory leaks found!
    print_header();
    std::cout << "LEAK SUMMARY:" << std::endl;
    std::cout << "  leaked: " << alloced << " bytes in "
              << allocated_records.size() << " allocations" << std::endl;
    std::cout << std::endl;
    std::cout << "LEAK DETAILS:" << std::endl;
    for (Record r : allocated_records) {
      std::cout << "  " << r.alloc_info;
      std::cout << ": " << r.size << " bytes allocated here were never freed."
                << std::endl;
    }
    print_footer();
  }

  // Check the list of all freed pointers to see if ptr has already been freed.
  // Because malloc() is smart, it will try to re-use addresses that have already
  // been freed. This means we actually don't call free() anywhere in this file!
  void check_double_free(void *ptr) {
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
        std::cout << "  " << r.alloc_info << ": allocated here" << std::endl;
        print_footer();

        // _Exit must be used because exit(1) still performs cleanup and the
        // memory leak statements also print. We want to preserve the "abort
        // on double-free" behavior from normal use of delete.
        _Exit(1);
      }
    }
  }

  // move a record from the list of allocations to the list of frees
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

  // TODO: see if this is still needed
  void check_address_reuse(void *ptr) {
    for (size_t i = 0; i < freed_records.size(); ++i) {
      if (freed_records.at(i).ptr == ptr) {
        /* std::cout << "Removing already used pointer " << ptr << std::endl; */
        freed_records.erase(freed_records.begin() + i);
        break;
      }
    }
  }

  // Add a record to the list of allocations
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

  // attaches the file & line information to a record.
  void extend_record_info(const FileInfo &info, void *ptr) {
    for (size_t i = 0; i < allocated_records.size(); ++i) {
      if (allocated_records.at(i).ptr == ptr) {
        allocated_records.at(i).alloc_info = info;
      }
    }
  }

  void track_delete(FileInfo info) {
    last_delete = info;
  }
};

static MemTrack tracker;

// ptr has already been allocated and is in the tracker
// this function exists to attach additional info to the record
void SetFileInfo(const FileInfo &info, void *ptr) {
  tracker.extend_record_info(info, ptr);
}

void track_delete(std::string filename, std::string function, int line) {
  tracker.track_delete(FileInfo(filename, function, line));
}

// The following override the global new and delete operators.
// It probably isn't needed in cs142 to override the [] variants,
// but it doesn't hurt either.
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
