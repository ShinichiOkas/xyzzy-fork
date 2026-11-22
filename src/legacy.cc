// The old internal code -> Unicode, for the data files that xyzzy wrote before
// the move to Unicode (docs/refactoring/s4-char-design.md 9.4, S4 piece R5).
//
//   (si:legacy-internal-to-unicode codes) => list
//
// codes is a list (or a vector) of the old internal codes (0..#xffff): the
// values of \x, \X and #\x in the history, the session, the keyboard macro
// file and the abbrev file of an old xyzzy.  The result is the values of
// today, in order:
//   - a key of the old layout (#x8000-#x807f meta + ASCII, #xfe00-#xffff
//     function keys and the rest) is the key of today, KEY_BASE + the old
//     value (S4-k, keycode.h);
//   - the two halves of a pair (a UTF-16 surrogate pair, and the pair of an
//     undefined UTF-16 character, #xd6xx #xd7xx) are one code point;
//   - every other value is the Unicode of the old table (i2w), and U+FFFD if
//     the table has none.
// The table is the table of the old internal code (internal2wc_table); it
// does not depend on XYZZY_CHAR_UNICODE, so the tests reach it on both sides.
#include "stdafx.h"
#include "ed.h"
#include "keycode.h"
#include <vector>

static const u_int32_t LEGACY_NO_CHAR = 0xfffd;

static bool
legacy_key_p (u_int32_t c)
{
  return (c >= 0x8000 && c <= 0x807f) || (c >= 0xfe00 && c <= 0xffff);
}

// The value of today for the old value(s) at P.  It returns the number of the
// old values that it used (1 or 2).
static int
legacy_to_unicode (const u_int32_t *p, const u_int32_t *pe, u_int32_t &u)
{
  u_int32_t c = *p;
  if (c >= 0x10000)
    {
      u = LEGACY_NO_CHAR;
      return 1;
    }
  if (legacy_key_p (c))
    {
      u = KEY_BASE | c;
      return 1;
    }
  if (c < 0x80)
    {
      u = c;
      return 1;
    }
  if (p + 1 < pe && p[1] < 0x10000)
    {
      if (utf16_surrogate_high_p (ucs2_t (c))
          && utf16_surrogate_low_p (ucs2_t (p[1])))
        {
          u = utf16_pair_to_ucs4 (ucs2_t (c), ucs2_t (p[1]));
          return 2;
        }
      if (utf16_undef_char_high_p (ucs2_t (c))
          && utf16_undef_char_low_p (ucs2_t (p[1])))
        {
          u = utf16_undef_pair_to_ucs2 (ucs2_t (c), ucs2_t (p[1]));
          return 2;
        }
    }
  ucs2_t wc = i2w (Char (c));
  u = wc == ucs2_t (-1) ? LEGACY_NO_CHAR : wc;
  return 1;
}

lisp
Fsi_legacy_internal_to_unicode (lisp codes)
{
  std::vector <u_int32_t> in;
  if (simple_vector_p (codes))
    {
      for (int i = 0; i < xvector_length (codes); i++)
        in.push_back (u_int32_t (fixnum_value (xvector_contents (codes)[i])));
    }
  else if (listp (codes))
    {
      for (lisp p = codes; consp (p); p = xcdr (p))
        {
          QUIT;
          in.push_back (u_int32_t (fixnum_value (xcar (p))));
        }
    }
  else
    FEtype_error (codes, Qsequence);

  std::vector <u_int32_t> out;
  const u_int32_t *p = in.empty () ? 0 : &in[0], *pe = p + in.size ();
  while (p < pe)
    {
      u_int32_t u;
      p += legacy_to_unicode (p, pe, u);
      out.push_back (u);
    }

  lisp r = Qnil;
  for (size_t i = out.size (); i > 0; i--)
    r = xcons (make_fixnum (long (out[i - 1])), r);
  return r;
}
