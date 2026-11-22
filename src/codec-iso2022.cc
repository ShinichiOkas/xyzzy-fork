// The codec layer: the ISO 2022 family (ISO-2022-JP and its variants, the
// 8 bit forms with escape sequences, EUC-JP/KR/CN without escape sequences).
//
// The decoder and the encoder follow the rules of the converters in
// encoding.cc (iso2022_to_internal_stream, internal_to_iso2022_stream) but
// the characters are Unicode: the character sets are the tables that
// gen-codec.cc made from src/unicode/*.TXT.
#include "stdafx.h"
#include "ed.h"
#include "codec-priv.h"

#define ISO_ESC 0x1b
#define ISO_SO 0x0e
#define ISO_SI 0x0f
#define ISO_SS2 0x8e
#define ISO_SS3 0x8f

// ---------------------------------------------------------------------------
// character sets

static inline bool
set_1byte_94_p (int ccs)
{
  return ccs == ccs_usascii || ccs == ccs_jisx0201_kana;
}

static inline bool
set_96_p (int ccs)
{
  return ccs >= ccs_iso8859_1 && ccs <= ccs_iso8859_13;
}

static inline bool
set_1byte_p (int ccs)
{
  return ccs < ccs_jisx0208;
}

static int
sb_of_iso8859 (int ccs)
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
big5_get (int h, int t)
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
big5_code (codec_char u)
{
  int i = codec_rev_get (codec_big5_rev, u);
  if (i < 0)
    return -1;
  int h = i / CODEC_BIG5_TRAILS + 0xa1;
  int ti = i % CODEC_BIG5_TRAILS;
  int t = ti < 63 ? ti + 0x40 : ti - 63 + 0xa1;
  return (h << 8) | t;
}

static inline int
get94x94 (const codec_u16 *fwd, int c1, int c2)
{
  codec_u16 u = fwd[(c1 - 0x21) * 94 + c2 - 0x21];
  return u == 0xffff ? -1 : int (u);
}

// A character of a 1 byte set. c is 0x20..0x7f (96 sets) or 0x21..0x7e.
static int
get1 (int ccs, int c)
{
  switch (ccs)
    {
    case ccs_usascii:
      return c;

    case ccs_jisx0201_kana:
      return c >= 0x21 && c <= 0x5f ? 0xff61 + (c - 0x21) : -1;

    default:
      {
        int id = sb_of_iso8859 (ccs);
        if (id < 0)
          return -1;
        codec_u16 u = codec_sb[id].fwd[(c | 0x80) - 0x80];
        return u == 0xffff ? -1 : int (u);
      }
    }
}

// A character of a 2 byte set. c1 and c2 are 0x21..0x7e.
static int
get2 (int ccs, int c1, int c2, int vender)
{
  switch (ccs)
    {
    case ccs_jisx0208:
      {
        if (vender == ENCODING_ISO_VENDER_OSFJVC && c1 >= 0x75)
          c1 += 10;
        int s = codec_jis2sjis (c1, c2);
        return codec_cp932_get (s >> 8, s & 255);
      }

    case ccs_jisx0212:
      if (vender == ENCODING_ISO_VENDER_OSFJVC && c1 >= 0x75)
        {
          int s = codec_jis2sjis (c1 + 20, c2);
          return codec_cp932_get (s >> 8, s & 255);
        }
      return get94x94 (codec_jisx0212_fwd, c1, c2);

    case ccs_gb2312:
      return get94x94 (codec_gb2312_fwd, c1, c2);

    case ccs_ksc5601:
      return get94x94 (codec_ksc5601_fwd, c1, c2);

    case ccs_cns11643_1:
      return get94x94 (codec_cns1_fwd, c1, c2);

    case ccs_cns11643_2:
      return get94x94 (codec_cns2_fwd, c1, c2);

    case ccs_big5_1:
    case ccs_big5_2:
      mule_g2b (ccs, c1, c2);
      return big5_get (c1, c2);
    }
  return -1;
}

// ---------------------------------------------------------------------------
// decoder

class iso2022_decoder: public char_decoder
{
  const bool m_noesc;
  const int m_vender;
  u_char m_g[4];
  int m_gl;
  int m_gr;
  int m_ss;                     // -1 or the number of a G set

  struct esc_result
  {
    enum {NOTSEQ, NEED_MORE, DESIGNATE, GL, GR, SS} kind;
    int n;
    int ccs;
    int len;
  };

  static void put_byte (codec_char *&d, int b)
    {*d++ = b >= 0x80 ? codec_raw_byte (u_char (b)) : codec_char (b);}

  static int final94n (int f)
    {
      switch (f)
        {
        case '@':
        case 'B': return ccs_jisx0208;
        case 'D': return ccs_jisx0212;
        case 'A': return ccs_gb2312;
        case 'C': return ccs_ksc5601;
        case 'G': return ccs_cns11643_1;
        case 'H': return ccs_cns11643_2;
        case '0': return ccs_big5_1;
        case '1': return ccs_big5_2;
        }
      return -1;
    }

  static int final94 (int f)
    {
      switch (f)
        {
        case 'B':
        case 'J': return ccs_usascii;
        case 'I': return ccs_jisx0201_kana;
        }
      return -1;
    }

  static int final96 (int f)
    {
      switch (f)
        {
        case 'A': return ccs_iso8859_1;
        case 'B': return ccs_iso8859_2;
        case 'C': return ccs_iso8859_3;
        case 'D': return ccs_iso8859_4;
        case 'F': return ccs_iso8859_7;
        case 'L': return ccs_iso8859_5;
        case 'M': return ccs_iso8859_9;
        case 'V': return ccs_iso8859_10;
        case 'Y': return ccs_iso8859_13;
        }
      return -1;
    }

  void parse_esc (const u_char *p, const u_char *se, bool final, esc_result &r) const
    {
      r.kind = esc_result::NOTSEQ;
      r.len = 1;
      r.n = 0;
      r.ccs = 0;
      int avail = int (se - p);
#define NEED(N) do {if (avail < (N)) {if (!final) r.kind = esc_result::NEED_MORE; return;}} while (0)
      NEED (2);
      switch (p[1])
        {
        case '$':
          NEED (3);
          switch (p[2])
            {
            case '@':
            case 'B':
              r.kind = esc_result::DESIGNATE;
              r.n = 0;
              r.ccs = ccs_jisx0208;
              r.len = 3;
              return;

            case 'A':
              r.kind = esc_result::DESIGNATE;
              r.n = 0;
              r.ccs = ccs_gb2312;
              r.len = 3;
              return;

            case '(':
            case ')':
            case '*':
            case '+':
              {
                NEED (4);
                int ccs = final94n (p[3]);
                if (ccs >= 0)
                  {
                    r.kind = esc_result::DESIGNATE;
                    r.n = p[2] == '(' ? 0 : p[2] == ')' ? 1 : p[2] == '*' ? 2 : 3;
                    r.ccs = ccs;
                    r.len = 4;
                  }
                return;
              }
            }
          return;

        case '(':
        case ')':
        case '*':
        case '+':
          {
            NEED (3);
            int ccs = final94 (p[2]);
            if (ccs >= 0)
              {
                r.kind = esc_result::DESIGNATE;
                r.n = p[1] == '(' ? 0 : p[1] == ')' ? 1 : p[1] == '*' ? 2 : 3;
                r.ccs = ccs;
                r.len = 3;
              }
            return;
          }

        case ',':
        case '-':
        case '.':
        case '/':
          {
            NEED (3);
            int ccs = final96 (p[2]);
            if (ccs >= 0)
              {
                r.kind = esc_result::DESIGNATE;
                r.n = p[1] == ',' ? 0 : p[1] == '-' ? 1 : p[1] == '.' ? 2 : 3;
                r.ccs = ccs;
                r.len = 3;
              }
            return;
          }

        case 'N':
        case 'O':
          {
            int n = p[1] == 'N' ? 2 : 3;
            if (m_g[n] != ccs_invalid)
              {
                r.kind = esc_result::SS;
                r.n = n;
                r.len = 2;
              }
            return;
          }

        case 'n':
        case 'o':
          {
            int n = p[1] == 'n' ? 2 : 3;
            if (m_g[n] != ccs_invalid)
              {
                r.kind = esc_result::GL;
                r.n = n;
                r.len = 2;
              }
            return;
          }

        case '~':
        case '}':
        case '|':
          {
            int n = p[1] == '~' ? 1 : p[1] == '}' ? 2 : 3;
            if (m_g[n] != ccs_invalid)
              {
                r.kind = esc_result::GR;
                r.n = n;
                r.len = 2;
              }
            return;
          }
        }
#undef NEED
    }

  enum conv_status {CONV_OK, CONV_PASS, CONV_UNDEF, CONV_MORE};

  // Reads the character of the set ccs that starts at s (its first byte is
  // c1 without the high bit). CONV_OK: u and len are the result. CONV_PASS:
  // the byte is not a character of the set (a control, a bad second byte):
  // it is itself. CONV_UNDEF: a code that the set does not define (len bytes).
  // CONV_MORE: the second byte is not in the input yet.
  int convert (const u_char *s, const u_char *se, int ccs, int c1, bool final,
               int &u, int &len) const
    {
      len = 1;
      if (set_1byte_p (ccs))
        {
          bool in_range = (set_96_p (ccs) ? c1 >= 0x20 : (c1 > 0x20 && c1 < 0x7f));
          u = in_range ? get1 (ccs, c1) : -1;
          return u >= 0 ? CONV_OK : CONV_PASS;
        }
      if (c1 <= 0x20 || c1 >= 0x7f)
        return CONV_PASS;
      if (s + 1 >= se)
        return final ? CONV_PASS : CONV_MORE;
      // Both bytes of a character have the high bit, or none of them has.
      if ((s[1] & 0x80) != (s[0] & 0x80))
        return CONV_PASS;
      int c2 = s[1] & 127;
      if (c2 <= 0x20 || c2 >= 0x7f)
        return CONV_PASS;
      len = 2;
      u = get2 (ccs, c1, c2, m_vender);
      return u >= 0 ? CONV_OK : CONV_UNDEF;
    }

  // Converts the character at s. It returns the number of bytes that it
  // used, or 0 when it needs more input.
  int to_unicode (const u_char *s, const u_char *se, codec_char *&d,
                  int ccs, int c1, bool final)
    {
      int u, len;
      switch (convert (s, se, ccs, c1, final, u, len))
        {
        case CONV_OK:
          *d++ = u;
          return len;

        case CONV_UNDEF:
          put_byte (d, s[0]);
          put_byte (d, s[1]);
          return 2;

        case CONV_MORE:
          return 0;

        default:
          put_byte (d, s[0]);
          return 1;
        }
    }

public:
  iso2022_decoder (const codec_spec &spec)
    : m_noesc (spec.type == encoding_iso2022_noesc), m_vender (spec.vender),
      m_gl (0), m_gr (1), m_ss (-1)
    {memcpy (m_g, spec.initial, 4);}

  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      const u_char *s = src;
      codec_char *d = dst;
      while (s < se && de - d >= CODEC_DECODE_ROOM)
        {
          int cc = *s;

          if (!m_noesc && cc == ISO_ESC)
            {
              esc_result r;
              parse_esc (s, se, final, r);
              if (r.kind == esc_result::NEED_MORE)
                break;
              m_ss = -1;
              switch (r.kind)
                {
                case esc_result::DESIGNATE:
                  m_g[r.n] = u_char (r.ccs);
                  s += r.len;
                  continue;

                case esc_result::GL:
                  m_gl = r.n;
                  s += r.len;
                  continue;

                case esc_result::GR:
                  m_gr = r.n;
                  s += r.len;
                  continue;

                case esc_result::SS:
                  m_ss = r.n;
                  s += r.len;
                  continue;

                default:
                  break;
                }
              // Not a sequence: ESC is a character.
            }
          else
            {
              if (cc == ISO_SS2 || cc == ISO_SS3)
                {
                  int n = cc == ISO_SS2 ? 2 : 3;
                  if (m_g[n] != ccs_invalid)
                    {
                      // The single shift and the character that follows are
                      // one unit. When the next byte is not a character of
                      // the set, the shift byte itself is the character (so
                      // that the byte is kept) and the next byte is read
                      // again.
                      if (s + 1 >= se)
                        {
                          if (!final)
                            break;
                          put_byte (d, cc);
                          s++;
                          continue;
                        }
                      int u, len;
                      int st = (s[1] & 0x80
                                ? convert (s + 1, se, m_g[n], s[1] & 127, final, u, len)
                                : int (CONV_PASS));
                      if (st == CONV_MORE)
                        break;
                      if (st == CONV_OK)
                        {
                          *d++ = u;
                          s += 1 + len;
                        }
                      else
                        {
                          put_byte (d, cc);
                          s++;
                        }
                      m_ss = -1;
                      continue;
                    }
                }
              else if (!m_noesc && (cc == ISO_SI || cc == ISO_SO))
                {
                  if (m_g[1] != ccs_invalid)
                    {
                      m_gl = cc == ISO_SI ? 0 : 1;
                      s++;
                      continue;
                    }
                }
            }

          int ccs, c1;
          if (m_noesc)
            {
              // EUC: G0 for 7 bit, G1 for 8 bit. A single shift (SS2, SS3)
              // applies to the next character.
              if (m_ss >= 0)
                {
                  ccs = m_g[m_ss];
                  c1 = cc & 127;
                }
              else if (cc < 128)
                {
                  ccs = m_g[0];
                  c1 = cc;
                }
              else
                {
                  ccs = m_g[1];
                  c1 = cc & 127;
                }
            }
          else if (cc < 128)
            {
              ccs = m_ss >= 0 ? m_g[m_ss] : m_g[m_gl];
              c1 = cc;
            }
          else
            {
              ccs = m_ss >= 0 ? m_g[m_ss] : m_g[m_gr];
              if (ccs != ccs_invalid)
                c1 = cc & 127;
              else
                {
                  ccs = ccs_usascii;
                  c1 = cc;
                }
            }
          if (ccs == ccs_invalid || ccs >= ccs_max)
            {
              put_byte (d, cc);
              s++;
              m_ss = -1;
              continue;
            }

          int n = to_unicode (s, se, d, ccs, c1, final);
          if (!n)
            break;              // needs more input
          s += n;
          m_ss = -1;
        }
      src = s;
      dst = d;
    }
};

// ---------------------------------------------------------------------------
// encoder

class iso2022_encoder: public char_encoder
{
  struct ccs_data
  {
    char final;
    char type;                  // 0: 94, 1: 96, 2: 94 x 94
    char fshort;
  };
  static const ccs_data s_ccs_data[ccs_max];

  const bool m_noesc;
  const int m_flags;
  const int m_vender;
  u_char m_initial[4];
  u_int m_designatable[4];
  u_char m_g[4];
  int m_gl;
  bool m_gr_enabled;            // false: 7 bit
  u_char m_designation[ccs_max];
  std::vector <int> m_order;

  bool allowed (int ccs) const
    {
      if (ccs == ccs_usascii)
        return true;
      for (int i = 0; i < 4; i++)
        {
          if (m_initial[i] == ccs)
            return true;
          if (m_noesc)
            continue;
          if (m_designatable[i] == u_int (-1))
            return true;
          if (m_designatable[i] & (1u << ccs))
            return true;
        }
      return false;
    }

  int select_designation (int ccs) const
    {
      if (ccs == ccs_usascii)
        return 0;
      for (int i = 0; i < 4; i++)
        if (m_initial[i] == ccs)
          return i;
      for (int i = 0; i < 4; i++)
        if (m_designatable[i] != u_int (-1) && (m_designatable[i] & (1u << ccs)))
          return i;
      for (int i = 0; i < 4; i++)
        if (m_designatable[i] == u_int (-1))
          return i;
      if (m_flags & ENCODING_ISO_LOCKING_SHIFT)
        return 1;
      if (s_ccs_data[ccs].type == 1)
        return 2;
      return 0;
    }

  void build_order (int lang)
    {
      static const int pref_jp[] =
        {ccs_jisx0201_kana, ccs_jisx0208, ccs_iso8859_1, ccs_iso8859_2,
         ccs_iso8859_3, ccs_iso8859_4, ccs_iso8859_5, ccs_iso8859_7,
         ccs_iso8859_9, ccs_iso8859_10, ccs_iso8859_13, ccs_jisx0212,
         ccs_gb2312, ccs_ksc5601, ccs_cns11643_1, ccs_cns11643_2, ccs_big5_1};
      static const int pref_kr[] =
        {ccs_ksc5601, ccs_jisx0201_kana, ccs_jisx0208, ccs_iso8859_1,
         ccs_iso8859_2, ccs_iso8859_3, ccs_iso8859_4, ccs_iso8859_5,
         ccs_iso8859_7, ccs_iso8859_9, ccs_iso8859_10, ccs_iso8859_13,
         ccs_jisx0212, ccs_gb2312, ccs_cns11643_1, ccs_cns11643_2, ccs_big5_1};
      static const int pref_cn[] =
        {ccs_gb2312, ccs_cns11643_1, ccs_cns11643_2, ccs_jisx0201_kana,
         ccs_jisx0208, ccs_iso8859_1, ccs_iso8859_2, ccs_iso8859_3,
         ccs_iso8859_4, ccs_iso8859_5, ccs_iso8859_7, ccs_iso8859_9,
         ccs_iso8859_10, ccs_iso8859_13, ccs_jisx0212, ccs_ksc5601, ccs_big5_1};

      const int *p;
      int n;
      switch (lang)
        {
        case ENCODING_LANG_KR:
          p = pref_kr;
          n = numberof (pref_kr);
          break;

        case ENCODING_LANG_CN:
        case ENCODING_LANG_CN_GB:
        case ENCODING_LANG_CN_BIG5:
          p = pref_cn;
          n = numberof (pref_cn);
          break;

        default:
          p = pref_jp;
          n = numberof (pref_jp);
          break;
        }
      for (int i = 0; i < n; i++)
        {
          int ccs = p[i];
          if (!allowed (ccs))
            continue;
          if ((ccs == ccs_cns11643_1 || ccs == ccs_cns11643_2)
              && !(m_flags & ENCODING_ISO_USE_CNS11643))
            continue;
          m_order.push_back (ccs);
        }
    }

  // The JIS X 0208 (or JIS X 0212) code of a Unicode, by the vender.
  int find_jis (codec_char u, int &c1, int &c2) const
    {
      int s = codec_cp932_code2 (u);
      if (s < 0)
        return -1;
      int h = s >> 8;
      if (h >= 0xfa && h <= 0xfc)
        {
          if (m_vender == ENCODING_ISO_VENDER_IBMEXT
              || m_vender == ENCODING_ISO_VENDER_OSFJVC)
            {
              int t = s & 255;
              int i = (h - 0xfa) * 188 + (t < 0x7f ? t - 0x40 : t - 0x41);
              if (i < 0 || i >= 388)
                return -1;
              u_int euc = codec_ibm_euc[i];
              c1 = (euc >> 8) & 0x7f;
              c2 = euc & 0x7f;
              int r = euc >= 0x8f0000 ? ccs_jisx0212 : ccs_jisx0208;
              // The table of the vender may give a code that is another
              // character in the sets of JIS (U+FFE4 -> 0x2243 of JIS X 0212,
              // which is U+00A6). It is not used then.
              if (get2 (r, c1, c2, m_vender) == int (u))
                return r;
              if (m_vender == ENCODING_ISO_VENDER_OSFJVC)
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
          if (m_vender != ENCODING_ISO_VENDER_OSFJVC)
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

  static bool find94x94 (const codec_rev &rev, codec_char u, int &c1, int &c2)
    {
      int i = codec_rev_get (rev, u);
      if (i < 0)
        return false;
      c1 = i / 94 + 0x21;
      c2 = i % 94 + 0x21;
      return true;
    }

  // The code of a Unicode in the character set ccs. c2 = -1 for a 1 byte set.
  // The set that is actually used may differ from ccs (JIS X 0212 for an
  // IBM extension character): the result is the set.
  int try_set (int ccs, codec_char u, int &c1, int &c2) const
    {
      c2 = -1;
      switch (ccs)
        {
        case ccs_usascii:
          if (u < 0x80)
            {
              c1 = int (u);
              return ccs;
            }
          return -1;

        case ccs_jisx0201_kana:
          if (u >= 0xff61 && u <= 0xff9f)
            {
              c1 = 0x21 + int (u - 0xff61);
              return ccs;
            }
          return -1;

        case ccs_jisx0208:
          {
            int r = find_jis (u, c1, c2);
            if (r < 0 || !allowed (r))
              return -1;
            return r;
          }

        case ccs_jisx0212:
          return find94x94 (codec_jisx0212_rev, u, c1, c2) ? ccs : -1;

        case ccs_gb2312:
          return find94x94 (codec_gb2312_rev, u, c1, c2) ? ccs : -1;

        case ccs_ksc5601:
          return find94x94 (codec_ksc5601_rev, u, c1, c2) ? ccs : -1;

        case ccs_cns11643_1:
          return find94x94 (codec_cns1_rev, u, c1, c2) ? ccs : -1;

        case ccs_cns11643_2:
          return find94x94 (codec_cns2_rev, u, c1, c2) ? ccs : -1;

        case ccs_big5_1:
          {
            int b = big5_code (u);
            if (b < 0)
              return -1;
            int r;
            c1 = b >> 8;
            c2 = b & 255;
            mule_b2g (r, c1, c2);
            return allowed (r) ? r : -1;
          }

        default:
          if (set_96_p (ccs))
            {
              int id = sb_of_iso8859 (ccs);
              int b = codec_rev_get (*codec_sb[id].rev, u);
              if (b < 0xa0)
                return -1;
              c1 = b & 127;
              return ccs;
            }
          return -1;
        }
    }

  int find (codec_char u, int &c1, int &c2) const
    {
      for (size_t i = 0; i < m_order.size (); i++)
        {
          int r = try_set (m_order[i], u, c1, c2);
          if (r >= 0)
            return r;
        }
      return -1;
    }

  void put_designation (u_char *&d, int n, int ccs)
    {
      static const char inter94[] = {'(', ')', '*', '+'};
      static const char inter96[] = {',', '-', '.', '/'};
      if (m_g[n] == ccs)
        return;
      *d++ = ISO_ESC;
      switch (s_ccs_data[ccs].type)
        {
        case 0:
          *d++ = inter94[n];
          break;

        case 1:
          *d++ = inter96[n];
          break;

        default:
          *d++ = '$';
          if (n || !(m_flags & ENCODING_ISO_SHORT_FORM) || !s_ccs_data[ccs].fshort)
            *d++ = inter94[n];
          break;
        }
      *d++ = s_ccs_data[ccs].final;
      m_g[n] = u_char (ccs);
    }

  // Makes the character set usable and returns the high bit of its bytes.
  int designate (u_char *&d, int ccs)
    {
      int n = m_designation[ccs];
      if (m_g[n] != ccs)
        {
          if (m_gl != 0 && (m_flags & ENCODING_ISO_ASCII_CTRL))
            {
              *d++ = ISO_SI;
              m_gl = 0;
            }
          put_designation (d, n, ccs);
        }

      switch (n)
        {
        default:
          if (m_gl != 0)
            {
              *d++ = ISO_SI;
              m_gl = 0;
            }
          return 0;

        case 1:
          if (m_gr_enabled)
            return 0x80;
          if (m_gl != 1)
            {
              *d++ = ISO_SO;
              m_gl = 1;
            }
          return 0;

        case 2:
          if (m_gr_enabled)
            {
              *d++ = ISO_SS2;
              return 0x80;
            }
          *d++ = ISO_ESC;
          *d++ = 'N';
          return 0;

        case 3:
          if (m_gr_enabled)
            {
              *d++ = ISO_SS3;
              return 0x80;
            }
          *d++ = ISO_ESC;
          *d++ = 'O';
          return 0;
        }
    }

public:
  iso2022_encoder (const codec_spec &spec)
    : m_noesc (spec.type == encoding_iso2022_noesc), m_flags (spec.flags),
      m_vender (spec.vender), m_gl (0),
      m_gr_enabled (!(spec.flags & ENCODING_ISO_7BITS))
    {
      memcpy (m_initial, spec.initial, 4);
      memcpy (m_designatable, spec.designatable, sizeof m_designatable);
      memcpy (m_g, m_initial, 4);
      for (int i = 0; i < ccs_max; i++)
        m_designation[i] = u_char (select_designation (i));
      build_order (spec.lang);
    }

  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      const codec_char *s = src;
      u_char *d = dst;
      while (s < se && de - d >= CODEC_ENCODE_ROOM)
        {
          codec_char u = *s++;
          if (codec_raw_byte_p (u))
            {
              *d++ = u_char (u - CODEC_RAW_BYTE_BASE);
              continue;
            }
          if (u == '\n')
            {
              if (m_flags & ENCODING_ISO_ASCII_EOL)
                {
                  if (m_gl != 0)
                    {
                      *d++ = ISO_SI;
                      m_gl = 0;
                    }
                  for (int i = 0; i < 4; i++)
                    if (m_g[i] != m_initial[i])
                      {
                        if (m_initial[i] != ccs_invalid)
                          put_designation (d, i, m_initial[i]);
                        else
                          m_g[i] = m_initial[i];
                      }
                }
              *d++ = '\n';
              continue;
            }
          if (u < 0x80)
            {
              if ((m_flags & ENCODING_ISO_ASCII_CTRL) || (u > ' ' && u < 0x7f))
                designate (d, ccs_usascii);
              *d++ = u_char (u);
              continue;
            }
          int c1, c2;
          int ccs = find (u, c1, c2);
          if (ccs < 0)
            {
              designate (d, ccs_usascii);
              *d++ = CODEC_DEFCHAR;
              continue;
            }
          int f = designate (d, ccs);
          *d++ = u_char (c1 | f);
          if (c2 >= 0)
            *d++ = u_char (c2 | f);
        }
      src = s;
      dst = d;
    }

  virtual void finish (u_char *&d, u_char *)
    {
      designate (d, ccs_usascii);
    }

  virtual bool can_encode (codec_char u) const
    {
      if (codec_raw_byte_p (u) || u < 0x80)
        return true;
      int c1, c2;
      return find (u, c1, c2) >= 0;
    }
};

const iso2022_encoder::ccs_data iso2022_encoder::s_ccs_data[ccs_max] =
{
  {'B', 0},                     // ccs_usascii
  {'I', 0},                     // ccs_jisx0201_kana
  {'A', 1},                     // ccs_iso8859_1
  {'B', 1},                     // ccs_iso8859_2
  {'C', 1},                     // ccs_iso8859_3
  {'D', 1},                     // ccs_iso8859_4
  {'L', 1},                     // ccs_iso8859_5
  {'F', 1},                     // ccs_iso8859_7
  {'M', 1},                     // ccs_iso8859_9
  {'V', 1},                     // ccs_iso8859_10
  {'Y', 1},                     // ccs_iso8859_13
  {'B', 2, 1},                  // ccs_jisx0208
  {'D', 2},                     // ccs_jisx0212
  {'A', 2, 1},                  // ccs_gb2312
  {'C', 2},                     // ccs_ksc5601
  {'0', 2},                     // ccs_big5_1
  {'1', 2},                     // ccs_big5_2
  {0},                          // ccs_utf16_undef_char_high
  {0},                          // ccs_utf16_undef_char_low
  {0},                          // ccs_utf16_surrogate_high
  {0},                          // ccs_utf16_surrogate_low
  {'G', 2},                     // ccs_cns11643_1
  {'H', 2},                     // ccs_cns11643_2
  {0},                          // ccs_ipa
  {0},                          // ccs_smlcdm
  {0},                          // ccs_georgian
  {0},                          // ccs_ujp
  {0},                          // ccs_ulatin
};

char_decoder *
codec_make_iso2022_decoder (const codec_spec &spec)
{
  return new iso2022_decoder (spec);
}

char_encoder *
codec_make_iso2022_encoder (const codec_spec &spec)
{
  return new iso2022_encoder (spec);
}
