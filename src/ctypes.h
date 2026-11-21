// -*-C++-*-
#ifndef _ctypes_h_
# define _ctypes_h_

// Fixed sizes and alignments of the C types that the FFI maps
// (the table *c-type-table* in lisp/foreign.l).  x64 Windows (LLP64).
//
// The same values are written in two other places; keep the three in step:
//   lisp/foreign.l              *c-type-table* (the Lisp side, by base type)
//   unittest/foreign-tests.l    c-type-table-matches-c (compares the Lisp
//                               table with the values measured here)
// Changing a type in the Lisp table without changing the values here (or the
// reverse) fails the test; changing the C++ type fails this build.

# include <stddef.h>
# include <stdint.h>
# include <time.h>
# include <sys/types.h>

# define CTYPE_CHECK(type, size, align) \
  static_assert (sizeof (type) == (size) && alignof (type) == (align), \
                 #type " does not have the width the FFI table assumes")

CTYPE_CHECK (void *, 8, 8);
CTYPE_CHECK (char *, 8, 8);
CTYPE_CHECK (char, 1, 1);
CTYPE_CHECK (unsigned char, 1, 1);
CTYPE_CHECK (short, 2, 2);
CTYPE_CHECK (unsigned short, 2, 2);
CTYPE_CHECK (wchar_t, 2, 2);
CTYPE_CHECK (int, 4, 4);
CTYPE_CHECK (unsigned int, 4, 4);
CTYPE_CHECK (long, 4, 4);
CTYPE_CHECK (unsigned long, 4, 4);
CTYPE_CHECK (int64_t, 8, 8);
CTYPE_CHECK (uint64_t, 8, 8);
CTYPE_CHECK (float, 4, 4);
CTYPE_CHECK (double, 8, 8);
CTYPE_CHECK (size_t, 8, 8);
CTYPE_CHECK (ptrdiff_t, 8, 8);
CTYPE_CHECK (time_t, 8, 8);
CTYPE_CHECK (clock_t, 4, 4);
CTYPE_CHECK (off_t, 4, 4);

# undef CTYPE_CHECK

#endif
