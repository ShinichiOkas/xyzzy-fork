// gen-codec: builds the conversion tables of codec.cc (the "codec" layer)
// from the mapping tables that are tracked in src/unicode/.
//
// It reads only those files. It does not call MultiByteToWideChar or any
// other OS service, so the result does not depend on the machine that builds
// xyzzy. The output (src/gen/codec-tab.cc) is compiled into xyzzy.exe.
//
// Run from the src/ directory: gen-src1 gen-codec > gen/codec-tab.cc
#include "gen-stdafx.h"
#include <vector>
#include <algorithm>
#include <utility>

#define NONE 0xffff

typedef std::vector<int> ivec;
typedef std::pair<int, int> ipair;
typedef std::vector<ipair> pvec;

static void
fatal (const char *file, int linenum, const char *msg)
{
  fprintf (stderr, "gen-codec: %s:%d: %s\n", file, linenum, msg);
  exit (2);
}

// Reads a mapping table of the Unicode.org style:
//   0xCODE <TAB> 0xUNICODE <TAB> # comment
// Lines whose Unicode column is empty (undefined code) are skipped.
static void
read_map (const char *file, pvec &out)
{
  FILE *fp = fopen (file, "r");
  if (!fp)
    {
      fprintf (stderr, "gen-codec: %s: %s\n", file, strerror (errno));
      exit (2);
    }
  char b[1024];
  for (int linenum = 1; fgets (b, sizeof b, fp); linenum++)
    {
      char *p = b;
      while (*p == ' ' || *p == '\t')
        p++;
      if (!*p || *p == '#' || *p == '\r' || *p == '\n')
        continue;
      if (p[0] != '0' || (p[1] != 'x' && p[1] != 'X'))
        fatal (file, linenum, "invalid format");
      char *e;
      long code = strtol (p + 2, &e, 16);
      while (*e == ' ' || *e == '\t')
        e++;
      if (e[0] != '0' || (e[1] != 'x' && e[1] != 'X'))
        continue;               // undefined
      long u = strtol (e + 2, &e, 16);
      out.push_back (ipair (int (code), int (u)));
    }
  fclose (fp);
}

static void
emit_u16 (const char *name, const ivec &v)
{
  printf ("const codec_u16 %s[%d] =\n{", name, int (v.size ()));
  for (size_t i = 0; i < v.size (); i++)
    {
      if (!(i % 12))
        printf ("\n ");
      printf (" 0x%04x,", v[i] & 0xffff);
    }
  printf ("\n};\n\n");
}

// Reverse table (Unicode -> code) in the two stage form:
// idx[u >> 8] is the block number (or NONE), blk[block * 256 + (u & 255)]
// is the code (or NONE). The first pair of a Unicode wins.
static void
emit_rev (const char *name, const pvec &pairs)
{
  ivec idx (256, NONE);
  ivec blk;
  for (size_t i = 0; i < pairs.size (); i++)
    {
      int u = pairs[i].first;
      int code = pairs[i].second;
      if (u < 0 || u > 0xffff)
        continue;
      if (idx[u >> 8] == NONE)
        {
          idx[u >> 8] = int (blk.size () / 256);
          blk.resize (blk.size () + 256, NONE);
        }
      int &d = blk[idx[u >> 8] * 256 + (u & 255)];
      if (d == NONE)
        d = code;
    }
  if (blk.empty ())
    blk.resize (256, NONE);

  char n[128];
  sprintf (n, "%s_idx", name);
  emit_u16 (n, idx);
  sprintf (n, "%s_blk", name);
  emit_u16 (n, blk);
  printf ("const codec_rev %s = {%s_idx, %s_blk};\n\n", name, name, name);
}

// ---------------------------------------------------------------------------
// cp932

#define CP932_LEADS 60
#define CP932_TRAILS 189        // 0x40..0xfc

static int
cp932_lead_index (int c)
{
  if (c >= 0x81 && c <= 0x9f)
    return c - 0x81;
  if (c >= 0xe0 && c <= 0xfc)
    return c - 0xe0 + 31;
  return -1;
}

// 0: JIS X 0208 (and JIS X 0208 + Windows additions in the same rows)
// 1: NEC row 13
// 2: IBM extension (FA..FC)
// 3: NEC selected IBM extension (ED, EE)
// 4: user defined (F0..F9)
static int
cp932_class (int code)
{
  int h = code >> 8;
  if (h == 0x87)
    return 1;
  if (h >= 0xfa && h <= 0xfc)
    return 2;
  if (h == 0xed || h == 0xee)
    return 3;
  if (h >= 0xf0 && h <= 0xf9)
    return 4;
  return 0;
}

static void
gen_cp932 ()
{
  pvec map;
  read_map ("unicode/CP932.TXT", map);

  ivec fwd (CP932_LEADS * CP932_TRAILS, NONE);
  int n2 = 0;
  for (size_t i = 0; i < map.size (); i++)
    {
      int code = map[i].first;
      if (code < 0x100)
        {
          // The single bytes are handled in codec.cc. Check that the table
          // agrees with what codec.cc assumes.
          int u = map[i].second;
          bool ok = (code < 0x80 ? u == code
                     : code >= 0xa1 && code <= 0xdf ? u == code - 0xa1 + 0xff61
                     : false);
          if (!ok)
            fatal ("unicode/CP932.TXT", 0, "unexpected single byte entry");
          continue;
        }
      int li = cp932_lead_index (code >> 8);
      int t = code & 255;
      if (li < 0 || t < 0x40 || t > 0xfc)
        fatal ("unicode/CP932.TXT", 0, "unexpected double byte entry");
      fwd[li * CP932_TRAILS + t - 0x40] = map[i].second;
      n2++;
    }

  // User defined area F040..F9FC <-> U+E000..U+E757
  for (int h = 0xf0; h <= 0xf9; h++)
    for (int t = 0x40; t <= 0xfc; t++)
      {
        if (t == 0x7f)
          continue;
        int li = cp932_lead_index (h);
        fwd[li * CP932_TRAILS + t - 0x40] =
          0xe000 + (h - 0xf0) * 188 + (t < 0x7f ? t - 0x40 : t - 0x41);
        n2++;
      }
  fprintf (stderr, "gen-codec: cp932: %d double byte codes\n", n2);

  // Reverse. When a Unicode has more than one code, the code is chosen by
  // the class (cp932_class), then by the code. This is the choice that
  // WideCharToMultiByte(932) makes.
  std::vector<ipair> best (0x10000, ipair (-1, 99));
  pvec ned;
  for (int h = 0; h < 256; h++)
    for (int t = 0x40; t <= 0xfc; t++)
      {
        int li = cp932_lead_index (h);
        if (li < 0)
          continue;
        int u = fwd[li * CP932_TRAILS + t - 0x40];
        if (u == NONE)
          continue;
        int code = (h << 8) | t;
        int c = cp932_class (code);
        if (best[u].first < 0 || c < best[u].second)
          best[u] = ipair (code, c);
        if (c == 3)
          ned.push_back (ipair (u, code));
      }
  pvec rev;
  for (int u = 0; u < 0x10000; u++)
    if (best[u].first >= 0)
      rev.push_back (ipair (u, best[u].first));

  emit_u16 ("codec_cp932_fwd", fwd);
  emit_rev ("codec_cp932_rev", rev);
  emit_rev ("codec_cp932_ned_rev", ned);
}

// ---------------------------------------------------------------------------
// IBM extension table (sjis FA40..FC4B <-> EUC-JP)

static void
gen_ibmext ()
{
  const char *file = "unicode/ibmext-sjis-eucjp.txt";
  pvec map;

  {
    FILE *fp = fopen (file, "r");
    if (!fp)
      {
        fprintf (stderr, "gen-codec: %s: %s\n", file, strerror (errno));
        exit (2);
      }
    char b[256];
    for (int linenum = 1; fgets (b, sizeof b, fp); linenum++)
      {
        if (b[0] != '0')
          continue;
        char *e;
        long s = strtol (b + 2, &e, 16);
        while (*e == ' ' || *e == '\t')
          e++;
        long x = strtol (e + 2, 0, 16);
        map.push_back (ipair (int (s), int (x)));
        (void)linenum;
      }
    fclose (fp);
  }
  if (map.size () != 388)
    fatal (file, 0, "expected 388 entries");
  ivec v (388);
  // the table is in cp932 order: FA40..FC4B
  for (int i = 0; i < 388; i++)
    {
      int s = map[i].first;
      int li = (s >> 8) - 0xfa;
      int t = s & 255;
      int expect = li * 188 + (t < 0x7f ? t - 0x40 : t - 0x41);
      if (li < 0 || li > 2 || expect != i)
        fatal (file, 0, "the table is not in cp932 order");
      v[i] = map[i].second;     // may be > 16 bits: EUC 0x8Fxxxx
    }
  printf ("const unsigned int codec_ibm_euc[388] =\n{");
  for (int i = 0; i < 388; i++)
    {
      if (!(i % 8))
        printf ("\n ");
      printf (" 0x%06x,", v[i]);
    }
  printf ("\n};\n\n");
}

// ---------------------------------------------------------------------------
// 94 x 94 character sets

static void
read_94x94 (const char *file, ivec &fwd, int plane = 0)
{
  pvec map;
  read_map (file, map);
  fwd.assign (94 * 94, NONE);
  for (size_t i = 0; i < map.size (); i++)
    {
      int code = map[i].first;
      if (plane)
        {
          if (code >> 16 != plane)
            continue;
          code &= 0xffff;
        }
      int c1 = code >> 8;
      int c2 = code & 255;
      if (c1 >= 0xa1 && c2 >= 0xa1)
        {
          if (c1 >= 0xa1 && c1 <= 0xfe && c2 >= 0xa1 && c2 <= 0xfe)
            {
              c1 -= 0x80;
              c2 -= 0x80;
            }
          else
            continue;           // e.g. the Unified Hangul extension of KSC5601.TXT
        }
      if (c1 < 0x21 || c1 > 0x7e || c2 < 0x21 || c2 > 0x7e)
        continue;
      fwd[(c1 - 0x21) * 94 + c2 - 0x21] = map[i].second;
    }
}

static void
rev_94x94 (const ivec &fwd, pvec &rev)
{
  for (size_t i = 0; i < fwd.size (); i++)
    if (fwd[i] != NONE)
      rev.push_back (ipair (fwd[i], int (i)));
}

static void
gen_94x94 (const char *name, const ivec &fwd)
{
  char n[128];
  sprintf (n, "codec_%s_fwd", name);
  emit_u16 (n, fwd);
  pvec rev;
  rev_94x94 (fwd, rev);
  sprintf (n, "codec_%s_rev", name);
  emit_rev (n, rev);
}

static void
gen_jisx0212 ()
{
  ivec fwd;
  read_94x94 ("unicode/JIS0212.TXT", fwd);

  // eucJP-ibmext.txt: the IBM extension characters that have a place in
  // rows 83 and 84 of JIS X 0212 (EUC-JP 0x8FF3xx, 0x8FF4xx).
  pvec map;
  read_map ("unicode/eucJP-ibmext.txt", map);
  for (size_t i = 0; i < map.size (); i++)
    {
      int code = map[i].first - 0x8f0000;
      int c1 = (code >> 8) - 0x80;
      int c2 = (code & 255) - 0x80;
      if (c1 < 0x21 || c1 > 0x7e || c2 < 0x21 || c2 > 0x7e)
        fatal ("unicode/eucJP-ibmext.txt", 0, "unexpected code");
      fwd[(c1 - 0x21) * 94 + c2 - 0x21] = map[i].second;
    }
  gen_94x94 ("jisx0212", fwd);
}

// ---------------------------------------------------------------------------
// Big5. fwd index: (lead - 0xa1) * 157 + trail index
// trail index: 0x40..0x7e -> 0..62, 0xa1..0xfe -> 63..156

static int
big5_index (int code)
{
  int h = code >> 8, t = code & 255;
  return (h - 0xa1) * 157 + (t <= 0x7e ? t - 0x40 : t - 0xa1 + 63);
}

static void
gen_big5 ()
{
  pvec map;
  read_map ("unicode/BIG5.TXT", map);
  ivec fwd (89 * 157, NONE);
  pvec rev;
  for (size_t i = 0; i < map.size (); i++)
    {
      int code = map[i].first;
      int h = code >> 8;
      int t = code & 255;
      int ti;
      if (t >= 0x40 && t <= 0x7e)
        ti = t - 0x40;
      else if (t >= 0xa1 && t <= 0xfe)
        ti = t - 0xa1 + 63;
      else
        fatal ("unicode/BIG5.TXT", 0, "unexpected trail byte");
      if (h < 0xa1 || h > 0xf9)
        fatal ("unicode/BIG5.TXT", 0, "unexpected lead byte");
      fwd[(h - 0xa1) * 157 + ti] = map[i].second;
    }
  // BIG5.TXT has no mapping (U+FFFD) for the places where Big5 has a
  // duplicate of another character. They are patched as the table of the
  // old xyzzy did (gen-ucs2tab.cc, read_big5): the duplicates are read
  // but never written.
  struct {int code, from;} dup[] =
    {
      {0xa15a, 0xa1c4}, {0xa1fe, 0xa2ac}, {0xa240, 0xa2ad},
      {0xa2cc, 0xa451}, {0xa2ce, 0xa4ca},
    };
  ivec decode_only (fwd.size (), 0);
  for (size_t i = 0; i < sizeof dup / sizeof dup[0]; i++)
    {
      int d = big5_index (dup[i].code);
      fwd[d] = fwd[big5_index (dup[i].from)];
      decode_only[d] = 1;
    }
  fwd[big5_index (0xa1c3)] = 0xffe3;
  fwd[big5_index (0xa1c5)] = 0x02cd;
  for (size_t i = 0; i < fwd.size (); i++)
    if (fwd[i] == 0xfffd)
      fwd[i] = NONE;
  rev.clear ();
  for (size_t i = 0; i < fwd.size (); i++)
    if (fwd[i] != NONE && !decode_only[i])
      rev.push_back (ipair (fwd[i], int (i)));
  emit_u16 ("codec_big5_fwd", fwd);
  emit_rev ("codec_big5_rev", rev);
}

// ---------------------------------------------------------------------------
// 1 byte character sets

struct sb_def
{
  const char *file;
  int c1_identity;              // 0x80..0x9f are the C1 controls (ISO 8859)
};

static const sb_def sb_defs[] =
{
  {"unicode/8859-1.TXT", 1},
  {"unicode/8859-2.TXT", 1},
  {"unicode/8859-3.TXT", 1},
  {"unicode/8859-4.TXT", 1},
  {"unicode/8859-5.TXT", 1},
  {"unicode/8859-7.TXT", 1},
  {"unicode/8859-9.TXT", 1},
  {"unicode/8859-10.TXT", 1},
  {"unicode/8859-13.TXT", 1},
  {"unicode/CP1250.TXT", 0},
  {"unicode/CP1251.TXT", 0},
  {"unicode/CP1252.TXT", 0},
  {"unicode/CP1253.TXT", 0},
  {"unicode/CP1254.TXT", 0},
  {"unicode/CP1257.TXT", 0},
  {"unicode/KOI8-R.TXT", 0},
  {"unicode/KOI8-U.TXT", 0},
};

static void
gen_sb ()
{
  int n = sizeof sb_defs / sizeof sb_defs[0];
  for (int k = 0; k < n; k++)
    {
      pvec map;
      read_map (sb_defs[k].file, map);
      ivec fwd (128, NONE);
      for (size_t i = 0; i < map.size (); i++)
        {
          int b = map[i].first;
          if (b < 0x80 || b > 0xff)
            continue;
          fwd[b - 0x80] = map[i].second;
        }
      if (sb_defs[k].c1_identity)
        for (int b = 0x80; b < 0xa0; b++)
          fwd[b - 0x80] = b;
      pvec rev;
      for (int i = 0; i < 128; i++)
        if (fwd[i] != NONE)
          rev.push_back (ipair (fwd[i], i + 0x80));
      char nm[64];
      sprintf (nm, "codec_sb%d_fwd", k);
      emit_u16 (nm, fwd);
      sprintf (nm, "codec_sb%d_rev", k);
      emit_rev (nm, rev);
    }
  printf ("const codec_sb_table codec_sb[%d] =\n{\n", n);
  for (int k = 0; k < n; k++)
    printf ("  {codec_sb%d_fwd, &codec_sb%d_rev},   // %s\n", k, k, sb_defs[k].file);
  printf ("};\n\n");
}

void
gen_codec (int argc, char **argv)
{
  printf ("// Generated by gen-codec. Do not edit.\n");
  printf ("#include \"../codec-tab.h\"\n\n");

  gen_cp932 ();
  gen_ibmext ();
  gen_jisx0212 ();

  ivec fwd;
  read_94x94 ("unicode/GB2312.TXT", fwd);
  gen_94x94 ("gb2312", fwd);
  read_94x94 ("unicode/KSC5601.TXT", fwd);
  gen_94x94 ("ksc5601", fwd);
  read_94x94 ("unicode/CNS11643.TXT", fwd, 1);
  gen_94x94 ("cns1", fwd);
  read_94x94 ("unicode/CNS11643.TXT", fwd, 2);
  gen_94x94 ("cns2", fwd);

  gen_big5 ();
  gen_sb ();
  exit (0);
}
