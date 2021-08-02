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
files, and including "memory.cpp -include memory.h" as the arguments,
zyBooks will use the memory checker for all autograded tests.
"-include memory.h" will add `#include "memory.h"` at the top of each
source file.
