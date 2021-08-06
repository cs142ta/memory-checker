#ifndef _MEMORY_H
#define _MEMORY_H

// Include all libraries here that a student would reasonably use.
// otherwise the macros below would be expanded in the definitions
// of new and delete that are included as dependencies of the
// libraries below.
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// pass -D _MEMCHECK_ALL_INCLUDES to the commandline flags to enable
// these extra includes. In theory they should not be required for
// cs142 use, but this define makes it easy to enable if needed.
// Making these optional by default shortens compile time by a
// second on a fast machine.
#ifdef _MEMCHECK_ALL_INCLUDES
#include <array>
#include <bitset>
#include <chrono>
#include <codecvt>
#include <complex>
#include <deque>
#include <exception>
#include <forward_list>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <numeric>
#include <queue>
#include <random>
#include <ratio>
#include <regex>
#include <set>
#include <stack>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <valarray>
#endif

#include <stdio.h>

struct SourceLocation {
  const char *file = "";
  const char *function = "";
  int line = 0;

  SourceLocation() {}

  SourceLocation(const char *filename, const char *function, int line) {
    file = filename;
    this->function = function;
    this->line = line;
  }

  void print(const char *details) {
    printf("  %s:%d in \"%s\": %s\n", file, line, function, details);
  }
};

// The function to attach SourceLocation to a allocated pointer is defined in
// memory.cpp so it can access the memory tracker.
void SetSourceLocation(const SourceLocation &info, void *ptr);

// Takes an allocated pointer (from the overridden operator new in memory.cpp)
// and passes the current file and line information to the memory tracker.
// This allows extending the information with more context to where the
// allocation occurred.
template <class T> inline T *operator*(const SourceLocation &info, T *ptr) {
  SetSourceLocation(info, ptr);
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
#define new SourceLocation(__FILE__, __PRETTY_FUNCTION__, __LINE__) * new

void track_delete(const char *filename, const char *function, int line);

// Here we call a function to track the line that delete was called on before
// we actually delete. This is used to warn about offending lines for double-
// free errors. We cannot use the same operator* trick from above because
// delete returns void. This means we can safely call a function in a
// separate statement before running the delete operator.
#define delete                                                                 \
  track_delete(__FILE__, __PRETTY_FUNCTION__, __LINE__);                       \
  delete

#endif
