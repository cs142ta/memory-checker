#include "memory.h"

// for memmove printf malloc and free
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Record {
  size_t size;
  void *ptr;
  // where was this allocated?
  SourceLocation alloc;
  // where was this first freed?
  SourceLocation free;

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
  size_t total_alloced = 0;

  // lists of allocated and freed records
  RecordList allocated;
  RecordList freed;

  // track the location of the most recent delete
  SourceLocation last_delete;

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
    printf("  leaked: %ld bytes in %ld allocations\n", alloced, total_alloced);
    printf("\n");
    printf("LEAK DETAILS:\n");
    for (size_t i = 0; i < allocated.size; i++) {
      SourceLocation location = allocated.get(i).alloc;
      printf("  %s:%d in \"%s\": ", location.file, location.line,
             location.function);
      printf("%ld bytes allocated with 'new' here were never freed with "
             "'delete'\n",
             allocated.get(i).size);
    }

    // free all remaining memory
    for (size_t i = 0; i < allocated.size; i++) {
      free(allocated.get(i).ptr);
    }
    for (size_t i = 0; i < freed.size; i++) {
      free(freed.get(i).ptr);
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
        printf("  attempted to free address %p with 'delete' twice.\n", ptr);
        printf("\n");
        printf("DOUBLE-FREE DETAILS:\n");
        r.alloc.print("allocated with 'new' here");
        r.free.print("first freed with 'delete' here");
        last_delete.print("freed again with 'delete' here");

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
        allocated.get(i).free = last_delete;
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
    total_alloced += size;
  }

  // attaches the file & line location to a record.
  void extend_record_location(const SourceLocation &location, void *ptr) {
    for (size_t i = 0; i < allocated.size; ++i) {
      if (allocated.get(i).ptr == ptr) {
        allocated.get(i).alloc = location;
      }
    }
  }

  void track_delete(SourceLocation location) { last_delete = location; }
};

static MemTrack tracker;

// ptr has already been allocated and is in the tracker
// this function exists to attach additional location to the record
void SetSourceLocation(const SourceLocation &location, void *ptr) {
  tracker.extend_record_location(location, ptr);
}

// Runs before operator delete (on the same line) and sets the last_delete
// of the tracker to the SourceLocation of that line. Otherwise there is no way
// to know where a delete occurred.
void track_delete(const char *filename, const char *function, int line) {
  tracker.track_delete(SourceLocation(filename, function, line));
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
  // This is where memory should be freed, but due to some undefined
  // behavior, we don't free until the end of the program.
  // See the readme for the rationale behind this decision.
  tracker.remove_freed(ptr);
}

void operator delete[](void *ptr) noexcept {
  tracker.check_double_free(ptr);
  tracker.remove_freed(ptr);
}
