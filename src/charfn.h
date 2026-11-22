// -*-C++-*-
#ifndef _charfn_h_
#define _charfn_h_

// The functions that read the value of a character (S4 piece R9).
//
// Each fn_* function has the value switch XYZZY_CHAR_UNICODE:
//   0: the value of a Char is the old internal code (the current behaviour);
//   1: the value of a Char is a Unicode code point, a raw byte (U+110080..
//      U+1100FF) or an isolated surrogate; the answer comes from uprops
//      (src/uprops.h) and the converters (src/codec.h).
// The unicode_* functions are the 1 side. They take a code point and do not
// depend on the switch (the tests in unittest/char-fn-tests.l reach them
// through si:unicode-*).
// docs/refactoring/s4-char-design.md 8.1.  A key is never given to them.

#include "uprops.h"
#include "codec.h"

// The raw byte characters (codec.h) are not Unicode text.
inline bool
char_raw_byte_p (u_int32_t c)
{
  return c >= CODEC_RAW_BYTE_MIN && c <= CODEC_RAW_BYTE_MAX;
}

// A half width katakana.
inline int
unicode_kana_char_p (u_int32_t c)
{
  return c >= 0xff61 && c <= 0xff9f;
}

// A character of the width 2: East Asian Wide and Fullwidth (Ambiguous
// follows *east-asian-ambiguous-width*).  A control character and a raw byte
// are not kanji.
inline int
unicode_kanji_char_p (u_int32_t c)
{
  return !char_raw_byte_p (c) && uc_width (c) == 2;
}

// The columns of a character (char-columns): 2 for a control character
// (C0, DEL and C1, as they are written ^X), 4 for a raw byte (\xNN), the
// East Asian Width for the rest (Wide and Fullwidth 2, Ambiguous by
// *east-asian-ambiguous-width*, otherwise 1).  A character that has no width
// of its own (combining marks, format characters) takes 1 column, as every
// character that is not a control does today.
inline int
unicode_char_columns (u_int32_t c)
{
  if (char_raw_byte_p (c))
    return 4;
  if (c < 0x20 || (c >= 0x7f && c < 0xa0))
    return 2;
  int w = uc_width (c);
  return w < 1 ? 1 : w;
}

// graphic-char-p: not a control, surrogate or unassigned code point
// (general category Cc, Cs, Cn).  A raw byte is not graphic.
inline int
unicode_graphic_char_p (u_int32_t c)
{
  if (char_raw_byte_p (c))
    return 0;
  int cat = uc_category (c);
  return cat != UC_Cc && cat != UC_Cs && cat != UC_Cn;
}

inline int
fn_kana_char_p (Char c)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    return unicode_kana_char_p (c);
  else
    return kana_char_p (c);
}

inline int
fn_kanji_char_p (Char c)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    return unicode_kanji_char_p (c);
  else
    return kanji_char_p (c);
}

inline int
fn_char_columns (Char c)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    return unicode_char_columns (c);
  else
    return char_width (c);
}

inline int
fn_graphic_char_p (Char c)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    return unicode_graphic_char_p (c);
  else
    {
      if ((c >= ' ' && c < CC_DEL) || kana_char_p (c))
        return 1;
      if (c < 0x100)
        return 0;
      if (code_charset_bit (c) & (ccsf_utf16_undef_char | ccsf_utf16_surrogate))
        return 0;
      return i2w (c) != UCS2_NONE;
    }
}

// The charset code of a Unicode code point (iso-char-code).  CCS is one of
// ccs_usascii, ccs_jisx0201_kana, ccs_jisx0208, ccs_jisx0212, ccs_gb2312,
// ccs_ksc5601, ccs_big5 (the code is the two bytes of Big5) and
// ccs_iso8859_*.  VENDER is ENCODING_ISO_VENDER_* (resolved).  It returns the
// code of the charset (as iso-char-code does) or -1.
int unicode_to_charset_code (u_int32_t u, int ccs, int vender);
// The inverse (iso-code-char).  CCS may also be ccs_big5_2 (the code is the
// 94 x 94 form) and ccs_cns11643_1 and 2.  It returns the code point or -1.
int charset_code_to_unicode (int code, int ccs, int vender);
// The first charset that has the code point, of the charsets of ACCEPT
// (bits 1 << ccs; -1 is all), searched in the order of iso-char-code
// (design 8.1: us-ascii, jisx0201-kana, jisx0208, jisx0212, gb2312, ksc5601,
// big5, iso8859-n).  Returns the ccs or -1.  With CP932 the jisx0208 is every
// 2 byte character of cp932 (NEC row 13, IBM extension), without it only
// JIS X 0208.
int unicode_find_charset (u_int32_t u, int accept, bool cp932, int vender);

#endif // _charfn_h_
