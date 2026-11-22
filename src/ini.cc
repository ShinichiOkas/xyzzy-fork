// The reader and the writer of ini files. See ini.h.
//
// A file is read as a whole into lines of code points (std::u32string), and
// a write changes the lines and writes the whole file again. The last
// version of the file that was read or written is kept (the one file that
// is used: xyzzy.ini) and is read again when its size or its time changes.
#include "stdafx.h"
#include "ed.h"
#include "cdecl.h"
#include "codec.h"
#include "wfile.h"
#include "ini.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{

typedef std::u32string ustr;

struct line
{
  ustr body;
  ustr eol;                     // "" only for the last line of a file
};

typedef std::vector <line> lines_t;

// ---------------------------------------------------------------------
// codec

// The memory of the duplicated codes of cp932 (codec.h). A cp932 file and
// the narrow strings of the program share it, so that a code is kept.
static dup_memory dupmem;

static codec_spec
make_spec (encoding_type type, int flags = 0)
{
  codec_spec s;
  memset (&s, 0, sizeof s);
  s.type = type;
  s.flags = flags;
  return s;
}

static codec_spec
spec_cp932 ()
{
  return make_spec (encoding_sjis);
}

static codec_spec
spec_utf8 (bool sig)
{
  return make_spec (encoding_utf8,
                    ENCODING_UTF_WINDOWS | (sig ? ENCODING_UTF_SIGNATURE : 0));
}

static codec_spec
spec_utf16 ()
{
  // Used for a file with a byte order mark: the decoder takes the byte order
  // from the mark.
  return make_spec (encoding_utf16, ENCODING_UTF_SIGNATURE | ENCODING_UTF_WINDOWS);
}

// The encoding of the narrow strings of the program (char *): cp932 now.
// This and the file encodings are the only places that know the encodings.
static codec_spec
ini_narrow_spec ()
{
  return spec_cp932 ();
}

// The encoding that a file is written in.
// mode: INI_WRITE_DEFAULT is by XYZZY_CHAR_UNICODE.
static codec_spec
ini_write_spec (int mode)
{
  // (Not "if constexpr": the project is built as C++14, where it is warning
  // C4984. Both sides are compiled either way.)
  if (mode == INI_WRITE_DEFAULT)
    mode = XYZZY_CHAR_UNICODE ? INI_WRITE_UTF8_BOM : INI_WRITE_CP932;
  if (mode == INI_WRITE_UTF8_BOM)
    return spec_utf8 (true);
  return spec_cp932 ();
}

static void
decode_bytes (const codec_spec &spec, const u_char *p, size_t n, ustr &out)
{
  out.clear ();
  codec_context *ctx = spec.type == encoding_sjis ? &dupmem : 0;
  std::unique_ptr <char_decoder> d (make_decoder (spec, ctx));
  if (!d)
    return;
  std::vector <codec_char> v;
  codec_decode_all (d.get (), p, n, v);
  out.assign (v.begin (), v.end ());
}

static void
encode_points (const codec_spec &spec, const ustr &s, std::vector <u_char> &out)
{
  codec_context *ctx = spec.type == encoding_sjis ? &dupmem : 0;
  std::unique_ptr <char_encoder> e (make_encoder (spec, ctx));
  if (!e)
    return;
  std::vector <codec_char> v (s.begin (), s.end ());
  codec_encode_all (e.get (), v.empty () ? 0 : &v[0], v.size (), out);
}

static void
narrow_to_ustr (const char *s, ustr &out)
{
  decode_bytes (ini_narrow_spec (), (const u_char *)s, strlen (s), out);
}

static void
ustr_to_narrow (const ustr &s, std::string &out)
{
  std::vector <u_char> v;
  encode_points (ini_narrow_spec (), s, v);
  out.assign (v.begin (), v.end ());
}

// A file: a byte order mark tells UTF-8 and UTF-16. Else cp932.
static void
decode_file (const std::vector <u_char> &data, ustr &text)
{
  const u_char *p = data.empty () ? 0 : &data[0];
  size_t n = data.size ();
  if (n >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf)
    decode_bytes (spec_utf8 (true), p, n, text);
  else if (n >= 2 && ((p[0] == 0xff && p[1] == 0xfe)
                      || (p[0] == 0xfe && p[1] == 0xff)))
    decode_bytes (spec_utf16 (), p, n, text);
  else
    decode_bytes (spec_cp932 (), p, n, text);
}

// ---------------------------------------------------------------------
// lines

static void
split_lines (const ustr &t, lines_t &ls)
{
  ls.clear ();
  size_t n = t.size ();
  size_t i = 0;
  while (i < n)
    {
      size_t j = i;
      while (j < n && t[j] != U'\r' && t[j] != U'\n')
        j++;
      line l;
      l.body = t.substr (i, j - i);
      if (j < n)
        {
          if (t[j] == U'\r' && j + 1 < n && t[j + 1] == U'\n')
            {
              l.eol = U"\r\n";
              j += 2;
            }
          else
            {
              l.eol = ustr (1, t[j]);
              j++;
            }
        }
      ls.push_back (l);
      i = j;
    }
}

static const char32_t *const DEFAULT_EOL = U"\r\n";

static void
join_lines (const lines_t &ls, ustr &t)
{
  t.clear ();
  for (size_t i = 0; i < ls.size (); i++)
    {
      t += ls[i].body;
      t += ls[i].eol.empty () ? ustr (DEFAULT_EOL) : ls[i].eol;
    }
}

static inline bool
ws_p (char32_t c)
{
  return c == U' ' || c == U'\t' || c == 0x0b || c == 0x0c;
}

static ustr
trim (const ustr &s)
{
  size_t b = 0, e = s.size ();
  while (b < e && ws_p (s[b]))
    b++;
  while (e > b && ws_p (s[e - 1]))
    e--;
  return s.substr (b, e - b);
}

static char32_t
fold (char32_t c)
{
  if (c < 0x80)
    return c >= U'A' && c <= U'Z' ? c + 32 : c;
  if (c < 0x10000)
    return char32_t (UINT_PTR (CharLowerW (LPWSTR (UINT_PTR (c)))));
  return c;
}

static bool
equal_ci (const ustr &a, const ustr &b)
{
  if (a.size () != b.size ())
    return false;
  for (size_t i = 0; i < a.size (); i++)
    if (a[i] != b[i] && fold (a[i]) != fold (b[i]))
      return false;
  return true;
}

enum line_kind {LK_BLANK, LK_COMMENT, LK_SECTION, LK_KEY, LK_OTHER};

struct parsed
{
  line_kind kind;
  ustr name;                    // the section or the key (trimmed)
  size_t eq;                    // the index of '=' (LK_KEY)
  size_t vbeg, vend;            // the value without white space (LK_KEY)
  parsed () : kind (LK_BLANK), eq (0), vbeg (0), vend (0) {}
};

static void
parse_line (const ustr &b, parsed &p)
{
  p = parsed ();
  size_t i = 0;
  while (i < b.size () && ws_p (b[i]))
    i++;
  if (i == b.size ())
    return;
  if (b[i] == U';')
    {
      p.kind = LK_COMMENT;
      return;
    }
  if (b[i] == U'[')
    {
      size_t e = b.find (U']', i + 1);
      if (e == ustr::npos)
        e = b.size ();
      p.kind = LK_SECTION;
      p.name = trim (b.substr (i + 1, e - (i + 1)));
      return;
    }
  size_t eq = b.find (U'=', i);
  if (eq == ustr::npos)
    {
      p.kind = LK_OTHER;
      return;
    }
  p.kind = LK_KEY;
  p.name = trim (b.substr (i, eq - i));
  p.eq = eq;
  size_t vb = eq + 1;
  while (vb < b.size () && ws_p (b[vb]))
    vb++;
  size_t ve = b.size ();
  while (ve > vb && ws_p (b[ve - 1]))
    ve--;
  p.vbeg = vb;
  p.vend = ve;
}

static bool
section_header_p (const line &l)
{
  size_t i = 0;
  while (i < l.body.size () && ws_p (l.body[i]))
    i++;
  return i < l.body.size () && l.body[i] == U'[';
}

// The index of the header of the first section of the name, or npos.
static size_t
find_section (const lines_t &ls, const ustr &sec)
{
  for (size_t i = 0; i < ls.size (); i++)
    if (section_header_p (ls[i]))
      {
        parsed p;
        parse_line (ls[i].body, p);
        if (equal_ci (p.name, sec))
          return i;
      }
  return ustr::npos;
}

// In the section whose header is at s: the index of the first line of the
// key (npos if there is no such key). *last is set to the index of the last
// key line of the section (the header if there is no key line).
static size_t
find_key (const lines_t &ls, size_t s, const ustr &key, size_t *last)
{
  size_t found = ustr::npos;
  size_t l = s;
  for (size_t i = s + 1; i < ls.size () && !section_header_p (ls[i]); i++)
    {
      parsed p;
      parse_line (ls[i].body, p);
      if (p.kind != LK_KEY)
        continue;
      l = i;
      if (found == ustr::npos && equal_ci (p.name, key))
        found = i;
    }
  if (last)
    *last = l;
  return found;
}

static ustr
unquote (const ustr &v)
{
  if (v.size () >= 2 && (v[0] == U'"' || v[0] == U'\'') && v[v.size () - 1] == v[0])
    return v.substr (1, v.size () - 2);
  return v;
}

static bool
lookup (const lines_t &ls, const ustr &sec, const ustr &key, ustr &val)
{
  size_t s = find_section (ls, sec);
  if (s == ustr::npos)
    return false;
  size_t i = find_key (ls, s, key, 0);
  if (i == ustr::npos)
    return false;
  parsed p;
  parse_line (ls[i].body, p);
  val = unquote (ls[i].body.substr (p.vbeg, p.vend - p.vbeg));
  return true;
}

static line
make_line (const ustr &body)
{
  line l;
  l.body = body;
  l.eol = DEFAULT_EOL;
  return l;
}

// Changes the lines. Returns true if something is changed.
// key == 0: delete the section; val == 0: delete the key.
static bool
modify (lines_t &ls, const ustr &sec, const ustr *key, const ustr *val)
{
  size_t s = find_section (ls, sec);

  if (!key)
    {
      if (s == ustr::npos)
        return false;
      size_t last;
      find_key (ls, s, ustr (), &last);
      ls.erase (ls.begin () + s, ls.begin () + last + 1);
      return true;
    }

  if (!val)
    {
      if (s == ustr::npos)
        return false;
      size_t i = find_key (ls, s, *key, 0);
      if (i == ustr::npos)
        return false;
      ls.erase (ls.begin () + i);
      return true;
    }

  if (s == ustr::npos)
    {
      if (!ls.empty () && ls.back ().eol.empty ())
        ls.back ().eol = DEFAULT_EOL;
      ls.push_back (make_line (ustr (U"[") + sec + U"]"));
      ls.push_back (make_line (*key + U"=" + *val));
      return true;
    }

  size_t last;
  size_t i = find_key (ls, s, *key, &last);
  if (i == ustr::npos)
    {
      ls.insert (ls.begin () + last + 1, make_line (*key + U"=" + *val));
      return true;
    }

  // The value is replaced in place. The white space before the value is
  // dropped; the white space after it stays.
  parsed p;
  parse_line (ls[i].body, p);
  const ustr &old = ls[i].body;
  ustr nb = old.substr (0, p.eq + 1) + *val;
  if (p.vbeg != p.vend)
    nb += old.substr (p.vend);
  if (nb == old)
    return false;
  ls[i].body = nb;
  return true;
}

// ---------------------------------------------------------------------
// the file

struct file_cache
{
  bool valid;
  std::wstring path;
  bool exists;
  FILETIME mtime;
  DWORD size_low, size_high;
  lines_t lines;
  file_cache () : valid (false), exists (false), size_low (0), size_high (0)
    {mtime.dwLowDateTime = mtime.dwHighDateTime = 0;}
};

static file_cache cache;
static std::mutex cache_lock;

static bool
stat_file (const wfile::path &p, bool &exists, FILETIME &mtime,
           DWORD &size_low, DWORD &size_high)
{
  WIN32_FILE_ATTRIBUTE_DATA d;
  exists = GetFileAttributesExW (p, GetFileExInfoStandard, &d) != 0;
  if (exists)
    {
      mtime = d.ftLastWriteTime;
      size_low = d.nFileSizeLow;
      size_high = d.nFileSizeHigh;
    }
  else
    {
      mtime.dwLowDateTime = mtime.dwHighDateTime = 0;
      size_low = size_high = 0;
    }
  return true;
}

// Opens a file. Another xyzzy may be writing it (the writer holds it
// exclusively for a moment): a sharing violation is tried again a few times.
static HANDLE
open_retry (const wfile::path &p, DWORD access, DWORD share, DWORD disposition)
{
  for (int i = 0;; i++)
    {
      HANDLE h = wfile::create_file (p, access, share, 0, disposition,
                                     FILE_ATTRIBUTE_NORMAL, 0);
      if (h != INVALID_HANDLE_VALUE)
        return h;
      DWORD e = GetLastError ();
      if (i >= 20 || (e != ERROR_SHARING_VIOLATION && e != ERROR_LOCK_VIOLATION))
        return h;
      Sleep (25);
    }
}

// false: the file could not be read as a whole.
static bool
read_all (const wfile::path &p, std::vector <u_char> &data)
{
  data.clear ();
  HANDLE h = open_retry (p, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         OPEN_EXISTING);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  int64_t size;
  bool ok = wfile::get_file_size (h, &size) && size >= 0 && size < (1 << 28);
  if (ok && size)
    {
      data.resize (size_t (size));
      size_t done = 0;
      while (done < data.size ())
        {
          DWORD n;
          if (!ReadFile (h, &data[done], DWORD (data.size () - done), &n, 0) || !n)
            break;
          done += n;
        }
      ok = done == data.size ();
      data.resize (done);
    }
  CloseHandle (h);
  return ok;
}

// The lines of the file (none if there is no file). The result lives in the
// cache. It returns 0 if the file exists but cannot be read: nothing is
// cached then, and a write must not go on (it would replace the file with
// the lines of this write only).
static lines_t *
load (const wfile::path &p)
{
  bool ex;
  FILETIME mt;
  DWORD sl, sh;
  stat_file (p, ex, mt, sl, sh);
  if (cache.valid && cache.path == p.c_str () && cache.exists == ex
      && (!ex || (!CompareFileTime (&cache.mtime, &mt)
                  && cache.size_low == sl && cache.size_high == sh)))
    return &cache.lines;

  cache.valid = false;
  cache.lines.clear ();
  std::vector <u_char> data;
  if (ex && !read_all (p, data))
    return 0;
  ustr text;
  decode_file (data, text);
  split_lines (text, cache.lines);
  cache.path = p.c_str ();
  cache.exists = ex;
  cache.mtime = mt;
  cache.size_low = sl;
  cache.size_high = sh;
  cache.valid = true;
  return &cache.lines;
}

static bool
write_all (const wfile::path &p, const std::vector <u_char> &data)
{
  // Opened without truncating, so that the attributes and the links of the
  // file stay; the length is set at the end. Nobody reads it meanwhile.
  HANDLE h = open_retry (p, GENERIC_WRITE, 0, OPEN_ALWAYS);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  bool ok = true;
  size_t done = 0;
  while (done < data.size ())
    {
      DWORD n;
      if (!WriteFile (h, &data[done], DWORD (data.size () - done), &n, 0) || !n)
        {
          ok = false;
          break;
        }
      done += n;
    }
  if (ok)
    ok = SetEndOfFile (h) != 0;
  CloseHandle (h);
  return ok;
}

static bool
save (const wfile::path &p, lines_t &ls, int mode)
{
  ustr text;
  join_lines (ls, text);
  std::vector <u_char> data;
  encode_points (ini_write_spec (mode), text, data);
  cache.valid = false;
  if (!write_all (p, data))
    return false;
  // The lines are now the ones with an end of line.
  for (size_t i = 0; i < ls.size (); i++)
    if (ls[i].eol.empty ())
      ls[i].eol = DEFAULT_EOL;
  cache.lines = ls;
  bool ex;
  stat_file (p, ex, cache.mtime, cache.size_low, cache.size_high);
  cache.path = p.c_str ();
  cache.exists = ex;
  cache.valid = ex;
  return true;
}

} // namespace

// ---------------------------------------------------------------------
// the interface

int
ini_read (const char *file, const char *section, const char *key,
          char *buf, int size, int *found)
{
  if (found)
    *found = 0;
  if (size <= 0)
    return 0;
  *buf = 0;
  if (!file || !section || !key)
    return 0;
  wfile::path p (file);
  if (!p.ok ())
    return 0;

  std::lock_guard <std::mutex> g (cache_lock);
  ustr sec, k, val;
  narrow_to_ustr (section, sec);
  narrow_to_ustr (key, k);
  lines_t *ls = load (p);
  if (!ls || !lookup (*ls, trim (sec), trim (k), val))
    return 0;
  std::string nb;
  ustr_to_narrow (val, nb);
  size_t n = nb.size ();
  if (n > size_t (size - 1))
    n = size_t (size - 1);
  memcpy (buf, nb.data (), n);
  buf[n] = 0;
  if (found)
    *found = 1;
  return int (n);
}

int
ini_write (const char *file, const char *section, const char *key,
           const char *value, int mode)
{
  if (!file || !section)
    return 0;
  wfile::path p (file);
  if (!p.ok ())
    return 0;

  std::lock_guard <std::mutex> g (cache_lock);
  ustr sec, k, v;
  narrow_to_ustr (section, sec);
  sec = trim (sec);
  if (key)
    {
      narrow_to_ustr (key, k);
      k = trim (k);
    }
  if (value)
    narrow_to_ustr (value, v);

  lines_t *cur = load (p);
  if (!cur)
    return 0;
  lines_t ls = *cur;
  if (!modify (ls, sec, key ? &k : 0, key && value ? &v : 0))
    return 1;
  return save (p, ls, mode) ? 1 : 0;
}

// ---------------------------------------------------------------------
// Lisp: the user interface of this layer for the tests, which work on
// any file (they do not touch the settings of the user).
//
//   (si:ini-read file section key) => string, or nil if there is no such key
//   (si:ini-write file section key value [mode])
//       value nil: delete the key. key nil: delete the section.
//       mode: nil (by XYZZY_CHAR_UNICODE), 0 (cp932) or 1 (UTF-8 with a
//       byte order mark): the encoding that the file is written in. The tests
//       use it to check the 1 side on the build of the 0 side.
//       => t if written

static char *
lisp_narrow (lisp x, std::vector <char> &buf)
{
  check_string (x);
  buf.resize (w2sl (x) + 1);
  w2s (&buf[0], x);
  return &buf[0];
}

lisp
Fsi_ini_read (lisp file, lisp section, lisp key)
{
  std::vector <char> f, s, k;
  lisp_narrow (file, f);
  lisp_narrow (section, s);
  lisp_narrow (key, k);
  std::vector <char> out (0x10000);
  int found;
  ini_read (&f[0], &s[0], &k[0], &out[0], int (out.size ()), &found);
  if (!found)
    return Qnil;
  return make_string (&out[0]);
}

lisp
Fsi_ini_write (lisp file, lisp section, lisp key, lisp value, lisp mode)
{
  int m = INI_WRITE_DEFAULT;
  if (mode && mode != Qnil)
    {
      m = int (fixnum_value (mode));
      if (m != INI_WRITE_CP932 && m != INI_WRITE_UTF8_BOM)
        FErange_error (mode);
    }
  std::vector <char> f, s, k, v;
  lisp_narrow (file, f);
  lisp_narrow (section, s);
  if (key != Qnil)
    lisp_narrow (key, k);
  if (value != Qnil)
    lisp_narrow (value, v);
  return boole (ini_write (&f[0], &s[0], key == Qnil ? 0 : &k[0],
                           value == Qnil ? 0 : &v[0], m));
}
