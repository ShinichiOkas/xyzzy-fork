// -*-C++-*-
// Shared by codec.cc and codec-iso2022.cc.
#ifndef _codec_priv_h_
#define _codec_priv_h_

#include "codec.h"
#include "codec-tab.h"

// cp932 --------------------------------------------------------------------

inline int
codec_cp932_lead_index (int h)
{
  if (h >= 0x81 && h <= 0x9f)
    return h - 0x81;
  if (h >= 0xe0 && h <= 0xfc)
    return h - 0xe0 + 31;
  return -1;
}

inline bool
codec_cp932_lead_p (int h)
{
  return codec_cp932_lead_index (h) >= 0;
}

inline bool
codec_cp932_trail_p (int t)
{
  return (t >= 0x40 && t <= 0x7e) || (t >= 0x80 && t <= 0xfc);
}

// The Unicode of a 2 byte cp932 code, or -1.
inline int
codec_cp932_get (int h, int t)
{
  int li = codec_cp932_lead_index (h);
  if (li < 0 || t < 0x40 || t > 0xfc)
    return -1;
  codec_u16 u = codec_cp932_fwd[li * CODEC_CP932_TRAILS + t - 0x40];
  return u == 0xffff ? -1 : int (u);
}

// The 2 byte cp932 code that Windows writes for a Unicode, or -1. The code
// points that are not in the table of cp932 but that Windows takes as a
// "best fit" of the JIS form (U+301C and so on) are in code2.
int codec_cp932_code2 (codec_char u);

// JIS X 0208 code (c1, c2 in 0x21..0x7e) <-> cp932 code.
inline int
codec_jis2sjis (int c1, int c2)
{
  int h = ((c1 - 1) >> 1) + (c1 <= 0x5e ? 0x71 : 0xb1);
  int l = c2 + ((c1 & 1) ? (c2 < 0x60 ? 0x1f : 0x20) : 0x7e);
  return (h << 8) | l;
}

inline void
codec_sjis2jis (int s, int &c1, int &c2)
{
  int h = s >> 8, l = s & 255;
  h -= h > 0x9f ? 0xb1 : 0x71;
  h += h + 1;
  if (l > 0x9e)
    {
      l -= 0x7e;
      h++;
    }
  else
    l -= l > 0x7f ? 0x20 : 0x1f;
  c1 = h;
  c2 = l;
}

// UTF ------------------------------------------------------------------------

// Characters that only Windows has in the cp932 form. A UTF encoder that is
// not "windows" (std-utf8 and so on) writes the JIS form (the second).
codec_char codec_jis_form (codec_char u);

// ISO 2022 ----------------------------------------------------------------

char_decoder *codec_make_iso2022_decoder (const codec_spec &);
char_encoder *codec_make_iso2022_encoder (const codec_spec &);

#endif
