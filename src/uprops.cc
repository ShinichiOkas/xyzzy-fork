#include "stdafx.h"
#include "ed.h"
#include "package.h"
#include "uprops.h"
#include "uprops-data.h"

#define UC_MAX 0x10FFFF

/* The flag byte of the FLAGS table (gen-uprops.cc agrees):
     bit 0-1  width class: 0 zero, 1 narrow, 2 wide, 3 ambiguous
     bit 2-5  font group
     bit 6    word character
     bit 7    graphic */
#define WC_ZERO 0
#define WC_NARROW 1
#define WC_WIDE 2
#define WC_AMBIGUOUS 3

static const u_char out_of_range_flags = WC_NARROW | (UC_FONT_FALLBACK << 2);

static inline u_int
lookup (const u_int16_t *top, const u_char *pages, int shift, u_int32_t c)
{
  return pages[(u_int (top[c >> shift]) << shift) + (c & ((1 << shift) - 1))];
}

static inline u_int
lookup16 (const u_int16_t *top, const u_int16_t *pages, int shift, u_int32_t c)
{
  return pages[(u_int (top[c >> shift]) << shift) + (c & ((1 << shift) - 1))];
}

static inline u_int
flags_of (u_int32_t c)
{
  if (c > UC_MAX)
    return out_of_range_flags;
  return lookup (up_FLAGS_top, up_FLAGS_pages, UP_FLAGS_SHIFT, c);
}

static inline u_int
gcea_of (u_int32_t c)
{
  if (c > UC_MAX)
    return UC_Cn | (UC_EA_N << 5);
  return lookup (up_GCEA_top, up_GCEA_pages, UP_GCEA_SHIFT, c);
}

static inline const up_case_rec &
case_of (u_int32_t c)
{
  if (c > UC_MAX)
    return up_case_recs[0];
  return up_case_recs[lookup16 (up_CASE_top, up_CASE_pages, UP_CASE_SHIFT, c)];
}

static const char *const category_names[UC_CATEGORY_MAX] =
{
  "Lu", "Ll", "Lt", "Lm", "Lo",
  "Mn", "Mc", "Me",
  "Nd", "Nl", "No",
  "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po",
  "Sm", "Sc", "Sk", "So",
  "Zs", "Zl", "Zp",
  "Cc", "Cf", "Cs", "Co", "Cn",
};

static const char *const east_asian_width_names[] =
  {"N", "A", "F", "H", "Na", "W"};

int
uc_category (u_int32_t c)
{
  return gcea_of (c) & 31;
}

const char *
uc_category_name (int n)
{
  return n >= 0 && n < UC_CATEGORY_MAX ? category_names[n] : 0;
}

int
uc_east_asian_width (u_int32_t c)
{
  return gcea_of (c) >> 5;
}

const char *
uc_east_asian_width_name (int n)
{
  return n >= 0 && n < numberof (east_asian_width_names)
    ? east_asian_width_names[n] : 0;
}

int
uc_script (u_int32_t c)
{
  if (c > UC_MAX)
    return 0;
  return lookup (up_SCRIPT_top, up_SCRIPT_pages, UP_SCRIPT_SHIFT, c);
}

const char *
uc_script_name (int n)
{
  return n >= 0 && n < UP_N_SCRIPTS ? up_script_names[n] : 0;
}

int
uc_width (u_int32_t c, int ambiguous)
{
  switch (flags_of (c) & 3)
    {
    case WC_ZERO:
      return 0;
    case WC_WIDE:
      return 2;
    case WC_AMBIGUOUS:
      return ambiguous == 1 ? 1 : 2;
    default:
      return 1;
    }
}

int
uc_width (u_int32_t c)
{
  return uc_width (c, uc_ambiguous_width ());
}

int
uc_ambiguous_width ()
{
  lisp x = xsymbol_value (Veast_asian_ambiguous_width);
  return x == make_fixnum (1) ? 1 : 2;
}

int
uc_font_group (u_int32_t c, int ambiguous)
{
  u_int f = flags_of (c);
  if ((f & 3) == WC_AMBIGUOUS && ambiguous != 1)
    return UC_FONT_HAN;
  return (f >> 2) & 15;
}

int
uc_font_group (u_int32_t c)
{
  return uc_font_group (c, uc_ambiguous_width ());
}

int
uc_word_char_p (u_int32_t c)
{
  return (flags_of (c) >> 6) & 1;
}

int
uc_graphic_p (u_int32_t c)
{
  return (flags_of (c) >> 7) & 1;
}

u_int32_t
uc_toupper (u_int32_t c)
{
  return c + case_of (c).upper;
}

u_int32_t
uc_tolower (u_int32_t c)
{
  return c + case_of (c).lower;
}

u_int32_t
uc_totitle (u_int32_t c)
{
  return c + case_of (c).title;
}

u_int32_t
uc_casefold (u_int32_t c)
{
  return c + case_of (c).fold;
}

const char *
uc_unicode_version ()
{
  return UPROPS_UNICODE_VERSION;
}

int
uc_table_bytes ()
{
  return UP_TABLE_BYTES;
}

/* Lisp interface for tests and for the display code to try.  A character is
   given as an integer code point (0 .. #x10FFFF), not as a Lisp character,
   because the Lisp characters do not hold Unicode code points yet. */

static u_int32_t
code_point (lisp c)
{
  long n = fixnum_value (c);
  if (n < 0 || n > UC_MAX)
    FErange_error (c);
  return u_int32_t (n);
}

static int
ambiguous_value (lisp x)
{
  if (!x || x == Qnil)
    return uc_ambiguous_width ();
  long n = fixnum_value (x);
  if (n != 1 && n != 2)
    FErange_error (x);
  return int (n);
}

lisp
Fsi_uprops_category (lisp c)
{
  return make_string (uc_category_name (uc_category (code_point (c))));
}

lisp
Fsi_uprops_east_asian_width (lisp c)
{
  return make_string (uc_east_asian_width_name (uc_east_asian_width (code_point (c))));
}

lisp
Fsi_uprops_script (lisp c)
{
  return make_string (uc_script_name (uc_script (code_point (c))));
}

lisp
Fsi_uprops_width (lisp c, lisp ambiguous)
{
  return make_fixnum (uc_width (code_point (c), ambiguous_value (ambiguous)));
}

lisp
Fsi_uprops_font_group (lisp c, lisp ambiguous)
{
  static const char *const names[UC_FONT_MAX] =
    {
      "ascii", "jp", "han", "hangul", "greek", "cyrillic", "georgian",
      "latin", "fallback",
    };
  int g = uc_font_group (code_point (c), ambiguous_value (ambiguous));
  return Fintern (make_string (names[g]), xsymbol_value (Vkeyword_package));
}

lisp
Fsi_uprops_word_char_p (lisp c)
{
  return boole (uc_word_char_p (code_point (c)));
}

lisp
Fsi_uprops_graphic_p (lisp c)
{
  return boole (uc_graphic_p (code_point (c)));
}

lisp
Fsi_uprops_upcase (lisp c)
{
  return make_fixnum (uc_toupper (code_point (c)));
}

lisp
Fsi_uprops_downcase (lisp c)
{
  return make_fixnum (uc_tolower (code_point (c)));
}

lisp
Fsi_uprops_titlecase (lisp c)
{
  return make_fixnum (uc_totitle (code_point (c)));
}

lisp
Fsi_uprops_casefold (lisp c)
{
  return make_fixnum (uc_casefold (code_point (c)));
}

lisp
Fsi_uprops_version ()
{
  return make_string (uc_unicode_version ());
}

lisp
Fsi_uprops_table_size ()
{
  return make_fixnum (uc_table_bytes ());
}
