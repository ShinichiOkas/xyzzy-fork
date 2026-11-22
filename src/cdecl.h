// -*-C++-*-
#ifndef _cdecl_h_
# define _cdecl_h_

# pragma warning (disable: 4201)

# include <stdio.h>
# include <stdint.h>
# include <limits.h>
# include <windows.h>
# include <winreg.h>
# include <commctrl.h>
# include <stdlib.h>
# include <stddef.h>
# include <string.h>
# include <mbstring.h>
# include <malloc.h>

# pragma warning (default: 4201)

# pragma warning (disable: 4510)
# pragma warning (disable: 4514)
# pragma warning (disable: 4610)

# define alloca _alloca
# define memicmp _memicmp
# define strdup _strdup
# define stricmp _stricmp

# define BITS_PER_SHORT (sizeof (short) * CHAR_BIT)
# define BITS_PER_INT (sizeof (int) * CHAR_BIT)
# define BITS_PER_LONG (sizeof (long) * CHAR_BIT)

# define PATH_MAX 1024
# define BUFFER_NAME_MAX PATH_MAX

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef u_char u_int8_t;
typedef u_short u_int16_t;
typedef u_long u_int32_t;

typedef uintptr_t pointer_t;

// The value switch of S4 (docs/refactoring/s4-char-design.md 10.1, 10.5).
// 0: a Char holds the old internal code (the current behaviour).
// 1: a Char holds a Unicode code point (turned on by the V step).
// Write `if constexpr (XYZZY_CHAR_UNICODE) ... else ...` so that both sides
// are compiled every time.  Where #if cannot be avoided, check that the tree
// also builds with `set CL=/DXYZZY_CHAR_UNICODE=1`.
#ifndef XYZZY_CHAR_UNICODE
# define XYZZY_CHAR_UNICODE 1
#endif

typedef u_int32_t Char;
# if XYZZY_CHAR_UNICODE
#  define CHAR_LIMIT 0x110000
# else
#  define CHAR_LIMIT 0x10000
# endif
typedef u_long lChar;
const lChar lChar_EOF = lChar (-1);

// "No character" (no mapping, not found...).  Never compare a Char with
// Char (-1) or 0xffff for this.
const Char CHAR_NONE = Char (-1);

typedef u_int16_t ucs2_t;
typedef u_int32_t ucs4_t;

// "No UTF-16 code unit" in the tables of ucs2_t (internal2wc_table...).
const ucs2_t UCS2_NONE = ucs2_t (-1);

// A character value kept in a static 16bit table (the tables of ucs2tab.h).
// 0xffff in the table means "none".  Widen it with char16_to_char when reading.
typedef u_int16_t Char16;
const Char16 CHAR16_NONE = 0xffff;

inline Char
char16_to_char (Char16 c)
{
  return c == CHAR16_NONE ? CHAR_NONE : Char (c);
}

typedef long point_t;

# undef min
# undef max
# define NOMINMAX

template <class T>
inline const T &
min (const T &a, const T &b)
{
  return a < b ? a : b;
}

template <class T>
inline const T &
max (const T &a, const T &b)
{
  return a > b ? a : b;
}

inline char min (char a, char b) {return a < b ? a : b;}
inline char max (char a, char b) {return a > b ? a : b;}
inline u_char min (u_char a, u_char b) {return a < b ? a : b;}
inline u_char max (u_char a, u_char b) {return a > b ? a : b;}
inline short min (short a, short b) {return a < b ? a : b;}
inline short max (short a, short b) {return a > b ? a : b;}
inline u_short min (u_short a, u_short b) {return a < b ? a : b;}
inline u_short max (u_short a, u_short b) {return a > b ? a : b;}
inline int min (int a, int b) {return a < b ? a : b;}
inline int max (int a, int b) {return a > b ? a : b;}
inline u_int min (u_int a, u_int b) {return a < b ? a : b;}
inline u_int max (u_int a, u_int b) {return a > b ? a : b;}
inline long min (long a, long b) {return a < b ? a : b;}
inline long max (long a, long b) {return a > b ? a : b;}
inline u_long min (u_long a, u_long b) {return a < b ? a : b;}
inline u_long max (u_long a, u_long b) {return a > b ? a : b;}
inline float min (float a, float b) {return a < b ? a : b;}
inline float max (float a, float b) {return a > b ? a : b;}
inline double min (double a, double b) {return a < b ? a : b;}
inline double max (double a, double b) {return a > b ? a : b;}

template <class T>
inline void
swap (T &a, T &b)
{
  T t = a;
  a = b;
  b = t;
}

inline int
bcmp (const void *p1, const void *p2, size_t size)
{
  return memcmp (p1, p2, size);
}

inline void *
bzero (void *dst, size_t size)
{
  return memset (dst, 0, size);
}

inline void
bcopy (const Char *src, Char *dst, size_t size)
{
  memcpy (dst, src, sizeof (Char) * size);
}

inline int
bcmp (const Char *p1, const Char *p2, size_t size)
{
  return memcmp (p1, p2, sizeof (Char) * size);
}

template <class T>
inline T *
bfill (T *p0, int start, int end, T x)
{
  for (T *p = p0 + start, *pe = p0 + end; p < pe; p++)
    *p = x;
  return p0;
}

template <class T>
inline T *
bfill (T *p0, int size, T x)
{
  return bfill (p0, 0, size, x);
}

# define numberof(a) (sizeof (a) / sizeof *(a))

# ifdef DEBUG
int assert_failed (const char *, int);
#  define assert(f) \
  ((void)((f) || assert_failed (__FILE__, __LINE__)))
# else
#  define assert(f) /* empty */
# endif

# ifdef DEBUG
#  define DBG_PRINT(a) (printf a, fflush (stdout))
# else
#  define DBG_PRINT(a) /* empty */
# endif

# define __CONCAT(X, Y) X ## Y
# define CONCAT(X, Y) __CONCAT (X, Y)

# define __CONCAT3(X, Y, Z) X ## Y ## Z
# define CONCAT3(X, Y, Z) __CONCAT3 (X, Y, Z)

# define __TOSTR(X) #X
# define _TOSTR(X) __TOSTR(X)
# define __TOWSTR(X) L##X
# define _TOWSTR(X) __TOWSTR (X)

# define THREADLOCAL __declspec (thread)

#endif
