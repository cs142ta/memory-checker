#include "memory.h"

// for memmove printf malloc and free
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

struct RecordList {
  Record *records;
  size_t size;
  size_t capacity;

  RecordList() {
    capacity = 4;
    records = (Record *)malloc(sizeof(Record) * capacity);
    size = 0;
  }

  ~RecordList() { free(records); }

  void add(Record r) {
    if (size >= capacity) {
      capacity *= 2;
      records = (Record *)realloc(records, sizeof(Record) * capacity);
    }
    records[size] = r;
    size++;
  }

  Record &get(size_t index) { return records[index]; }

  void remove(size_t index) {
    if (index < size - 1) {
      const size_t remaining = size - index - 1;
      memmove(&records[index], &records[index + 1], remaining * sizeof(Record));
    }
    size--;
  }
};

// There is a global MemTrack object declared below (tracker)
// which is created at the beginning of the program's execution,
// and automaticlly destructed at the end of the program.
struct MemTrack {
  // total allocated bytes
  size_t alloced = 0;

  // lists of allocated and freed records
  RecordList allocated;
  RecordList freed;

  // track the most recent delete's line info (part of the delete
  // macro in memory.h)
  FileInfo last_delete;

  MemTrack() {}

  void print_header() { printf("\n---------- memory checker ----------\n\n"); }

  // this destructor is automatically run at the end of the program.
  ~MemTrack() {
    // all memory was freed
    if (alloced == 0) {
      print_header();
      printf("no issues detected\n");
      return;
    }

    // memory leaks found!
    print_header();
    printf("LEAK SUMMARY:\n");
    printf("  leaked: %ld bytes in %d allocations\n", alloced, 12345);
    printf("\n");
    printf("LEAK DETAILS:\n");
    for (size_t i = 0; i < allocated.size; i++) {
      FileInfo info = allocated.get(i).alloc_info;
      printf("  %s:%d in \"%s\": ", info.file, info.line, info.function);
      printf("%ld bytes allocated here were never freed\n",
             allocated.get(i).size);
    }
  }

  // Check the list of all freed pointers to see if ptr has already been freed.
  void check_double_free(void *ptr) {
    for (size_t i = 0; i < freed.size; ++i) {
      if (freed.get(i).ptr == ptr) {
        // double free found
        Record r = freed.get(i);

        print_header();
        printf("DOUBLE-FREE SUMMARY:\n");
        printf("  attempted to free pointer %p twice.\n", ptr);
        printf("\n");
        printf("DOUBLE-FREE DETAILS:\n");
        r.free_info.print("first freed here");
        last_delete.print("freed again here");
        r.alloc_info.print("allocated here");

        // _Exit must be used because exit(1) still performs cleanup and the
        // memory leak statements also print. We want to preserve the "abort
        // on double-free" behavior from normal use of delete. */
        _Exit(1);
      }
    }
  }

  // move a record from the list of allocations to the list of frees
  void remove_freed(void *ptr) {
    for (size_t i = 0; i < allocated.size; ++i) {
      if (allocated.get(i).ptr == ptr) {
        // track this record as freed for double-free warnings
        allocated.get(i).free_info = last_delete;
        freed.add(allocated.get(i));

        alloced -= allocated.get(i).size;
        allocated.remove(i);
        break;
      }
    }
  }

  // TODO: see if this is still needed
  void check_address_reuse(void *ptr) {
    for (size_t i = 0; i < freed.size; ++i) {
      if (freed.get(i).ptr == ptr) {
        freed.remove(i);
        break;
      }
    }
  }

  // Add a record to the list of allocations
  void add_record(size_t size, void *ptr) {
    check_address_reuse(ptr);
    allocated.add(Record(size, ptr));
    alloced += size;
  }

  // attaches the file & line information to a record.
  void extend_record_info(const FileInfo &info, void *ptr) {
    for (size_t i = 0; i < allocated.size; ++i) {
      if (allocated.get(i).ptr == ptr) {
        allocated.get(i).alloc_info = info;
      }
    }
  }

  void track_delete(FileInfo info) { last_delete = info; }
};

static MemTrack tracker;

// ptr has already been allocated and is in the tracker
// this function exists to attach additional info to the record
void SetFileInfo(const FileInfo &info, void *ptr) {
  tracker.extend_record_info(info, ptr);
}

// Runs before operator delete (on the same line) and sets the last_delete
// of the tracker to the FileInfo of that line. Otherwise there is no way
// to know where a delete occurred.
void track_delete(const char *filename, const char *function, int line) {
  tracker.track_delete(FileInfo(filename, function, line));
}

// undefine new and delete macros from memory.h so defining new and delete
// below doesn't create conflicts with the text replacements.
#undef new
#undef delete

// The following override the global new and delete operators.
// It probably isn't needed in CS142 to override the [] variants,
// but it doesn't hurt either.
void *operator new(size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
  }
  tracker.add_record(size, ptr);
  return ptr;
}

void *operator new[](size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
  }
  tracker.add_record(size, ptr);
  return ptr;
}

void operator delete(void *ptr) noexcept {
  tracker.check_double_free(ptr);
  // why can't we free here?
  /* free(ptr); */
  tracker.remove_freed(ptr);
}

void operator delete[](void *ptr) noexcept {
  tracker.check_double_free(ptr);
  /* free(ptr); */
  tracker.remove_freed(ptr);
}
