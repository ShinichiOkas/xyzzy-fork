// The codec layer: cp932, 1 byte character sets, Big5, binary and UTF.
// (The ISO 2022 family is in codec-iso2022.cc.)
#include "stdafx.h"
#include "ed.h"
#include "codec-priv.h"

// ---------------------------------------------------------------------------
// duplicated codes of cp932

int
dup_memory::dup_lookup (codec_char u)
{
  size_t lo = 0, hi = m_v.size ();
  while (lo < hi)
    {
      size_t mid = (lo + hi) / 2;
      if (m_v[mid].u < u)
        lo = mid + 1;
      else
        hi = mid;
    }
  if (lo < m_v.size () && m_v[lo].u == u)
    return m_v[lo].code;
  return -1;
}

void
dup_memory::dup_remember (codec_char u, int code)
{
  size_t lo = 0, hi = m_v.size ();
  while (lo < hi)
    {
      size_t mid = (lo + hi) / 2;
      if (m_v[mid].u < u)
        lo = mid + 1;
      else
        hi = mid;
    }
  if (lo < m_v.size () && m_v[lo].u == u)
    return;                     // the first one stays
  entry e;
  e.u = u;
  e.code = code;
  m_v.insert (m_v.begin () + lo, e);
}

// ---------------------------------------------------------------------------
// helpers

int
codec_cp932_code2 (codec_char u)
{
  int c = codec_rev_get (codec_cp932_rev, u);
  if (c >= 0)
    return c;
  switch (u)
    {
    case 0x301c: return 0x8160;
    case 0x2016: return 0x8161;
    case 0x2212: return 0x817c;
    case 0x00a2: return 0x8191;
    case 0x00a3: return 0x8192;
    case 0x00ac: return 0x81ca;
    }
  return -1;
}

codec_char
codec_jis_form (codec_char u)
{
  switch (u)
    {
    case 0xff5e: return 0x301c;
    case 0x2225: return 0x2016;
    case 0xff0d: return 0x2212;
    case 0xffe0: return 0x00a2;
    case 0xffe1: return 0x00a3;
    case 0xffe2: return 0x00ac;
    }
  return u;
}

// ---------------------------------------------------------------------------
// binary

class binary_decoder: public char_decoder
{
public:
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        *d++ = *s++;
      src = s;
      dst = d;
    }
};

class binary_encoder: public char_encoder
{
public:
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          codec_char c = *s++;
          if (c < 0x100)
            *d++ = u_char (c);
          else if (codec_raw_byte_p (c))
            *d++ = u_char (c - CODEC_RAW_BYTE_BASE);
          else
            *d++ = CODEC_DEFCHAR;
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char c) const
    {return c < 0x100 || codec_raw_byte_p (c);}
};

// ---------------------------------------------------------------------------
// 1 byte character sets

class sb_decoder: public char_decoder
{
  const codec_sb_table &m_t;
public:
  sb_decoder (int id) : m_t (codec_sb[id]) {}
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          u_char b = *s++;
          if (b < 0x80)
            *d++ = b;
          else
            {
              codec_u16 u = m_t.fwd[b - 0x80];
              *d++ = u == 0xffff ? codec_raw_byte (b) : codec_char (u);
            }
        }
      src = s;
      dst = d;
    }
};

class sb_encoder: public char_encoder
{
  const codec_sb_table &m_t;
  int code (codec_char c) const
    {
      if (c < 0x80)
        return int (c);
      if (codec_raw_byte_p (c))
        return int (c - CODEC_RAW_BYTE_BASE);
      return codec_rev_get (*m_t.rev, c);
    }
public:
  sb_encoder (int id) : m_t (codec_sb[id]) {}
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          int b = code (*s++);
          *d++ = b < 0 ? u_char (CODEC_DEFCHAR) : u_char (b);
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char c) const {return code (c) >= 0;}
};

// ---------------------------------------------------------------------------
// cp932

class cp932_decoder: public char_decoder
{
  codec_context *m_ctx;
public:
  cp932_decoder (codec_context *ctx) : m_ctx (ctx) {}
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int b = *s;
          if (b <= 0x80)
            {
              *d++ = b;
              s++;
            }
          else if (b >= 0xa1 && b <= 0xdf)
            {
              *d++ = 0xff61 + (b - 0xa1);
              s++;
            }
          else if (codec_cp932_lead_p (b))
            {
              if (s + 1 >= se)
                {
                  if (!final)
                    break;
                  *d++ = codec_raw_byte (u_char (b));
                  s++;
                  continue;
                }
              int t = s[1];
              if (!codec_cp932_trail_p (t))
                {
                  // The lead byte alone. The next byte is read again.
                  *d++ = codec_raw_byte (u_char (b));
                  s++;
                  continue;
                }
              int u = codec_cp932_get (b, t);
              if (u < 0)
                {
                  // A code that is not defined: both bytes are kept.
                  *d++ = codec_raw_byte (u_char (b));
                  *d++ = codec_raw_byte (u_char (t));
                }
              else
                {
                  *d++ = u;
                  if (m_ctx)
                    {
                      int code = (b << 8) | t;
                      if (codec_rev_get (codec_cp932_rev, u) != code)
                        m_ctx->dup_remember (u, code);
                    }
                }
              s += 2;
            }
          else
            {
              // 0xa0, 0xfd..0xff
              *d++ = codec_raw_byte (u_char (b));
              s++;
            }
        }
      src = s;
      dst = d;
    }
};

class cp932_encoder: public char_encoder
{
  codec_context *m_ctx;

  // A single byte code (< 0x100), a 2 byte code, or -1.
  int code (codec_char u) const
    {
      if (u <= 0x80)
        return int (u);
      if (u >= 0xff61 && u <= 0xff9f)
        return int (u - 0xff61) + 0xa1;
      if (codec_raw_byte_p (u))
        return int (u - CODEC_RAW_BYTE_BASE);
      if (m_ctx)
        {
          int c = m_ctx->dup_lookup (u);
          if (c >= 0)
            return c;
        }
      int c = codec_cp932_code2 (u);
      if (c >= 0)
        return c;
      switch (u)
        {
        case 0x00a5: return 0x5c;
        case 0x203e: return 0x7e;
        case 0xf8f0: return 0xa0;
        case 0xf8f1: return 0xfd;
        case 0xf8f2: return 0xfe;
        case 0xf8f3: return 0xff;
        }
      return -1;
    }
public:
  cp932_encoder (codec_context *ctx) : m_ctx (ctx) {}
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          int c = code (*s++);
          if (c < 0)
            *d++ = CODEC_DEFCHAR;
          else if (c > 0xff)
            {
              *d++ = u_char (c >> 8);
              *d++ = u_char (c);
            }
          else
            *d++ = u_char (c);
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char u) const {return code (u) >= 0;}
};

// ---------------------------------------------------------------------------
// Big5

inline int
big5_trail_index (int t)
{
  if (t >= 0x40 && t <= 0x7e)
    return t - 0x40;
  if (t >= 0xa1 && t <= 0xfe)
    return t - 0xa1 + 63;
  return -1;
}

class big5_decoder: public char_decoder
{
public:
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int b = *s;
          if (b < 0x80)
            {
              *d++ = b;
              s++;
            }
          else if (b >= 0xa1 && b <= 0xf9)
            {
              if (s + 1 >= se)
                {
                  if (!final)
                    break;
                  *d++ = codec_raw_byte (u_char (b));
                  s++;
                  continue;
                }
              int t = s[1];
              int ti = big5_trail_index (t);
              if (ti < 0)
                {
                  *d++ = codec_raw_byte (u_char (b));
                  s++;
                  continue;
                }
              codec_u16 u = codec_big5_fwd[(b - 0xa1) * CODEC_BIG5_TRAILS + ti];
              if (u == 0xffff)
                {
                  *d++ = codec_raw_byte (u_char (b));
                  *d++ = codec_raw_byte (u_char (t));
                }
              else
                *d++ = u;
              s += 2;
            }
          else
            {
              *d++ = codec_raw_byte (u_char (b));
              s++;
            }
        }
      src = s;
      dst = d;
    }
};

class big5_encoder: public char_encoder
{
  static int code (codec_char u)
    {
      if (u < 0x80)
        return int (u);
      if (codec_raw_byte_p (u))
        return int (u - CODEC_RAW_BYTE_BASE);
      int i = codec_rev_get (codec_big5_rev, u);
      if (i < 0)
        return -1;
      int h = i / CODEC_BIG5_TRAILS + 0xa1;
      int ti = i % CODEC_BIG5_TRAILS;
      int t = ti < 63 ? ti + 0x40 : ti - 63 + 0xa1;
      return (h << 8) | t;
    }
public:
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          codec_char u = *s++;
          int c = code (u);
          if (c < 0)
            *d++ = CODEC_DEFCHAR;
          else if (c > 0xff && !codec_raw_byte_p (u))
            {
              *d++ = u_char (c >> 8);
              *d++ = u_char (c);
            }
          else
            *d++ = u_char (c);
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char u) const {return code (u) >= 0;}
};

// ---------------------------------------------------------------------------
// UTF: common

class utf_decoder: public char_decoder
{
  bool m_sig_pending;
  bool m_has_sig;
protected:
  utf_decoder (int flags)
    : m_sig_pending ((flags & ENCODING_UTF_SIGNATURE) != 0), m_has_sig (false) {}
  // Puts a character. The first character is dropped when it is a
  // signature (BOM).
  void put (codec_char *&d, codec_char c)
    {
      if (m_sig_pending)
        {
          m_sig_pending = false;
          if (c == 0xfeff)
            {
              m_has_sig = true;
              return;
            }
        }
      *d++ = c;
    }
  // Consumes a signature without a flag (UTF-16 with an unknown byte order).
  void sig_found () {m_has_sig = true; m_sig_pending = false;}
public:
  virtual bool has_signature () const {return m_has_sig;}
};

class utf_encoder: public char_encoder
{
protected:
  const bool m_windows;
  bool m_bom;
  utf_encoder (int flags)
    : m_windows ((flags & ENCODING_UTF_WINDOWS) != 0),
      m_bom ((flags & ENCODING_UTF_SIGNATURE) != 0) {}
  codec_char fold (codec_char c) const
    {return m_windows ? c : codec_jis_form (c);}
};

inline bool
surrogate_p (codec_char c)
{
  return c >= 0xd800 && c <= 0xdfff;
}

// ---------------------------------------------------------------------------
// UTF-8

class utf8_decoder: public utf_decoder
{
public:
  utf8_decoder (int flags) : utf_decoder (flags) {}
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int b = *s;
          if (b < 0x80)
            {
              put (d, b);
              s++;
              continue;
            }
          int need;
          codec_char code;
          int lo = 0x80, hi = 0xbf;      // range of the 2nd byte
          if (b >= 0xc2 && b <= 0xdf)
            {
              need = 1;
              code = b & 0x1f;
            }
          else if (b >= 0xe0 && b <= 0xef)
            {
              need = 2;
              code = b & 0x0f;
              if (b == 0xe0)
                lo = 0xa0;
              else if (b == 0xed)
                hi = 0x9f;
            }
          else if (b >= 0xf0 && b <= 0xf4)
            {
              need = 3;
              code = b & 0x07;
              if (b == 0xf0)
                lo = 0x90;
              else if (b == 0xf4)
                hi = 0x8f;
            }
          else
            {
              put (d, codec_raw_byte (u_char (b)));
              s++;
              continue;
            }

          int k;
          bool ok = true;
          for (k = 1; k <= need && s + k < se; k++)
            {
              int c = s[k];
              if (k == 1 ? (c < lo || c > hi) : (c < 0x80 || c > 0xbf))
                {
                  ok = false;
                  break;
                }
              code = (code << 6) | (c & 0x3f);
            }
          if (!ok)
            {
              put (d, codec_raw_byte (u_char (b)));
              s++;
              continue;
            }
          if (k <= need)
            {
              // cut at the end of the input
              if (!final)
                break;
              put (d, codec_raw_byte (u_char (b)));
              s++;
              continue;
            }
          put (d, code);
          s += need + 1;
        }
      src = s;
      dst = d;
    }
};

class utf8_encoder: public utf_encoder
{
  bool encodable (codec_char c) const
    {return c <= 0x10ffff && !surrogate_p (c);}
public:
  utf8_encoder (int flags) : utf_encoder (flags) {}
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          if (m_bom)
            {
              m_bom = false;
              *d++ = 0xef;
              *d++ = 0xbb;
              *d++ = 0xbf;
            }
          codec_char c = *s++;
          if (codec_raw_byte_p (c))
            {
              *d++ = u_char (c - CODEC_RAW_BYTE_BASE);
              continue;
            }
          c = fold (c);
          if (!encodable (c))
            *d++ = CODEC_DEFCHAR;
          else if (c < 0x80)
            *d++ = u_char (c);
          else if (c < 0x800)
            {
              *d++ = u_char (0xc0 | (c >> 6));
              *d++ = u_char (0x80 | (c & 0x3f));
            }
          else if (c < 0x10000)
            {
              *d++ = u_char (0xe0 | (c >> 12));
              *d++ = u_char (0x80 | ((c >> 6) & 0x3f));
              *d++ = u_char (0x80 | (c & 0x3f));
            }
          else
            {
              *d++ = u_char (0xf0 | (c >> 18));
              *d++ = u_char (0x80 | ((c >> 12) & 0x3f));
              *d++ = u_char (0x80 | ((c >> 6) & 0x3f));
              *d++ = u_char (0x80 | (c & 0x3f));
            }
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char c) const
    {return codec_raw_byte_p (c) || encodable (fold (c));}
};

// ---------------------------------------------------------------------------
// UTF-16

class utf16_decoder: public utf_decoder
{
  int m_order;                  // 0: unknown, 1: little endian, 2: big endian
  const bool m_default_be;

  int unit (const u_char *s) const
    {return m_order == 2 ? (s[0] << 8) | s[1] : (s[1] << 8) | s[0];}
public:
  utf16_decoder (int flags, bool default_be)
    : utf_decoder (flags), m_order (0), m_default_be (default_be)
    {
      if (flags & ENCODING_UTF_BE)
        m_order = 2;
      else if (flags & ENCODING_UTF_LE)
        m_order = 1;
    }
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      if (!m_order)
        {
          if (se - s < 2)
            {
              if (final && s < se)
                {
                  put (d, codec_raw_byte (*s++));
                  m_order = m_default_be ? 2 : 1;
                }
              src = s;
              dst = d;
              return;
            }
          int w = (s[0] << 8) | s[1];
          if (w == 0xfeff)
            {
              m_order = 2;
              sig_found ();
              s += 2;
            }
          else if (w == 0xfffe)
            {
              m_order = 1;
              sig_found ();
              s += 2;
            }
          else
            m_order = m_default_be ? 2 : 1;
        }
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          if (se - s < 2)
            {
              if (!final)
                break;
              put (d, codec_raw_byte (*s++));
              continue;
            }
          int w = unit (s);
          if (w >= 0xd800 && w <= 0xdbff)
            {
              if (se - s < 4)
                {
                  if (!final)
                    break;
                  put (d, codec_char (w));
                  s += 2;
                  continue;
                }
              int w2 = unit (s + 2);
              if (w2 >= 0xdc00 && w2 <= 0xdfff)
                {
                  put (d, 0x10000 + ((codec_char (w) - 0xd800) << 10)
                       + (codec_char (w2) - 0xdc00));
                  s += 4;
                  continue;
                }
            }
          put (d, codec_char (w));
          s += 2;
        }
      src = s;
      dst = d;
    }
};

class utf16_encoder: public utf_encoder
{
  const bool m_be;
  bool encodable (codec_char c) const {return c <= 0x10ffff;}
  void put16 (u_char *&d, int w) const
    {
      if (m_be)
        {
          *d++ = u_char (w >> 8);
          *d++ = u_char (w);
        }
      else
        {
          *d++ = u_char (w);
          *d++ = u_char (w >> 8);
        }
    }
public:
  utf16_encoder (int flags, bool be) : utf_encoder (flags), m_be (be) {}
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          if (m_bom)
            {
              m_bom = false;
              put16 (d, 0xfeff);
            }
          codec_char c = *s++;
          if (codec_raw_byte_p (c))
            {
              *d++ = u_char (c - CODEC_RAW_BYTE_BASE);
              continue;
            }
          c = fold (c);
          if (!encodable (c))
            put16 (d, CODEC_DEFCHAR);
          else if (c >= 0x10000)
            {
              put16 (d, 0xd800 + ((c - 0x10000) >> 10));
              put16 (d, 0xdc00 + ((c - 0x10000) & 0x3ff));
            }
          else
            put16 (d, int (c));
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char c) const
    {return codec_raw_byte_p (c) || encodable (c);}
};

// ---------------------------------------------------------------------------
// UTF-7 (and the modified UTF-7 of IMAP4 mailbox names)

class utf7_decoder: public utf_decoder
{
  const bool m_imap;
  const int m_shift;
  bool m_in_shift;
  unsigned m_acc;
  int m_nbits;
  int m_nchars;                 // base64 characters of this shift
  codec_char m_high;            // a high surrogate that waits for its pair

  int b64 (int c) const
    {return m_imap ? imap4_base64_decode (c) : base64_decode (c);}

  void put_unit (codec_char *&d, int w)
    {
      if (m_high)
        {
          if (w >= 0xdc00 && w <= 0xdfff)
            {
              put (d, 0x10000 + ((m_high - 0xd800) << 10) + (w - 0xdc00));
              m_high = 0;
              return;
            }
          put (d, m_high);
          m_high = 0;
        }
      if (w >= 0xd800 && w <= 0xdbff)
        m_high = w;
      else
        put (d, w);
    }
  void flush_high (codec_char *&d)
    {
      if (m_high)
        {
          put (d, m_high);
          m_high = 0;
        }
    }
public:
  utf7_decoder (int flags)
    : utf_decoder (flags | ENCODING_UTF_SIGNATURE),
      m_imap ((flags & UTF7_IMAP4_MAILBOX_NAME) != 0),
      m_shift ((flags & UTF7_IMAP4_MAILBOX_NAME) ? '&' : '+'),
      m_in_shift (false), m_acc (0), m_nbits (0), m_nchars (0), m_high (0) {}
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int c = *s;
          if (!m_in_shift)
            {
              s++;
              if (c == m_shift)
                {
                  m_in_shift = true;
                  m_acc = 0;
                  m_nbits = 0;
                  m_nchars = 0;
                }
              else if (c < 0x80)
                put (d, c);
              else
                put (d, codec_raw_byte (u_char (c)));
              continue;
            }
          int v = b64 (c);
          if (v < 64)
            {
              s++;
              m_nchars++;
              m_acc = (m_acc << 6) | v;
              m_nbits += 6;
              while (m_nbits >= 16)
                {
                  int w = (m_acc >> (m_nbits - 16)) & 0xffff;
                  m_nbits -= 16;
                  m_acc &= (1u << m_nbits) - 1;
                  put_unit (d, w);
                }
              continue;
            }
          // The end of the shift.
          flush_high (d);
          m_in_shift = false;
          if (c == '-')
            {
              s++;
              if (!m_nchars)
                put (d, m_shift);
            }
        }
      if (final && s == se)
        flush_high (d);
      src = s;
      dst = d;
    }
};

class utf7_encoder: public utf_encoder
{
  const bool m_imap;
  const int m_shift;
  int m_accept;
  const char *m_b64;
  bool m_in_shift;
  int m_nunits;
  int m_first_unit;
  unsigned m_acc;
  int m_nbits;

  void put_b64 (u_char *&d, int v) {*d++ = u_char (m_b64[v & 63]);}

  void add16 (u_char *&d, int w)
    {
      m_acc = (m_acc << 16) | w;
      m_nbits += 16;
      while (m_nbits >= 6)
        {
          put_b64 (d, m_acc >> (m_nbits - 6));
          m_nbits -= 6;
          m_acc &= (1u << m_nbits) - 1;
        }
    }

  // The first unit of a shift waits: when the shift has only this one and it
  // is the shift character, it is written as "+-".
  void put_unit (u_char *&d, int w)
    {
      if (!m_in_shift)
        {
          *d++ = u_char (m_shift);
          m_in_shift = true;
          m_nunits = 0;
          m_acc = 0;
          m_nbits = 0;
        }
      if (!m_nunits)
        {
          m_first_unit = w;
          m_nunits = 1;
          return;
        }
      if (m_nunits == 1)
        add16 (d, m_first_unit);
      m_nunits++;
      add16 (d, w);
    }

  // Ends the shift. term: write '-' when it is needed after a base64 text.
  void end_shift (u_char *&d, bool term)
    {
      if (m_nunits == 1 && m_first_unit == m_shift)
        {
          *d++ = '-';           // "+-"
        }
      else
        {
          if (m_nunits == 1)
            add16 (d, m_first_unit);
          if (m_nbits)
            put_b64 (d, m_acc << (6 - m_nbits));
          if (term)
            *d++ = '-';
        }
      m_in_shift = false;
      m_nunits = 0;
      m_nbits = 0;
      m_acc = 0;
    }

  bool direct (codec_char c) const
    {return c < 0x80 && (utf7_set (int (c)) & m_accept);}
public:
  utf7_encoder (int flags)
    : utf_encoder (flags & ~ENCODING_UTF_SIGNATURE),
      m_imap ((flags & UTF7_IMAP4_MAILBOX_NAME) != 0),
      m_shift ((flags & UTF7_IMAP4_MAILBOX_NAME) ? '&' : '+'),
      m_accept (flags),
      m_b64 ((flags & UTF7_IMAP4_MAILBOX_NAME)
             ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+,"
             : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"),
      m_in_shift (false), m_nunits (0), m_first_unit (0), m_acc (0), m_nbits (0)
    {
      if (m_imap)
        m_accept |= UTF7_IMAP4_SHIFT_CHAR;
    }
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          codec_char c = *s++;
          if (codec_raw_byte_p (c))
            {
              if (m_in_shift)
                end_shift (d, true);
              *d++ = u_char (c - CODEC_RAW_BYTE_BASE);
              continue;
            }
          c = fold (c);
          if (c > 0x10ffff || surrogate_p (c))
            c = CODEC_DEFCHAR;
          if (direct (c))
            {
              if (m_in_shift)
                {
                  bool term = (m_imap || c == '-'
                               || (utf7_set (int (c)) & UTF7_SET_B));
                  end_shift (d, term);
                }
              *d++ = u_char (c);
              if (int (c) == m_shift)
                *d++ = '-';
            }
          else if (c >= 0x10000)
            {
              put_unit (d, 0xd800 + ((c - 0x10000) >> 10));
              put_unit (d, 0xdc00 + ((c - 0x10000) & 0x3ff));
            }
          else
            put_unit (d, int (c));
        }
      src = s;
      dst = d;
    }
  virtual void finish (u_char *&d, u_char *)
    {
      if (m_in_shift)
        end_shift (d, true);
    }
  virtual bool can_encode (codec_char c) const
    {
      if (codec_raw_byte_p (c))
        return true;
      c = fold (c);
      return c <= 0x10ffff && !surrogate_p (c);
    }
};

// ---------------------------------------------------------------------------
// UTF-5

inline int
utf5_digit (int c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 10;
  return 36;
}

class utf5_decoder: public utf_decoder
{
  bool m_in_char;
  codec_char m_code;

  void emit (codec_char *&d)
    {
      if (m_code <= 0x10ffff)
        put (d, m_code);
      m_in_char = false;
    }
public:
  utf5_decoder (int flags) : utf_decoder (flags), m_in_char (false), m_code (0) {}
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int n = utf5_digit (*s);
          if (m_in_char)
            {
              if (n < 16)
                {
                  m_code = (m_code << 4) | n;
                  s++;
                  continue;
                }
              emit (d);
              continue;         // the byte is read again as a start
            }
          s++;
          if (n >= 16 && n < 32)
            {
              m_in_char = true;
              m_code = n - 16;
            }
          // Other bytes are not a part of UTF-5. They are skipped.
        }
      if (final && s == se && m_in_char)
        emit (d);
      src = s;
      dst = d;
    }
};

class utf5_encoder: public utf_encoder
{
  bool encodable (codec_char c) const {return c <= 0x10ffff;}
public:
  utf5_encoder (int flags) : utf_encoder (flags & ~ENCODING_UTF_SIGNATURE) {}
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          codec_char c = *s++;
          if (codec_raw_byte_p (c))
            {
              *d++ = u_char (c - CODEC_RAW_BYTE_BASE);
              continue;
            }
          c = fold (c);
          if (!encodable (c))
            c = CODEC_DEFCHAR;
          int n = 1;
          while (n < 8 && (c >> (4 * n)))
            n++;
          *d++ = u_char (digits[((c >> (4 * (n - 1))) & 15) + 16]);
          for (int i = n - 2; i >= 0; i--)
            *d++ = u_char (digits[(c >> (4 * i)) & 15]);
        }
      src = s;
      dst = d;
    }
  virtual bool can_encode (codec_char c) const
    {return codec_raw_byte_p (c) || encodable (fold (c));}
};

// ---------------------------------------------------------------------------
// factories

static int
sb_id (const codec_spec &spec)
{
  switch (spec.type)
    {
    case encoding_iso8859:
      switch (spec.iso8859_charset)
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
      break;

    case encoding_windows_codepage:
      switch (spec.windows_codepage)
        {
        case CP_LATIN1: return CODEC_SB_CP1252;
        case CP_LATIN2: return CODEC_SB_CP1250;
        case CP_CYRILLIC: return CODEC_SB_CP1251;
        case CP_GREEK: return CODEC_SB_CP1253;
        case CP_TURKISH: return CODEC_SB_CP1254;
        case CP_BALTIC: return CODEC_SB_CP1257;
        case CP_KOI8R: return CODEC_SB_KOI8R;
        case CP_PSEUDO_KOI8U: return CODEC_SB_KOI8U;
        }
      break;
    }
  return -1;
}

static bool
cp932_p (const codec_spec &spec)
{
  return (spec.type == encoding_sjis
          || (spec.type == encoding_windows_codepage
              && spec.windows_codepage == CP_JAPANESE));
}

char_decoder *
make_decoder (const codec_spec &spec, codec_context *ctx)
{
  if (cp932_p (spec))
    return new cp932_decoder (ctx);
  int sb = sb_id (spec);
  if (sb >= 0)
    return new sb_decoder (sb);

  switch (spec.type)
    {
    case encoding_big5:
      return new big5_decoder;

    case encoding_binary:
      return new binary_decoder;

    case encoding_iso2022:
    case encoding_iso2022_noesc:
      return codec_make_iso2022_decoder (spec);

    case encoding_utf5:
      return new utf5_decoder (spec.flags);

    case encoding_utf7:
      return new utf7_decoder (spec.flags);

    case encoding_utf8:
      return new utf8_decoder (spec.flags);

    case encoding_utf16:
      return new utf16_decoder (spec.flags, spec.utf16_default_big_endian);

    default:
      return 0;
    }
}

char_encoder *
make_encoder (const codec_spec &spec, codec_context *ctx)
{
  if (cp932_p (spec))
    return new cp932_encoder (ctx);
  int sb = sb_id (spec);
  if (sb >= 0)
    return new sb_encoder (sb);

  switch (spec.type)
    {
    case encoding_big5:
      return new big5_encoder;

    case encoding_binary:
      return new binary_encoder;

    case encoding_iso2022:
    case encoding_iso2022_noesc:
      return codec_make_iso2022_encoder (spec);

    case encoding_utf5:
      return new utf5_encoder (spec.flags);

    case encoding_utf7:
      return new utf7_encoder (spec.flags);

    case encoding_utf8:
      return new utf8_encoder (spec.flags);

    case encoding_utf16:
      {
        bool be;
        if (spec.flags & ENCODING_UTF_LE)
          be = false;
        else if (spec.flags & ENCODING_UTF_BE)
          be = true;
        else
          be = spec.utf16_default_big_endian;
        return new utf16_encoder (spec.flags, be);
      }

    default:
      return 0;
    }
}

bool
codec_spec_from_encoding (lisp encoding, codec_spec &spec)
{
  if (!char_encoding_p (encoding))
    return false;
  memset (&spec, 0, sizeof spec);
  spec.type = xchar_encoding_type (encoding);
  spec.utf16_default_big_endian
    = xsymbol_value (Vdefault_utf16_byte_order) == Kbig_endian;
  switch (spec.type)
    {
    case encoding_iso2022:
    case encoding_iso2022_noesc:
      spec.flags = xchar_encoding_iso_flags (encoding);
      memcpy (spec.designatable, xchar_encoding_iso_designatable (encoding),
              sizeof spec.designatable);
      memcpy (spec.initial, xchar_encoding_iso_initial (encoding),
              sizeof spec.initial);
      spec.lang = xchar_encoding_iso_cjk (encoding);
      spec.vender = vender_depend_code (spec.flags & ENCODING_ISO_VENDER_MASK);
      break;

    case encoding_utf5:
    case encoding_utf7:
    case encoding_utf8:
    case encoding_utf16:
      spec.flags = xchar_encoding_utf_flags (encoding);
      spec.lang = xchar_encoding_utf_cjk (encoding);
      break;

    case encoding_iso8859:
      spec.iso8859_charset = xchar_encoding_iso8859_charset (encoding);
      break;

    case encoding_windows_codepage:
      spec.windows_codepage = xchar_encoding_windows_codepage (encoding);
      if (!cp932_p (spec) && sb_id (spec) < 0)
        return false;
      break;

    case encoding_sjis:
    case encoding_big5:
    case encoding_binary:
      break;

    default:
      return false;
    }
  return true;
}

char_decoder *
make_decoder (lisp encoding, codec_context *ctx)
{
  codec_spec spec;
  if (!codec_spec_from_encoding (encoding, spec))
    return 0;
  return make_decoder (spec, ctx);
}

char_encoder *
make_encoder (lisp encoding, codec_context *ctx)
{
  codec_spec spec;
  if (!codec_spec_from_encoding (encoding, spec))
    return 0;
  return make_encoder (spec, ctx);
}

// ---------------------------------------------------------------------------
// whole sequences

// chunk > 0 gives the input in pieces of chunk bytes (the decoder is told
// that it is the final one only at the end). The result is the same: it is
// what a stream does with a buffer.
void
codec_decode_all (char_decoder *d, const u_char *s, size_t n,
                  std::vector <codec_char> &out, size_t chunk)
{
  const u_char *end = s + n;
  const u_char *se = chunk ? s : end;
  codec_char buf[1024];
  for (;;)
    {
      if (se < end)
        se = size_t (end - se) > chunk ? se + chunk : end;
      bool final = se == end;
      codec_char *dst = buf;
      const u_char *s0 = s;
      d->decode (s, se, dst, buf + numberof (buf), final);
      out.insert (out.end (), buf, dst);
      if (s == s0 && dst == buf && final)
        break;                  // nothing more: a decoder has flushed its state
    }
}

void
codec_encode_all (char_encoder *e, const codec_char *s, size_t n,
                  std::vector <u_char> &out, size_t chunk)
{
  const codec_char *end = s + n;
  u_char buf[1024];
  while (s < end)
    {
      const codec_char *se = (chunk && size_t (end - s) > chunk) ? s + chunk : end;
      while (s < se)
        {
          u_char *dst = buf;
          const codec_char *s0 = s;
          e->encode (s, se, dst, buf + sizeof buf);
          out.insert (out.end (), buf, dst);
          if (s == s0 && dst == buf)
            break;              // no progress
        }
    }
  u_char *dst = buf;
  e->finish (dst, buf + sizeof buf);
  out.insert (out.end (), buf, dst);
}
