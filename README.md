# CS142 Simple Memory Checker

This is a simple memory checker for the CS142 C++ projects that involve
manual memory management with new and delete. It tracks allocations and
freed memory. This allows warnings for:
* memory leaks
* double-free errors

This is not intended to be as robust as a tool like Valgrind or ASAN.
Because the memory checker runs in the zyBooks sandboxed environment,
there are many limitations to what we can do.

## Goals

* Provide more context to students. CS142 students learn about pointers
and dynamic memory late in the semester, and it isn't reasonable to
expect perfect understanding from them. The output from this tool will
show the students what problems are in their code, and where to look to
fix the issues.
* Assist TAs in grading. By catching memory leaks as part of the
autograder the TA is freed from verifying memory-soundness by reading
the code.

## How it works

zyBooks allows passing commandline arguments and attaching additional
files for labs. By uploading memory.cpp and memory.h as additional
files, and including `memory.cpp -include memory.h` as the arguments,
zyBooks will use the memory checker for all autograded tests.
"-include memory.h" will add `#include "memory.h"` at the top of each
source file.

When the executable is run, all allocations and frees are tracked. If
a double-free is found, the program prints the locations of the new
and delete calls and terminates. If the program reaches completion
and has memory still allocated it prints a list of all memory that
was not freed.

At a high level here is how the code works:

A global memory tracker is created in memory.cpp to store all allocs
and frees. The global `operator new` and `operator delete` functions
are overridden. New stores each new pointer and associated size in a
list in `Record` objects, and `delete` moves the `Record` associated
with a pointer to the freed list.

Two macros are created, one for `new` and the other for `delete`. The
purpose of the macros is to capture each file, function, and line
number where `new` and `delete` are used.

The `new` macro takes the pointer returned by `new` and multiplies it
by an instance of `SourceLocation`. The `operator *` method then
takes that address, and passes the file location to the global memory
tracker. It is attached to the `Record` that was created with `new`.

The `delete` macro cannot use the same `operator *` trick because
delete returns void. So delete is replaced by a function call to
track the location of the delete, foloowed by the actual delete.

### Tradeoffs

#### Macros
Because we use macros to provide the context of the alloc or free,
we have to be careful with the order of includes. Some of the files
in the standard library includes define `operator new` and `operator
delete`. If the `#include memory.h` is included before these files,
there will be compilation errors from the expanded macro.

The best solution would be to include the file after all standard
library files. Unfortunately we don't want to require students to
write their files a certain way which is why we use the `-include`
flag. So the solution is to place all the standard library includes
inside memory.h so they are included before the macros are defined.

The default list of standard library files should be sufficient, but
if there are compilation errors, adding `D _MEMCHECK_ALL_INCLUDES`
to the compilation flags may help, and is an easy fix. It does
increase compilation times though.

#### Undefined behavior
One other issue observed is undefined behavior when freeing memory.
Ideally, the overridden `operator delete` should call `free()`, but
we observed very strange behavior with truncated addresses which
prevented the tracker code from working, and would cause segfaults.
So we wait until the end of the program to free all memory.

This isn't ideal, but because CS142 students are writing very small
programs this shouldn't cause issues. If a student runs out of
memory in their program due to calls to `delete` not freeing memory,
they have bigger issues in their lab.
