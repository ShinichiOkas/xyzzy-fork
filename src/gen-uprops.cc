// gen-uprops: build the Unicode property tables (uprops) from the UCD files
// tracked in src/unicode/ucd/.  The result is C source for uprops.cc,
// written to the standard output (src/gen/uprops-data.h).
//
// Inputs (read from src/unicode/ucd/, the working directory is src/):
//   UnicodeData.txt     general category, simple case mappings
//   EastAsianWidth.txt  East Asian Width
//   Scripts.txt         script
//   Blocks.txt          block (only to choose the font group)
//   CaseFolding.txt     simple case folding (status C and S)
//
// Every table is a two stage table.  A page holds (1 << shift) values; the
// top table maps (code point >> shift) to a page, and equal pages are shared.
// The page size is chosen per table to minimize its size.

#include "gen-stdafx.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>

#define UCD_DIR "unicode/ucd/"
#define UCD_VERSION "18.0.0"
#define CP_LIMIT 0x110000

// The enumerations below must agree with uprops.h.
static const char *const gc_names[] =
{
  "Lu", "Ll", "Lt", "Lm", "Lo",
  "Mn", "Mc", "Me",
  "Nd", "Nl", "No",
  "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po",
  "Sm", "Sc", "Sk", "So",
  "Zs", "Zl", "Zp",
  "Cc", "Cf", "Cs", "Co", "Cn",
};
enum {GC_Cc = 25, GC_Cf = 26, GC_Cs = 27, GC_Co = 28, GC_Cn = 29,
      GC_Mn = 5, GC_Me = 7, GC_Zl = 23, GC_Zp = 24};

static const char *const ea_names[] = {"N", "A", "F", "H", "Na", "W"};
enum {EA_N, EA_A, EA_F, EA_H, EA_Na, EA_W};

// width classes
enum {WC_ZERO, WC_NARROW, WC_WIDE, WC_AMBIGUOUS};

// font groups (see uprops.h)
enum {FG_ASCII, FG_JP, FG_HAN, FG_HANGUL, FG_GREEK, FG_CYRILLIC,
      FG_GEORGIAN, FG_LATIN, FG_FALLBACK};

struct case_rec
{
  int upper, lower, title, fold;
  bool operator < (const case_rec &x) const
  {
    if (upper != x.upper) return upper < x.upper;
    if (lower != x.lower) return lower < x.lower;
    if (title != x.title) return title < x.title;
    return fold < x.fold;
  }
  bool operator == (const case_rec &x) const
  {
    return upper == x.upper && lower == x.lower
      && title == x.title && fold == x.fold;
  }
};

struct block_t
{
  unsigned lo, hi;
  std::string name;
};

static void
die (const char *msg, const char *file, int line)
{
  fprintf (stderr, "gen-uprops: %s:%d: %s\n", file, line, msg);
  exit (2);
}

static FILE *
open_ucd (const char *name)
{
  std::string path = std::string (UCD_DIR) + name;
  FILE *fp = fopen (path.c_str (), "r");
  if (!fp)
    {
      fprintf (stderr, "gen-uprops: cannot open %s\n", path.c_str ());
      exit (2);
    }
  return fp;
}

// Read a line into buf, strip CR/LF and a trailing comment.  Returns false at EOF.
static bool
read_line (FILE *fp, char *buf, int size)
{
  if (!fgets (buf, size, fp))
    return false;
  char *p = buf + strlen (buf);
  while (p > buf && (p[-1] == '\n' || p[-1] == '\r'))
    *--p = 0;
  char *h = strchr (buf, '#');
  if (h)
    *h = 0;
  return true;
}

static std::string
trim (const std::string &s)
{
  size_t b = 0, e = s.size ();
  while (b < e && (s[b] == ' ' || s[b] == '\t'))
    b++;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t'))
    e--;
  return s.substr (b, e - b);
}

static std::vector<std::string>
split (const std::string &s, char sep)
{
  std::vector<std::string> v;
  size_t b = 0;
  while (1)
    {
      size_t e = s.find (sep, b);
      if (e == std::string::npos)
        {
          v.push_back (trim (s.substr (b)));
          break;
        }
      v.push_back (trim (s.substr (b, e - b)));
      b = e + 1;
    }
  return v;
}

// "0041" or "0041..005A"
static void
parse_range (const std::string &s, unsigned &lo, unsigned &hi,
             const char *file, int line)
{
  size_t d = s.find ("..");
  char *e;
  lo = strtoul (s.c_str (), &e, 16);
  if (e == s.c_str ())
    die ("bad code point", file, line);
  hi = lo;
  if (d != std::string::npos)
    {
      hi = strtoul (s.c_str () + d + 2, &e, 16);
      if (hi < lo || hi >= CP_LIMIT)
        die ("bad range", file, line);
    }
  if (lo >= CP_LIMIT)
    die ("bad code point", file, line);
}

// ---- the data ----

static unsigned char gc_of[CP_LIMIT];
static unsigned char ea_of[CP_LIMIT];
static unsigned char script_of[CP_LIMIT];
static int block_of[CP_LIMIT];
static int upper_of[CP_LIMIT], lower_of[CP_LIMIT], title_of[CP_LIMIT], fold_of[CP_LIMIT];
static std::vector<std::string> script_names;
static std::vector<block_t> blocks;

static int
find_name (const char *const *names, int n, const std::string &s,
           const char *file, int line)
{
  for (int i = 0; i < n; i++)
    if (s == names[i])
      return i;
  die ("unknown value", file, line);
  return -1;
}

static void
load_unicode_data ()
{
  const char *file = "UnicodeData.txt";
  FILE *fp = open_ucd (file);
  memset (gc_of, GC_Cn, sizeof gc_of);
  char buf[2048];
  int line = 0;
  unsigned first = 0;
  bool in_range = false;
  while (read_line (fp, buf, sizeof buf))
    {
      line++;
      if (!*buf)
        continue;
      std::vector<std::string> f = split (buf, ';');
      if (f.size () < 15)
        die ("too few fields", file, line);
      unsigned cp = strtoul (f[0].c_str (), 0, 16);
      if (cp >= CP_LIMIT)
        die ("bad code point", file, line);
      int gc = find_name (gc_names, sizeof gc_names / sizeof *gc_names, f[2], file, line);
      const std::string &name = f[1];
      if (name.size () > 8 && name.compare (name.size () - 8, 8, ", First>") == 0)
        {
          first = cp;
          in_range = true;
          continue;
        }
      if (name.size () > 7 && name.compare (name.size () - 7, 7, ", Last>") == 0)
        {
          if (!in_range)
            die ("Last without First", file, line);
          for (unsigned c = first; c <= cp; c++)
            gc_of[c] = gc;
          in_range = false;
          continue;
        }
      gc_of[cp] = gc;
      if (!f[12].empty ())
        upper_of[cp] = strtoul (f[12].c_str (), 0, 16) - cp;
      if (!f[13].empty ())
        lower_of[cp] = strtoul (f[13].c_str (), 0, 16) - cp;
      if (!f[14].empty ())
        title_of[cp] = strtoul (f[14].c_str (), 0, 16) - cp;
    }
  fclose (fp);
}

static void
load_east_asian_width ()
{
  const char *file = "EastAsianWidth.txt";
  FILE *fp = open_ucd (file);
  memset (ea_of, EA_N, sizeof ea_of);
  char buf[1024];
  int line = 0;
  while (read_line (fp, buf, sizeof buf))
    {
      line++;
      if (!*buf)
        continue;
      std::vector<std::string> f = split (buf, ';');
      if (f.size () < 2)
        die ("too few fields", file, line);
      unsigned lo, hi;
      parse_range (f[0], lo, hi, file, line);
      int ea = find_name (ea_names, sizeof ea_names / sizeof *ea_names, f[1], file, line);
      for (unsigned c = lo; c <= hi; c++)
        ea_of[c] = ea;
    }
  fclose (fp);
}

static void
load_scripts ()
{
  const char *file = "Scripts.txt";
  FILE *fp = open_ucd (file);
  script_names.push_back ("Unknown");
  memset (script_of, 0, sizeof script_of);
  std::map<std::string, int> ids;
  char buf[1024];
  int line = 0;
  while (read_line (fp, buf, sizeof buf))
    {
      line++;
      if (!*buf)
        continue;
      std::vector<std::string> f = split (buf, ';');
      if (f.size () < 2)
        die ("too few fields", file, line);
      unsigned lo, hi;
      parse_range (f[0], lo, hi, file, line);
      int id;
      std::map<std::string, int>::iterator i = ids.find (f[1]);
      if (i == ids.end ())
        {
          id = (int)script_names.size ();
          if (id > 255)
            die ("too many scripts", file, line);
          script_names.push_back (f[1]);
          ids[f[1]] = id;
        }
      else
        id = i->second;
      for (unsigned c = lo; c <= hi; c++)
        script_of[c] = id;
    }
  fclose (fp);
}

static void
load_blocks ()
{
  const char *file = "Blocks.txt";
  FILE *fp = open_ucd (file);
  for (int i = 0; i < CP_LIMIT; i++)
    block_of[i] = -1;
  char buf[1024];
  int line = 0;
  while (read_line (fp, buf, sizeof buf))
    {
      line++;
      if (!*buf)
        continue;
      std::vector<std::string> f = split (buf, ';');
      if (f.size () < 2)
        die ("too few fields", file, line);
      block_t b;
      parse_range (f[0], b.lo, b.hi, file, line);
      b.name = f[1];
      for (unsigned c = b.lo; c <= b.hi; c++)
        block_of[c] = (int)blocks.size ();
      blocks.push_back (b);
    }
  fclose (fp);
}

static void
load_case_folding ()
{
  const char *file = "CaseFolding.txt";
  FILE *fp = open_ucd (file);
  char buf[1024];
  int line = 0;
  while (read_line (fp, buf, sizeof buf))
    {
      line++;
      if (!*buf)
        continue;
      std::vector<std::string> f = split (buf, ';');
      if (f.size () < 3)
        die ("too few fields", file, line);
      if (f[1] != "C" && f[1] != "S")
        continue;
      unsigned cp = strtoul (f[0].c_str (), 0, 16);
      if (cp >= CP_LIMIT)
        die ("bad code point", file, line);
      fold_of[cp] = strtoul (f[2].c_str (), 0, 16) - cp;
    }
  fclose (fp);
}

// ---- derived properties ----

static bool
block_is (unsigned c, const char *name)
{
  return block_of[c] >= 0 && blocks[block_of[c]].name == name;
}

static bool
script_is (unsigned c, const char *name)
{
  return script_names[script_of[c]] == name;
}

static int
width_class (unsigned c)
{
  int gc = gc_of[c];
  // Non-spacing and enclosing marks, format characters and controls take no
  // cell by themselves.  U+00AD (soft hyphen) is drawn as a hyphen in the
  // editor, so it keeps one cell.
  if (gc == GC_Mn || gc == GC_Me || gc == GC_Cf || gc == GC_Cc)
    return c == 0xad ? WC_NARROW : WC_ZERO;
  switch (ea_of[c])
    {
    case EA_W:
    case EA_F:
      return WC_WIDE;
    case EA_A:
      return WC_AMBIGUOUS;
    default:
      return WC_NARROW;
    }
}

static bool
word_char_p (unsigned c)
{
  int gc = gc_of[c];
  return gc <= 4 /* L* */ || (gc >= 5 && gc <= 7) /* M* */
    || (gc >= 8 && gc <= 10) /* N* */;
}

static bool
graphic_p (unsigned c)
{
  int gc = gc_of[c];
  return !(gc == GC_Cc || gc == GC_Cf || gc == GC_Cs || gc == GC_Cn
           || gc == GC_Zl || gc == GC_Zp);
}

static int
font_group (unsigned c)
{
  if (c < 0x80)
    return FG_ASCII;
  if (script_is (c, "Hangul"))
    return FG_HANGUL;
  if (script_is (c, "Hiragana") || script_is (c, "Katakana"))
    return FG_JP;
  static const char *const jp_blocks[] =
    {
      "Hiragana",
      "Katakana",
      "CJK Symbols and Punctuation",
      "Katakana Phonetic Extensions",
      "Halfwidth and Fullwidth Forms",
      "Enclosed CJK Letters and Months",
      "CJK Compatibility",
      "CJK Compatibility Forms",
      "Bopomofo",
      "Bopomofo Extended",
      "Kana Supplement",
      "Kana Extended-A",
      "Kana Extended-B",
      "Small Kana Extension",
      "CJK Strokes",
      "Ideographic Description Characters",
      "Enclosed Ideographic Supplement",
      "Small Form Variants",
      "Vertical Forms",
    };
  for (size_t i = 0; i < sizeof jp_blocks / sizeof *jp_blocks; i++)
    if (block_is (c, jp_blocks[i]))
      return FG_JP;
  if (script_is (c, "Han"))
    return FG_HAN;
  if (script_is (c, "Greek"))
    return FG_GREEK;
  if (block_is (c, "Greek and Coptic") || block_is (c, "Greek Extended"))
    {
      if (script_is (c, "Common") || script_is (c, "Inherited"))
        return FG_GREEK;
    }
  if (script_is (c, "Cyrillic")
      || block_is (c, "Cyrillic") || block_is (c, "Cyrillic Supplement"))
    return FG_CYRILLIC;
  if (script_is (c, "Georgian")
      || block_is (c, "Georgian Supplement") || block_is (c, "Georgian Extended"))
    return FG_GEORGIAN;
  if (script_is (c, "Latin"))
    return FG_LATIN;
  if (block_is (c, "Latin-1 Supplement")
      || block_is (c, "IPA Extensions")
      || block_is (c, "Spacing Modifier Letters")
      || block_is (c, "Combining Diacritical Marks"))
    return FG_LATIN;
  return FG_FALLBACK;
}

// ---- two stage tables ----

struct table
{
  std::string name;
  int elem_size;                 // bytes per value in the page
  int shift;
  std::vector<unsigned short> top;
  std::vector<unsigned> pages;   // pages * (1 << shift) values
  size_t bytes () const
  {
    return top.size () * 2 + pages.size () * elem_size;
  }
};

static void
build (table &t, const std::vector<unsigned> &v, int shift)
{
  t.shift = shift;
  t.top.clear ();
  t.pages.clear ();
  int psize = 1 << shift;
  std::map<std::vector<unsigned>, int> seen;
  for (size_t b = 0; b < v.size (); b += psize)
    {
      std::vector<unsigned> page (v.begin () + b, v.begin () + b + psize);
      std::map<std::vector<unsigned>, int>::iterator i = seen.find (page);
      int idx;
      if (i == seen.end ())
        {
          idx = (int)seen.size ();
          seen[page] = idx;
          t.pages.insert (t.pages.end (), page.begin (), page.end ());
        }
      else
        idx = i->second;
      if (idx > 65535)
        {
          fprintf (stderr, "gen-uprops: too many pages\n");
          exit (2);
        }
      t.top.push_back ((unsigned short)idx);
    }
}

static void
build_best (table &t, const std::string &name, int elem_size,
            const std::vector<unsigned> &v)
{
  t.name = name;
  t.elem_size = elem_size;
  table best;
  best.name = name;
  best.elem_size = elem_size;
  bool have = false;
  for (int shift = 4; shift <= 9; shift++)
    {
      table x;
      x.name = name;
      x.elem_size = elem_size;
      build (x, v, shift);
      if (!have || x.bytes () < best.bytes ())
        {
          best = x;
          have = true;
        }
    }
  t = best;
}

static void
emit (const table &t)
{
  const char *n = t.name.c_str ();
  printf ("#define UP_%s_SHIFT %d\n", n, t.shift);
  printf ("static const u_int16_t up_%s_top[%d] =\n{", n, (int)t.top.size ());
  for (size_t i = 0; i < t.top.size (); i++)
    printf ("%s%d,", i % 16 ? " " : "\n  ", t.top[i]);
  printf ("\n};\n");
  const char *type = t.elem_size == 1 ? "u_char" : "u_int16_t";
  printf ("static const %s up_%s_pages[%d] =\n{", type, n, (int)t.pages.size ());
  for (size_t i = 0; i < t.pages.size (); i++)
    printf ("%s%u,", i % 16 ? " " : "\n  ", t.pages[i]);
  printf ("\n};\n\n");
}

void
gen_uprops (int argc, char **argv)
{
  load_unicode_data ();
  load_east_asian_width ();
  load_scripts ();
  load_blocks ();
  load_case_folding ();

  // The generator and uprops.h keep the same encoding of the flag byte:
  //   bit 0-1 width class, bit 2-5 font group, bit 6 word, bit 7 graphic.
  std::vector<unsigned> flags (CP_LIMIT), gcea (CP_LIMIT), script (CP_LIMIT),
    caseidx (CP_LIMIT);
  std::vector<case_rec> recs;
  std::map<case_rec, int> recids;
  case_rec zero = {0, 0, 0, 0};
  recs.push_back (zero);
  recids[zero] = 0;

  int nwide = 0, namb = 0, nzero = 0;
  for (unsigned c = 0; c < CP_LIMIT; c++)
    {
      int wc = width_class (c);
      int fg = font_group (c);
      flags[c] = wc | (fg << 2) | (word_char_p (c) << 6) | (graphic_p (c) << 7);
      gcea[c] = gc_of[c] | (ea_of[c] << 5);
      script[c] = script_of[c];
      case_rec r = {upper_of[c], lower_of[c], title_of[c], fold_of[c]};
      std::map<case_rec, int>::iterator i = recids.find (r);
      int id;
      if (i == recids.end ())
        {
          id = (int)recs.size ();
          recids[r] = id;
          recs.push_back (r);
        }
      else
        id = i->second;
      caseidx[c] = id;
      nwide += wc == WC_WIDE;
      namb += wc == WC_AMBIGUOUS;
      nzero += wc == WC_ZERO;
    }
  if (recs.size () > 65535)
    {
      fprintf (stderr, "gen-uprops: too many case records\n");
      exit (2);
    }

  table tflags, tgcea, tscript, tcase;
  build_best (tflags, "FLAGS", 1, flags);
  build_best (tgcea, "GCEA", 1, gcea);
  build_best (tscript, "SCRIPT", 1, script);
  build_best (tcase, "CASE", 2, caseidx);

  size_t total = tflags.bytes () + tgcea.bytes () + tscript.bytes ()
    + tcase.bytes () + recs.size () * sizeof (case_rec);

  printf ("/* Generated by gen-uprops from the UCD files in src/unicode/ucd/.  Do not edit. */\n");
  printf ("#define UPROPS_UNICODE_VERSION \"%s\"\n\n", UCD_VERSION);
  emit (tflags);
  emit (tgcea);
  emit (tscript);
  emit (tcase);

  printf ("struct up_case_rec {int upper, lower, title, fold;};\n");
  printf ("static const up_case_rec up_case_recs[%d] =\n{\n", (int)recs.size ());
  for (size_t i = 0; i < recs.size (); i++)
    printf ("  {%d, %d, %d, %d},\n", recs[i].upper, recs[i].lower,
            recs[i].title, recs[i].fold);
  printf ("};\n\n");

  printf ("#define UP_N_SCRIPTS %d\n", (int)script_names.size ());
  printf ("static const char *const up_script_names[UP_N_SCRIPTS] =\n{\n");
  for (size_t i = 0; i < script_names.size (); i++)
    printf ("  \"%s\",\n", script_names[i].c_str ());
  printf ("};\n\n");

  printf ("#define UP_TABLE_BYTES %d\n", (int)total);

  fprintf (stderr,
           "gen-uprops: Unicode %s: flags %d (shift %d), gc/ea %d (shift %d), "
           "script %d (shift %d), case %d (shift %d) + %d case records"
           " = %d bytes; %d scripts; wide %d, ambiguous %d, zero %d\n",
           UCD_VERSION,
           (int)tflags.bytes (), tflags.shift, (int)tgcea.bytes (), tgcea.shift,
           (int)tscript.bytes (), tscript.shift, (int)tcase.bytes (), tcase.shift,
           (int)recs.size (), (int)total, (int)script_names.size (),
           nwide, namb, nzero);
  exit (0);
}
