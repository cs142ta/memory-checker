#ifndef _MEMORY_H
#define _MEMORY_H

#include <iostream>
#include <sstream>
#include <string>

struct AllocInfo {
  std::string file = "";
  std::string function = "";
  int line = 0;

  AllocInfo() {}

  AllocInfo(std::string filename, std::string function, int line)
      : file(filename), function(function), line(line) {}

  friend std::ostream &operator<<(std::ostream &out, const AllocInfo &info) {
    out << info.file << ":" << info.line << " in \"" << info.function << "\"";
    return out;
  }
};

void SetAllocInfo(const AllocInfo &info, void *ptr);

template <class T> inline T *operator*(const AllocInfo &info, T *ptr) {
  SetAllocInfo(info, ptr);
  return ptr;
}

// This macro is the magic behind tracking the filename, function, and line
// number for each allocation. New runs as per usual, and then the resulting
// allocated pointer is multiplied with an AllocInfo object which has been
// constructed with the current line info.
// Because new returns a value, it is necessary to use this operator
// overloading method to track the information on the current line. The
// operator* function returns the pointer returned from new which allows this
// to work anywhere new is allowed.
#define new AllocInfo(__FILE__, __PRETTY_FUNCTION__, __LINE__) * new

void track_delete(std::string filename, std::string function, int line);

// Here we call a function to track the line that delete was called on before
// we actually delete. This is used to warn about offending lines for double-
// free errors. We cannot use the same operator* trick from above because
// delete returns void. This means we can safely call a function in a
// separate statement before running the delete operator.
#define delete                                                                 \
  track_delete(__FILE__, __PRETTY_FUNCTION__, __LINE__);                       \
  delete

#endif
