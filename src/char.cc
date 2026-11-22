#include "stdafx.h"
#include "ed.h"
#include "charfn.h"
#include "codec-priv.h"

// The character functions take a key as well as a text character.  A key is
// not an alphabetic character and has no other case.  Comparisons are done by
// the value.
lisp
Fstandard_char_p (lisp cc)
{
  check_char (cc);
  u_int32_t c = xchar_value (cc);
  return boole (c == '\n' || (c >= ' ' && c < CC_DEL));
}

lisp
Fgraphic_char_p (lisp cc)
{
  check_char (cc);
  u_int32_t v = xchar_value (cc);
  if (v >= KEY_BASE)
    return Qnil;
  return boole (fn_graphic_char_p (Char (v)));
}

lisp
Falpha_char_p (lisp cc)
{
  check_char (cc);
  return boole (alpha_char_p (lChar (xchar_value (cc))));
}

lisp
Fupper_case_p (lisp cc)
{
  check_char (cc);
  return boole (upper_char_p (lChar (xchar_value (cc))));
}

lisp
Flower_case_p (lisp cc)
{
  check_char (cc);
  return boole (lower_char_p (lChar (xchar_value (cc))));
}

lisp
Fboth_case_p (lisp cc)
{
  check_char (cc);
  return boole (alpha_char_p (lChar (xchar_value (cc))));
}

lisp
Fdigit_char_p (lisp cc, lisp radix)
{
  check_char (cc);
  int r = (radix && radix != Qnil) ? fixnum_value (radix) : 10;
  int n = digit_char (int (xchar_value (cc)));
  return n < r ? make_fixnum (n) : Qnil;
}

lisp
Falphanumericp (lisp cc)
{
  check_char (cc);
  return boole (alphanumericp (lChar (xchar_value (cc))));
}

lisp
Fcharacter (lisp object)
{
  if (charp (object))
    return object;
  if (symbolp (object) && xstring_length (xsymbol_name (object)) == 1)
    return make_char (xstring_contents (xsymbol_name (object)) [0]);
  if (stringp (object) && xstring_length (object) == 1)
    return make_char (xstring_contents (object) [0]);
  return FEprogram_error (Ecannot_coerce_to_character, object);
}

lisp
Fchar_eql (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = xchar_value (first);
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      if (x != xchar_value (xcar (rest)))
        return Qnil;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_eql (lisp args)
{
  int nargs = 0;
  for (lisp p = args; consp (p); p = xcdr (p), nargs++)
    {
      check_char (xcar (p));
      QUIT;
    }

  switch (nargs)
    {
    case 0:
      return FEtoo_few_arguments ();

    case 1:
      return Qt;

    case 2:
      return boole (xchar_value (xcar (args)) != xchar_value (xcar (xcdr (args))));

    default:
      for (lisp p = args; consp (p); p = xcdr (p))
        for (lisp q = args; consp (q); q = xcdr (q))
          if (p != q && xchar_value (xcar (p)) == xchar_value (xcar (q)))
            return Qnil;
      return Qt;
    }
}

lisp
Fchar_less (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = xchar_value (first);
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = xchar_value (xcar (rest));
      if (x >= y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_greater (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = xchar_value (first);
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = xchar_value (xcar (rest));
      if (x <= y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_greater (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = xchar_value (first);
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = xchar_value (xcar (rest));
      if (x > y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_less (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = xchar_value (first);
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = xchar_value (xcar (rest));
      if (x < y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_equal (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = char_value_upcase (xchar_value (first));
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      if (x != char_value_upcase (xchar_value (xcar (rest))))
        return Qnil;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_equal (lisp args)
{
  int nargs = 0;
  for (lisp p = args; consp (p); p = xcdr (p), nargs++)
    {
      check_char (xcar (p));
      QUIT;
    }

  switch (nargs)
    {
    case 0:
      return FEtoo_few_arguments ();

    case 1:
      return Qt;

    case 2:
      return boole (char_value_upcase (xchar_value (xcar (args)))
                    != char_value_upcase (xchar_value (xcar (xcdr (args)))));

    default:
      for (lisp p = args; consp (p); p = xcdr (p))
        for (lisp q = args; consp (q); q = xcdr (q))
          if (p != q
              && (char_value_upcase (xchar_value (xcar (p)))
                  == char_value_upcase (xchar_value (xcar (q)))))
            return Qnil;
      return Qt;
    }
}

lisp
Fchar_lessp (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = char_value_upcase (xchar_value (first));
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = char_value_upcase (xchar_value (xcar (rest)));
      if (x >= y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_greaterp (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = char_value_upcase (xchar_value (first));
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = char_value_upcase (xchar_value (xcar (rest)));
      if (x <= y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_greaterp (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = char_value_upcase (xchar_value (first));
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = char_value_upcase (xchar_value (xcar (rest)));
      if (x > y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_not_lessp (lisp first, lisp rest)
{
  check_char (first);
  u_int32_t x = char_value_upcase (xchar_value (first));
  for (; consp (rest); rest = xcdr (rest))
    {
      check_char (xcar (rest));
      u_int32_t y = char_value_upcase (xchar_value (xcar (rest)));
      if (x < y)
        return Qnil;
      x = y;
      QUIT;
    }
  return Qt;
}

lisp
Fchar_code (lisp cc)
{
  check_char (cc);
  return make_fixnum (xchar_value (cc));
}

// nil if there is no such character (BUG-0041: it used to cut the code to
// 16bit).  A code outside long is a range-error, as before.
lisp
Fcode_char (lisp code)
{
  return code_char (fixnum_value (code));
}

lisp
Fchar_upcase (lisp cc)
{
  check_char (cc);
  if (key_char_p (cc))
    return cc;
  return make_char (char_upcase (xchar_code (cc)));
}

lisp
Fchar_downcase (lisp cc)
{
  check_char (cc);
  if (key_char_p (cc))
    return cc;
  return make_char (char_downcase (xchar_code (cc)));
}

lisp
Fdigit_char (lisp weight, lisp radix)
{
  int w = fixnum_value (weight);
  int r = (radix && radix != Qnil) ? fixnum_value (radix) : 10;
  return ((r >= 2 && r <= 36 && w >= 0 && w < r)
          ? make_char (upcase_digit_char[w]) : Qnil);
}

lisp
Fset_meta_bit (lisp cc, lisp f)
{
  check_char (cc);
  return make_key_char (keycode_set_meta (xkeycode (cc), f != Qnil));
}

lisp
Fdbc_first_byte_p (lisp x)
{
  int n = fixnum_value (x);
  return boole (n < 0x100 && SJISP (n));
}

lisp
Fdbc_second_byte_p (lisp x)
{
  int n = fixnum_value (x);
  return boole (n < 0x100 && SJIS2P (n));
}

lisp
Fkanji_char_p (lisp x)
{
  check_char (x);
  if (key_char_p (x))
    return Qnil;
  return boole (fn_kanji_char_p (xchar_code (x)));
}

lisp
Fkana_char_p (lisp x)
{
  check_char (x);
  if (key_char_p (x))
    return Qnil;
  return boole (fn_kana_char_p (xchar_code (x)));
}

lisp
Fchar_unicode (lisp cc)
{
  check_char (cc);
  if (key_char_p (cc))
    return Qnil;
  Char c = xchar_code (cc);
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      // The value is the code point.  A raw byte has none.
      if (char_raw_byte_p (c) || c >= UNICODE_CHAR_LIMIT)
        return Qnil;
      return make_fixnum (c);
    }
  else
    {
      ucs2_t wc = i2w (c);
      return wc != UCS2_NONE ? make_fixnum (wc) : Qnil;
    }
}

lisp
Funicode_char (lisp code)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      // The same as code-char, but only the Unicode range.  A code point
      // beyond the BMP is one character.
      long n = fixnum_value (code);
      if (n < 0 || n >= UNICODE_CHAR_LIMIT)
        return Qnil;
      return make_char (Char (n));
    }

  ucs4_t wc = fixnum_value (code);
  if (wc >= UNICODE_CHAR_LIMIT)
    return Qnil;

  Char b[2];
  if (wc < 0x10000)
    {
      Char cc;
      if (xsymbol_value (Vunicode_to_half_width) != Qnil
          || (cc = wc2cp932 (ucs2_t (wc))) == CHAR_NONE
          || ccs_1byte_94_charset_p (code_charset (cc)))
        cc = w2i (ucs2_t (wc));
      if (cc != CHAR_NONE)
        return make_char (cc);
      b[0] = utf16_ucs2_to_undef_pair_high (ucs2_t (wc));
      b[1] = utf16_ucs2_to_undef_pair_low (ucs2_t (wc));
    }
  else
    {
      b[0] = utf16_ucs4_to_pair_high (wc);
      b[1] = utf16_ucs4_to_pair_low (wc);
    }
  return make_string (b, 2);
}

// ---------------------------------------------------------------------------
// The charsets of a Unicode code point (design 8.1).  These work on code
// points and do not depend on the value of a Char.  iso-char-code and
// iso-code-char use them when XYZZY_CHAR_UNICODE is 1; si:unicode-iso-code
// and si:iso-code-unicode give them to Lisp whatever the switch is.

static const struct {int ccs; lisp *kwd;} iso_order[] =
{
  {ccs_usascii, &Kus_ascii},
  {ccs_jisx0201_kana, &Kjisx0201_kana},
  {ccs_jisx0208, &Kjisx0208},
  {ccs_jisx0212, &Kjisx0212},
  {ccs_gb2312, &Kgb2312},
  {ccs_ksc5601, &Kksc5601},
  {ccs_big5, &Kbig5},
  {ccs_iso8859_1, &Kiso8859_1},
  {ccs_iso8859_2, &Kiso8859_2},
  {ccs_iso8859_3, &Kiso8859_3},
  {ccs_iso8859_4, &Kiso8859_4},
  {ccs_iso8859_5, &Kiso8859_5},
  {ccs_iso8859_7, &Kiso8859_7},
  {ccs_iso8859_9, &Kiso8859_9},
  {ccs_iso8859_10, &Kiso8859_10},
  {ccs_iso8859_13, &Kiso8859_13},
};

static int
sb_id_of_ccs (int ccs)
{
  switch (ccs)
    {
    case ccs_iso8859_1: return CODEC_SB_ISO8859_1;
    case ccs_iso8859_2: return CODEC_SB_ISO8859_2;
    case ccs_iso8859_3: return CODEC_SB_ISO8859_3;
    case ccs_iso8859_4: return CODEC_SB_ISO8859_4;
    case ccs_iso8859_5: return CODEC_SB_ISO8859_5;
    case ccs_iso8859_7: return CODEC_SB_ISO8859_7;
    case ccs_iso8859_9: return CODEC_SB_ISO8859_9;
    case ccs_iso8859_10: return CODEC_SB_ISO8859_10;
    case ccs_iso8859_13: return CODEC_SB_ISO8859_13;
    }
  return -1;
}

static int
big5_from_unicode (u_int32_t u)
{
  int i = codec_rev_get (codec_big5_rev, u);
  if (i < 0)
    return -1;
  int h = i / CODEC_BIG5_TRAILS + 0xa1;
  int ti = i % CODEC_BIG5_TRAILS;
  int t = ti < 63 ? ti + 0x40 : ti - 63 + 0xa1;
  return (h << 8) | t;
}

static int
big5_to_unicode (int h, int t)
{
  if (h < 0xa1 || h > 0xf9)
    return -1;
  int ti;
  if (t >= 0x40 && t <= 0x7e)
    ti = t - 0x40;
  else if (t >= 0xa1 && t <= 0xfe)
    ti = t - 0xa1 + 63;
  else
    return -1;
  codec_u16 u = codec_big5_fwd[(h - 0xa1) * CODEC_BIG5_TRAILS + ti];
  return u == 0xffff ? -1 : int (u);
}

static int
fwd94 (const codec_u16 *fwd, int c1, int c2)
{
  codec_u16 u = fwd[(c1 - 0x21) * 94 + c2 - 0x21];
  return u == 0xffff ? -1 : int (u);
}

// The code point of a JIS X 0208 or JIS X 0212 code (c1, c2: 0x21..0x7e).
static int
jis_to_unicode (int ccs, int c1, int c2, int vender)
{
  if (ccs == ccs_jisx0208)
    {
      if (vender == ENCODING_ISO_VENDER_OSFJVC && c1 >= 0x75)
        c1 += 10;
      int s = codec_jis2sjis (c1, c2);
      return codec_cp932_get (s >> 8, s & 255);
    }
  if (vender == ENCODING_ISO_VENDER_OSFJVC && c1 >= 0x75)
    {
      int s = codec_jis2sjis (c1 + 20, c2);
      return codec_cp932_get (s >> 8, s & 255);
    }
  return fwd94 (codec_jisx0212_fwd, c1, c2);
}

// The JIS X 0208 (or JIS X 0212) code of a code point, by the vender (the
// same rule as the ISO-2022 encoder of the codec layer).
static int
find_jis (u_int32_t u, int vender, int &c1, int &c2)
{
  int s = codec_cp932_code2 (u);
  if (s < 0)
    return -1;
  int h = s >> 8;
  if (h >= 0xfa && h <= 0xfc)
    {
      if (vender == ENCODING_ISO_VENDER_IBMEXT
          || vender == ENCODING_ISO_VENDER_OSFJVC)
        {
          int t = s & 255;
          int i = (h - 0xfa) * 188 + (t < 0x7f ? t - 0x40 : t - 0x41);
          if (i < 0 || i >= 388)
            return -1;
          u_int euc = codec_ibm_euc[i];
          c1 = (euc >> 8) & 0x7f;
          c2 = euc & 0x7f;
          int r = euc >= 0x8f0000 ? ccs_jisx0212 : ccs_jisx0208;
          if (jis_to_unicode (r, c1, c2, vender) == int (u))
            return r;
          if (vender == ENCODING_ISO_VENDER_OSFJVC)
            return -1;
        }
      // NEC extension (and the default): the NEC selected IBM extension.
      s = codec_rev_get (codec_cp932_ned_rev, u);
      if (s < 0)
        return -1;
    }
  else if (h >= 0xf0 && h <= 0xf9)
    {
      // user defined area
      if (vender != ENCODING_ISO_VENDER_OSFJVC)
        return -1;
      codec_sjis2jis (s, c1, c2);
      if (h <= 0xf4)
        {
          c1 -= 10;
          return ccs_jisx0208;
        }
      c1 -= 20;
      return ccs_jisx0212;
    }
  codec_sjis2jis (s, c1, c2);
  if (c1 < 0x21 || c1 > 0x7e)
    return -1;
  return ccs_jisx0208;
}

int
unicode_to_charset_code (u_int32_t u, int ccs, int vender)
{
  if (u >= UNICODE_CHAR_LIMIT)
    return -1;
  int c1, c2, i;
  switch (ccs)
    {
    case ccs_usascii:
      return u < 0x80 ? int (u) : -1;

    case ccs_jisx0201_kana:
      return unicode_kana_char_p (u) ? int (u - 0xff61) + 0xa1 : -1;

    case ccs_jisx0208:
      if (find_jis (u, vender, c1, c2) == ccs_jisx0208)
        return (c1 << 8) | c2;
      return -1;

    case ccs_jisx0212:
      i = codec_rev_get (codec_jisx0212_rev, u);
      if (i >= 0)
        return ((i / 94 + 0x21) << 8) | (i % 94 + 0x21);
      if (find_jis (u, vender, c1, c2) == ccs_jisx0212)
        return (c1 << 8) | c2;
      return -1;

    case ccs_gb2312:
      i = codec_rev_get (codec_gb2312_rev, u);
      return i < 0 ? -1 : ((i / 94 + 0x21) << 8) | (i % 94 + 0x21);

    case ccs_ksc5601:
      i = codec_rev_get (codec_ksc5601_rev, u);
      return i < 0 ? -1 : ((i / 94 + 0x21) << 8) | (i % 94 + 0x21);

    case ccs_big5:
      return big5_from_unicode (u);

    default:
      i = sb_id_of_ccs (ccs);
      if (i < 0 || u < 0x80)
        return -1;
      return codec_rev_get (*codec_sb[i].rev, u);
    }
}

int
charset_code_to_unicode (int code, int ccs, int vender)
{
  int c1 = (code >> 8) & 127, c2 = code & 127;
  int i;
  switch (ccs)
    {
    case ccs_usascii:
      return code & 127;

    case ccs_jisx0201_kana:
      i = (code & 127) | 128;
      return i >= 0xa1 && i <= 0xdf ? 0xff61 + i - 0xa1 : -1;

    case ccs_big5:
      return big5_to_unicode ((code >> 8) & 255, code & 255);

    default:
      i = sb_id_of_ccs (ccs);
      if (i >= 0)
        {
          codec_u16 u = codec_sb[i].fwd[((code & 127) | 128) - 0x80];
          return u == 0xffff ? -1 : int (u);
        }
      break;
    }

  if (c1 <= 0x20 || c1 == 0x7f || c2 <= 0x20 || c2 == 0x7f)
    return -1;
  switch (ccs)
    {
    case ccs_jisx0208:
    case ccs_jisx0212:
      return jis_to_unicode (ccs, c1, c2, vender);
    case ccs_gb2312:
      return fwd94 (codec_gb2312_fwd, c1, c2);
    case ccs_ksc5601:
      return fwd94 (codec_ksc5601_fwd, c1, c2);
    case ccs_big5_2:
      mule_g2b (ccs_big5_2, c1, c2);
      return big5_to_unicode (c1, c2);
    case ccs_cns11643_1:
      return fwd94 (codec_cns1_fwd, c1, c2);
    case ccs_cns11643_2:
      return fwd94 (codec_cns2_fwd, c1, c2);
    }
  return -1;
}

int
unicode_find_charset (u_int32_t u, int accept, bool cp932, int vender)
{
  for (int i = 0; i < numberof (iso_order); i++)
    {
      int ccs = iso_order[i].ccs;
      if (accept != -1 && !(accept & (1 << ccs)))
        continue;
      if (ccs == ccs_jisx0208)
        {
          int s = u < UNICODE_CHAR_LIMIT ? codec_cp932_code2 (u) : -1;
          if (s > 0xff)
            {
              // Without cp932: JIS X 0208 only (rows 1-8 and 16-84), not
              // NEC row 13, the IBM extension and the user defined area.
              int h = s >> 8, t = s & 255;
              if (cp932
                  || (h >= 0x81 && h <= 0x84)
                  || (h == 0x88 && t >= 0x9f)
                  || (h >= 0x89 && h <= 0x9f)
                  || (h >= 0xe0 && h <= 0xea))
                return ccs;
            }
          continue;
        }
      if (unicode_to_charset_code (u, ccs, vender) >= 0)
        return ccs;
    }
  return -1;
}

static int
charset_object_to_ccs (lisp charset)
{
  for (int i = 0; i < numberof (iso_order); i++)
    if (charset == *iso_order[i].kwd)
      return iso_order[i].ccs;
  if (charset == Kbig5_1)
    return ccs_big5_1;
  if (charset == Kbig5_2)
    return ccs_big5_2;
  if (charset == Kcns11643_1)
    return ccs_cns11643_1;
  if (charset == Kcns11643_2)
    return ccs_cns11643_2;
  FEsimple_error (Eunknown_charset, charset);
  return 0;
}

// (si:unicode-iso-code code-point charset &optional vender)
//   => the code of the charset, or nil
lisp
Fsi_unicode_iso_code (lisp code, lisp charset, lisp vender)
{
  long u = fixnum_value (code);
  int ccs = charset_object_to_ccs (charset);
  if (u < 0)
    return Qnil;
  int r = unicode_to_charset_code (u_int32_t (u), ccs,
                                   vender_depend_code (to_vender_code (vender)));
  return r < 0 ? Qnil : make_fixnum (r);
}

// (si:iso-code-unicode code charset &optional vender) => code point or nil
lisp
Fsi_iso_code_unicode (lisp code, lisp charset, lisp vender)
{
  long c = fixnum_value (code);
  int ccs = charset_object_to_ccs (charset);
  if (c < 0)
    return Qnil;
  int r = charset_code_to_unicode (int (c), ccs,
                                   vender_depend_code (to_vender_code (vender)));
  return r < 0 ? Qnil : make_fixnum (r);
}

// The properties that fn_* read, of a code point (the 1 side whatever the
// switch is).  (si:unicode-char-columns code) etc.
lisp
Fsi_unicode_char_columns (lisp code)
{
  return make_fixnum (unicode_char_columns (u_int32_t (fixnum_value (code))));
}

lisp
Fsi_unicode_kanji_char_p (lisp code)
{
  return boole (unicode_kanji_char_p (u_int32_t (fixnum_value (code))));
}

lisp
Fsi_unicode_kana_char_p (lisp code)
{
  return boole (unicode_kana_char_p (u_int32_t (fixnum_value (code))));
}

lisp
Fsi_unicode_graphic_char_p (lisp code)
{
  return boole (unicode_graphic_char_p (u_int32_t (fixnum_value (code))));
}

// t if a Char holds a Unicode code point (XYZZY_CHAR_UNICODE is 1).
lisp
Fsi_char_unicode_p ()
{
  return boole (XYZZY_CHAR_UNICODE);
}

lisp
Fiso_char_code (lisp lcc, lisp vender)
{
  check_char (lcc);
  if (key_char_p (lcc))
    return Qnil;
  Char cc = xchar_code (lcc);
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      multiple_value::count () = 2;
      multiple_value::value (1) = Qnil;
      int v = vender_depend_code (to_vender_code (vender));
      for (int i = 0; i < numberof (iso_order); i++)
        {
          int r = unicode_to_charset_code (cc, iso_order[i].ccs, v);
          if (r >= 0)
            {
              multiple_value::value (1) = *iso_order[i].kwd;
              return make_fixnum (r);
            }
        }
      return Qnil;
    }
  if (cc != DEFCHAR)
    {
      cc = (*select_vender_code_mapper (to_vender_code (vender)))(cc);
      if (cc == DEFCHAR)
        return Qnil;
    }
  multiple_value::count () = 2;
  multiple_value::value (1) = Qnil;
  int ccs = code_charset (cc);
  switch (ccs)
    {
      int c1, c2;
    case ccs_usascii:
      multiple_value::value (1) = Kus_ascii;
      return make_fixnum (cc);

    case ccs_jisx0201_kana:
      if (!kana_char_p (cc))
        return Qnil;
      multiple_value::value (1) = Kjisx0201_kana;
      return make_fixnum (cc);

    case ccs_iso8859_1:
    case ccs_iso8859_2:
    case ccs_iso8859_3:
    case ccs_iso8859_4:
    case ccs_iso8859_5:
    case ccs_iso8859_7:
    case ccs_iso8859_9:
    case ccs_iso8859_10:
    case ccs_iso8859_13:
      {
        static const lisp *const ccs2obj[] =
          {
            &Kiso8859_1,
            &Kiso8859_2,
            &Kiso8859_3,
            &Kiso8859_4,
            &Kiso8859_5,
            &Kiso8859_7,
            &Kiso8859_9,
            &Kiso8859_10,
            &Kiso8859_13,
          };
        multiple_value::value (1) = *ccs2obj[ccs - ccs_iso8859_1];
        return make_fixnum (128 | (cc & 127));
      }

    case ccs_jisx0212:
      if (cc > CCS_JISX0212_MAX)
        return Qnil;
      multiple_value::value (1) = Kjisx0212;
      int_to_jisx0212 (cc, c1, c2);
      return make_fixnum ((c1 << 8) | c2);

    case ccs_gb2312:
      if (cc > CCS_GB2312_MAX)
        return Qnil;
      multiple_value::value (1) = Kgb2312;
      int_to_gb2312 (cc, c1, c2);
      return make_fixnum ((c1 << 8) | c2);

    case ccs_ksc5601:
      if (cc > CCS_KSC5601_MAX)
        return Qnil;
      multiple_value::value (1) = Kksc5601;
      int_to_ksc5601 (cc, c1, c2);
      return make_fixnum ((c1 << 8) | c2);

    case ccs_big5:
      if (cc > CCS_BIG5_MAX)
        return Qnil;
      multiple_value::value (1) = Kbig5;
      int_to_big5 (cc, c1, c2);
      return make_fixnum ((c1 << 8) | c2);

    default:
      c1 = cc >> 8;
      c2 = cc & 255;
      if (!SJISP (c1) || !SJIS2P (c2))
        return Qnil;
      multiple_value::value (1) = Kjisx0208;
      s2j (c1, c2);
      if (c1 >= 95 + 32)
        {
          if (c1 < 105 + 32)
            c1 -= 10;
          else if (c1 < 115 + 32)
            {
              c1 -= 20;
              multiple_value::value (1) = Kjisx0212;
            }
          else
            return Qnil;
        }
      return make_fixnum ((c1 << 8) | c2);
    }
}

lisp
Fiso_code_char (lisp code, lisp charset, lisp vender)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      long c = fixnum_value (code);
      int ccs = charset_object_to_ccs (charset);
      if (c < 0)
        return Qnil;
      int r = charset_code_to_unicode (int (c), ccs,
                                       vender_depend_code (to_vender_code (vender)));
      return r < 0 ? Qnil : make_char (Char (r));
    }

  Char cc = (Char)fixnum_value (code);
  if (charset == Kus_ascii)
    return make_char (cc & 127);
  if (charset == Kjisx0201_kana)
    return make_char ((cc & 127) | 128);
  if (charset == Kiso8859_1)
    return make_char (Char ((cc & 127) | (ccs_iso8859_1 << 7)));
  if (charset == Kiso8859_2)
    return make_char (Char ((cc & 127) | (ccs_iso8859_2 << 7)));
  if (charset == Kiso8859_3)
    return make_char (Char ((cc & 127) | (ccs_iso8859_3 << 7)));
  if (charset == Kiso8859_4)
    return make_char (Char ((cc & 127) | (ccs_iso8859_4 << 7)));
  if (charset == Kiso8859_5)
    return make_char (Char ((cc & 127) | (ccs_iso8859_5 << 7)));
  if (charset == Kiso8859_7)
    return make_char (Char ((cc & 127) | (ccs_iso8859_7 << 7)));
  if (charset == Kiso8859_9)
    return make_char (Char ((cc & 127) | (ccs_iso8859_9 << 7)));
  if (charset == Kiso8859_10)
    return make_char (Char ((cc & 127) | (ccs_iso8859_10 << 7)));
  if (charset == Kiso8859_13)
    return make_char (Char ((cc & 127) | (ccs_iso8859_13 << 7)));

  int c1 = cc >> 8, c2 = cc & 255;
  if (charset == Kbig5)
    {
      if (c1 >= 0xa1 && c1 <= 0xf8 && c1 != 0xc8
          && c2 >= 0x40 && c2 <= 0x7e || c2 >= 0xa1 && c2 <= 0xfe)
        return make_char (big5_to_int (c1, c2));
      return Qnil;
    }

  c1 &= 127;
  c2 &= 127;
  if (c1 <= 0x20 || c1 == 0x7f || c2 <= 0x20 || c2 == 0x7f)
    return Qnil;

  if (charset == Kjisx0208)
    {
      if (c1 >= 0x75
          && vender_depend_code (to_vender_code (vender)) == ENCODING_ISO_VENDER_OSFJVC)
        c1 += 10;
      return make_char ((j2sh (c1, c2) << 8) | j2sl (c1, c2));
    }
  if (charset == Kjisx0212)
    return make_char (jisx0212_to_internal (c1, c2,
                                            (vender_depend_code (to_vender_code (vender)))));
  if (charset == Kgb2312)
    return make_char (gb2312_to_int (c1, c2));
  if (charset == Kksc5601)
    return make_char (ksc5601_to_int (c1, c2));
  if (charset == Kbig5_1)
    {
      mule_g2b (ccs_big5_1, c1, c2);
      return make_char (big5_to_int (c1, c2));
    }
  if (charset == Kbig5_2)
    {
      mule_g2b (ccs_big5_2, c1, c2);
      return make_char (big5_to_int (c1, c2));
    }
  if (charset == Kcns11643_1)
    {
      init_cns11643_table ();
      cc = cns11643_1_to_internal[c1 * 94 + c2 - (0x21 * 94 + 0x21)];
      if (cc != CHAR_NONE)
        return make_char (cc);
      return Qnil;
    }
  if (charset == Kcns11643_2)
    {
      init_cns11643_table ();
      cc = cns11643_2_to_internal[c1 * 94 + c2 - (0x21 * 94 + 0x21)];
      if (cc != CHAR_NONE)
        return make_char (cc);
      return Qnil;
    }

  return FEsimple_error (Eunknown_charset, charset);
}

lisp
Fiso_char_charset (lisp lcc, lisp vender)
{
  Fiso_char_code (lcc, vender);
  multiple_value::count () = 1;
  return multiple_value::value (1);
}

lisp
Fword_char_p (lisp lcc)
{
  check_char (lcc);
  if (key_char_p (lcc))
    return Qnil;
  return boole (word_state::char_category
                (xsyntax_table (selected_buffer ()->lsyntax_table),
                 xchar_code (lcc)) == word_state::WCword);
}
