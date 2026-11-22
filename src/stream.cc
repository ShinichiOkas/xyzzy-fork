#include "stdafx.h"
#include "ed.h"
#include "sequence.h"
#include "wstream.h"
#include "sockinet.h"
#include "sockssl.h"
#include "debug.h"
#include "codec.h"

// ---------------------------------------------------------------------------
// The converter of a file stream and a socket stream
// (docs/refactoring/s4-char-design.md 4.2, 4.4, 4.5)
//
// A stream turns the bytes into Chars with a char_decoder and back with a
// char_encoder.  The line end (CRLF <-> LF) is converted here, on the Chars,
// so that a converter knows only bytes and characters.
//
// XYZZY_CHAR_UNICODE 0: a Char is the internal code.  The raw cp932 form that
// the streams have always had (a lead byte and a trail byte make one Char) is
// one converter (raw_sjis_decoder, raw_sjis_encoder) and gives the same bytes
// and the same values as before.  The other converters give Unicode, which is
// turned into the internal code (codec_to_chars) and back (chars_to_codec).
// XYZZY_CHAR_UNICODE 1: a Char is a Unicode code point (or a raw byte
// character), which is what the converters give.

enum
{
  CONV_IB = 16,                 // bytes that wait for the rest of a sequence
  CONV_CB = 48,                 // room for a decoder (> CODEC_DECODE_ROOM)
  CONV_OB = CONV_CB * 2,        // Chars of the decoded characters
  CONV_PIECE = 128,             // characters that are encoded at once
  CONV_EOB = 512                // bytes of the output of an encoder
};

// The raw cp932 form (the internal code of 0 is the cp932 code).
class raw_sjis_decoder: public char_decoder
{
public:
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final)
    {
      while (src < se && dst < de)
        {
          int c = *src;
          if (SJISP (c))
            {
              if (src + 1 == se)
                {
                  if (!final)
                    return;
                  *dst++ = c;
                  src++;
                }
              else if (SJIS2P (src[1]))
                {
                  *dst++ = (c << 8) | src[1];
                  src += 2;
                }
              else
                {
                  *dst++ = c;
                  src++;
                }
            }
          else
            {
              *dst++ = c;
              src++;
            }
        }
    }
};

class raw_sjis_encoder: public char_encoder
{
public:
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de)
    {
      while (src < se && de - dst >= 2)
        {
          Char cc = *src++;
          if (DBCP (cc))
            *dst++ = u_char (cc >> 8);
          *dst++ = u_char (cc);
        }
    }
  virtual bool can_encode (codec_char) const {return true;}
};

// 0: Unicode (or a raw byte) -> the internal code.  1 or 2 Chars.
static int
codec_to_chars (codec_char u, Char *out)
{
  if (codec_raw_byte_p (u))
    {
      out[0] = u & 0xff;
      return 1;
    }
  if (u < 0x10000)
    {
      Char cc = w2i (ucs2_t (u));
      if (cc != CHAR_NONE)
        {
          out[0] = cc;
          return 1;
        }
      out[0] = utf16_ucs2_to_undef_pair_high (ucs2_t (u));
      out[1] = utf16_ucs2_to_undef_pair_low (ucs2_t (u));
      return 2;
    }
  if (u < 0x110000)
    {
      out[0] = utf16_ucs4_to_pair_high (u);
      out[1] = utf16_ucs4_to_pair_low (u);
      return 2;
    }
  out[0] = CODEC_DEFCHAR;
  return 1;
}

// 0: the internal code -> Unicode.  It returns the number of Chars that it
// used, or 0 when it needs the next Char (the first half of a pair is the
// last one).
static int
chars_to_codec (const Char *p, const Char *pe, codec_char &u)
{
  Char cc = *p;
  if (cc < 0x80)
    {
      u = cc;
      return 1;
    }
  if (cc < 0x10000)
    {
      if (utf16_surrogate_high_p (ucs2_t (cc)))
        {
          if (p + 1 == pe)
            return 0;
          if (p[1] < 0x10000 && utf16_surrogate_low_p (ucs2_t (p[1])))
            {
              u = utf16_pair_to_ucs4 (ucs2_t (cc), ucs2_t (p[1]));
              return 2;
            }
        }
      else if (utf16_undef_char_high_p (ucs2_t (cc)))
        {
          if (p + 1 == pe)
            return 0;
          if (p[1] < 0x10000 && utf16_undef_char_low_p (ucs2_t (p[1])))
            {
              u = utf16_undef_pair_to_ucs2 (ucs2_t (cc), ucs2_t (p[1]));
              return 2;
            }
        }
      ucs2_t wc = i2w (cc);
      if (wc != ucs2_t (-1))
        {
          u = wc;
          return 1;
        }
    }
  u = CODEC_DEFCHAR;
  return 1;
}

class stream_conv
{
public:
  bool raw;                     // the raw cp932 form (0 only)
  bool want_dec, want_enc;
  bool no_signature;            // the encoder must not write a signature
  codec_spec spec;
  dup_memory mem;               // duplicated codes of cp932 (design 5.4)
  char_decoder *dec;
  char_encoder *enc;
  u_char ib[CONV_IB];           // input bytes that the decoder did not take
  int in_n;
  Char ob[CONV_OB];             // decoded characters that were not read yet
  int ob_head, ob_tail;
  lChar peeked;                 // the character after a CR
  Char held;                    // 0: the first half of a pair

  stream_conv ()
    : raw (false), want_dec (false), want_enc (false), no_signature (false),
      dec (0), enc (0), in_n (0), ob_head (0), ob_tail (0),
      peeked (lChar_EOF), held (CHAR_NONE)
    {
      memset (&spec, 0, sizeof spec);
    }
  ~stream_conv ()
    {
      delete dec;
      delete enc;
    }
  bool make_codecs ();
  void after_seek (bool at_start)
    {
      no_signature = !at_start;
      make_codecs ();
    }
};

bool
stream_conv::make_codecs ()
{
  delete dec;
  delete enc;
  dec = 0;
  enc = 0;
  in_n = 0;
  ob_head = ob_tail = 0;
  peeked = lChar_EOF;
  held = CHAR_NONE;

  raw = !XYZZY_CHAR_UNICODE && spec.type == encoding_sjis;
  if (raw)
    {
      if (want_dec)
        dec = new raw_sjis_decoder;
      if (want_enc)
        enc = new raw_sjis_encoder;
      return true;
    }
  if (want_dec)
    dec = make_decoder (spec, &mem);
  if (want_enc)
    {
      codec_spec es = spec;
      if (no_signature)
        es.flags &= ~ENCODING_UTF_SIGNATURE;
      enc = make_encoder (es, &mem);
    }
  return (!want_dec || dec) && (!want_enc || enc);
}

// The converter of the encoding (a char-encoding object, or nil for the
// default of a stream: *default-fileio-encoding*).
static void
conv_spec (lisp lencoding, codec_spec &spec)
{
  if (!lencoding || lencoding == Qnil)
    lencoding = symbol_value_char_encoding (Vdefault_fileio_encoding);
  else
    check_char_encoding (lencoding);
  if (!codec_spec_from_encoding (lencoding, spec))
    FEprogram_error (Einvalid_encoding_option, lencoding);
}

static stream_conv *
make_stream_conv (const codec_spec &spec, bool input, bool output)
{
  stream_conv *cv = new stream_conv;
  cv->spec = spec;
  cv->want_dec = input;
  cv->want_enc = output;
  if (!cv->make_codecs ())
    {
      delete cv;
      FEprogram_error (Einvalid_encoding_option, Qnil);
    }
  return cv;
}

static stream_conv *
conv_of (lisp stream)
{
  lstream *s = (lstream *)stream;
  if (!s->conv)
    {
      codec_spec spec;
      conv_spec (0, spec);
      s->conv = make_stream_conv (spec, input_stream_p (stream) != 0,
                                  output_stream_p (stream) != 0);
    }
  return s->conv;
}

// The sources and the sinks of the bytes.
struct file_src
{
  FILE *fp;
  int get () {return getc (fp);}
};

struct sock_src
{
  sockinet *so;
  int get ()
    {
      int c = so->sgetc ();
      return c == sock::eof ? EOF : c;
    }
};

struct file_sink
{
  FILE *fp;
  void put (int c) {putc (c, fp);}
  void write (const u_char *p, size_t n) {fwrite (p, 1, n, fp);}
};

struct sock_sink
{
  sockinet *so;
  void put (int c) {so->sputc (c);}
  void write (const u_char *p, size_t n)
    {
      for (size_t i = 0; i < n; i++)
        so->sputc (p[i]);
    }
};

// Decodes the bytes in ib.  With final, whatever is left becomes raw bytes.
static void
conv_decode (stream_conv *cv, bool final)
{
  for (;;)
    {
      codec_char cb[CONV_CB];
      codec_char *dst = cb;
      const u_char *s = cv->ib, *s0 = s, *se = cv->ib + cv->in_n;
      cv->dec->decode (s, se, dst, cb + CONV_CB, final);
      int rest = int (se - s);
      if (rest && s != cv->ib)
        memmove (cv->ib, s, rest);
      cv->in_n = rest;
      for (const codec_char *p = cb; p < dst; p++)
        {
          if (cv->raw)
            cv->ob[cv->ob_tail++] = *p;
          else if constexpr (XYZZY_CHAR_UNICODE)
            cv->ob[cv->ob_tail++] = *p;
          else
            cv->ob_tail += codec_to_chars (*p, cv->ob + cv->ob_tail);
        }
      if (s == s0 && dst == cb)
        break;
      if (!final && !cv->in_n)
        break;
    }
  if (final)
    cv->in_n = 0;
}

// The next decoded character, or lChar_EOF.  The line end is not touched.
template <class Src>
static lChar
conv_getc (stream_conv *cv, Src &src)
{
  for (;;)
    {
      if (cv->ob_head < cv->ob_tail)
        return cv->ob[cv->ob_head++];
      cv->ob_head = cv->ob_tail = 0;
      int c = src.get ();
      if (c == EOF)
        {
          conv_decode (cv, true);
          if (cv->ob_tail == 0)
            return lChar_EOF;
          continue;
        }
      cv->ib[cv->in_n++] = u_char (c);
      conv_decode (cv, cv->in_n == CONV_IB);
    }
}

template <class Src>
static lChar
conv_readc (stream_conv *cv, int eol, Src &src)
{
  lChar c;
  if (cv->peeked != lChar_EOF)
    {
      c = cv->peeked;
      cv->peeked = lChar_EOF;
    }
  else
    c = conv_getc (cv, src);
  if (c == '\r' && eol == lstream::ENCODE_CANON)
    {
      lChar c2 = conv_getc (cv, src);
      if (c2 == '\n')
        c = c2;
      else
        cv->peeked = c2;
    }
  return c;
}

static bool
conv_pending_p (stream_conv *cv)
{
  return cv && (cv->ob_head < cv->ob_tail || cv->peeked != lChar_EOF);
}

template <class Sink>
static void
conv_encode_codec (stream_conv *cv, const codec_char *s, const codec_char *se,
                   Sink &sink)
{
  while (s < se)
    {
      u_char ob[CONV_EOB];
      u_char *d = ob;
      const codec_char *s0 = s;
      cv->enc->encode (s, se, d, ob + sizeof ob);
      if (d != ob)
        sink.write (ob, d - ob);
      if (s == s0 && d == ob)
        break;
    }
}

template <class Sink>
static void
conv_encode (stream_conv *cv, const Char *p, const Char *pe, Sink &sink)
{
  codec_char cc[CONV_PIECE * 2 + 2];
  codec_char *q = cc;
  if (cv->raw)
    while (p < pe)
      *q++ = *p++;
  else if constexpr (XYZZY_CHAR_UNICODE)
    while (p < pe)
      *q++ = *p++;
  else
    while (p < pe)
      {
        codec_char u;
        int k = chars_to_codec (p, pe, u);
        if (!k)
          {
            cv->held = *p;
            break;
          }
        *q++ = u;
        p += k;
      }
  conv_encode_codec (cv, cc, q, sink);
}

template <class Sink>
static void
conv_write (stream_conv *cv, int eol, const Char *b, size_t n, Sink &sink)
{
  while (n)
    {
      size_t m = n > CONV_PIECE ? CONV_PIECE : n;
      Char tmp[CONV_PIECE * 2 + 2];
      Char *t = tmp;
      if (cv->held != CHAR_NONE)
        {
          *t++ = cv->held;
          cv->held = CHAR_NONE;
        }
      for (size_t i = 0; i < m; i++)
        {
          if (eol == lstream::ENCODE_CANON && b[i] == '\n')
            *t++ = '\r';
          *t++ = b[i];
        }
      b += m;
      n -= m;
      conv_encode (cv, tmp, t, sink);
    }
}

// What is pending at the end of the text (the return to ASCII of ISO-2022...).
template <class Sink>
static void
conv_finish (stream_conv *cv, Sink &sink)
{
  if (!cv || !cv->enc)
    return;
  if (cv->held != CHAR_NONE)
    {
      cv->held = CHAR_NONE;
      codec_char u = CODEC_DEFCHAR;
      conv_encode_codec (cv, &u, &u + 1, sink);
    }
  u_char ob[CONV_EOB];
  u_char *d = ob;
  cv->enc->finish (d, ob + sizeof ob);
  if (d != ob)
    sink.write (ob, d - ob);
}

// :binary.  A byte is a character (0..255).
template <class Sink>
static inline void
put_binary (Char cc, Sink &sink)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    sink.put (cc < 0x100 || codec_raw_byte_p (cc) ? int (cc & 0xff) : '?');
  else
    {
      if (DBCP (cc))
        sink.put (cc >> 8);
      sink.put (u_char (cc));
    }
}

// ---------------------------------------------------------------------------
// The encoding of a file that is loaded (design 4.5)
//
//   1. BOM: EF BB BF is UTF-8, FF FE and FE FF are UTF-16
//   2. -*- ... Encoding: <name> ... -*- in the first or the second line
//   3. the whole file is valid UTF-8 and has a non ASCII byte: UTF-8
//   4. otherwise cp932

// Whole-file check of UTF-8.
struct utf8_validator
{
  int need;
  u_char lo, hi;
  bool nonascii, ok;

  utf8_validator () : need (0), lo (0x80), hi (0xbf), nonascii (false), ok (true) {}
  void feed (const u_char *p, size_t n)
    {
      for (size_t i = 0; i < n && ok; i++)
        {
          u_char c = p[i];
          if (need)
            {
              if (c < lo || c > hi)
                {
                  ok = false;
                  return;
                }
              lo = 0x80;
              hi = 0xbf;
              need--;
            }
          else if (c >= 0x80)
            {
              nonascii = true;
              if (c >= 0xc2 && c <= 0xdf)
                need = 1;
              else if (c >= 0xe0 && c <= 0xef)
                {
                  need = 2;
                  if (c == 0xe0)
                    lo = 0xa0;
                  else if (c == 0xed)
                    hi = 0x9f;
                }
              else if (c >= 0xf0 && c <= 0xf4)
                {
                  need = 3;
                  if (c == 0xf0)
                    lo = 0x90;
                  else if (c == 0xf4)
                    hi = 0x8f;
                }
              else
                {
                  ok = false;
                  return;
                }
            }
        }
    }
  bool valid () const {return ok && !need;}
};

static const u_char *
find_mark (const u_char *p, const u_char *pe)
{
  for (; p + 3 <= pe; p++)
    if (p[0] == '-' && p[1] == '*' && p[2] == '-')
      return p;
  return 0;
}

static bool
mark_key_p (const u_char *p, const u_char *pe, const char *key)
{
  size_t l = strlen (key);
  return size_t (pe - p) == l && !_strnicmp ((const char *)p, key, l);
}

// The value of "encoding" or "coding" in the -*- ... -*- of a line.
static bool
scan_line_mark (const u_char *p, const u_char *pe, char *name, size_t size)
{
  const u_char *b = find_mark (p, pe);
  if (!b)
    return false;
  b += 3;
  const u_char *e = find_mark (b, pe);
  if (!e)
    return false;
  while (b < e)
    {
      const u_char *s = b;
      while (s < e && *s != ';')
        s++;
      const u_char *colon = b;
      while (colon < s && *colon != ':')
        colon++;
      if (colon < s)
        {
          const u_char *k0 = b, *k1 = colon;
          while (k0 < k1 && isspace (*k0))
            k0++;
          while (k1 > k0 && isspace (k1[-1]))
            k1--;
          const u_char *v0 = colon + 1, *v1 = s;
          while (v0 < v1 && isspace (*v0))
            v0++;
          while (v1 > v0 && isspace (v1[-1]))
            v1--;
          if ((mark_key_p (k0, k1, "encoding") || mark_key_p (k0, k1, "coding"))
              && v1 > v0 && size_t (v1 - v0) < size)
            {
              memcpy (name, v0, v1 - v0);
              name[v1 - v0] = 0;
              return true;
            }
        }
      b = s + 1;
    }
  return false;
}

static bool
scan_encoding_mark (const u_char *p, size_t n, char *name, size_t size)
{
  const u_char *pe = p + n;
  for (int line = 0; line < 2 && p < pe; line++)
    {
      const u_char *e = p;
      while (e < pe && *e != '\n' && *e != '\r')
        e++;
      if (scan_line_mark (p, e, name, size))
        return true;
      p = e;
      if (p < pe && *p == '\r')
        p++;
      if (p < pe && *p == '\n')
        p++;
    }
  return false;
}

static bool
spec_by_name (const char *name, codec_spec &spec)
{
  int l = int (strlen (name));

  // The names of the char-encoding objects.
  for (lisp p = xsymbol_value (Vchar_encoding_list); consp (p); p = xcdr (p))
    {
      lisp e = xcar (p);
      if (char_encoding_p (e)
          && stringp (xchar_encoding_name (e))
          && string_equalp (xstring_contents (xchar_encoding_name (e)),
                            xstring_length (xchar_encoding_name (e)), name, l)
          && codec_spec_from_encoding (e, spec)
          && spec.type != encoding_auto_detect)
        return true;
    }

  // The MIME names (encoding.l makes the table from *character-set-alist*).
  lisp package = xsymbol_value (Veditor_package);
  if (package && package != Qunbound && package != Qnil)
    {
      lisp sym = Ffind_symbol (make_string ("*mime-charset-name-hash-table*"), package);
      if (symbolp (sym) && sym != Qnil && xsymbol_value (sym) != Qunbound
          && hash_table_p (xsymbol_value (sym)))
        {
          lisp e = Fgethash (make_string (name), xsymbol_value (sym), Qnil);
          if (e != Qnil && char_encoding_p (e)
              && codec_spec_from_encoding (e, spec)
              && spec.type != encoding_auto_detect)
            return true;
        }
    }

  // Before encoding.l is loaded.
  static const char *const utf8_names[] = {"utf-8", "utf8", "utf-8n", "utf8n"};
  static const char *const sjis_names[] =
    {"sjis", "shift_jis", "shift-jis", "cp932", "windows-31j", "x-sjis", "ms_kanji"};
  for (int i = 0; i < numberof (utf8_names); i++)
    if (!_stricmp (name, utf8_names[i]))
      {
        memset (&spec, 0, sizeof spec);
        spec.type = encoding_utf8;
        return true;
      }
  for (int i = 0; i < numberof (sjis_names); i++)
    if (!_stricmp (name, sjis_names[i]))
      {
        memset (&spec, 0, sizeof spec);
        spec.type = encoding_sjis;
        return true;
      }
  return false;
}

// Chooses the encoding of the file stream by the rules above.  The stream
// has to be an input file stream that is at its start.
void
stream_detect_load_encoding (lisp stream)
{
  if (!streamp (stream) || !file_stream_p (stream) || !xfile_stream_input (stream))
    return;
  FILE *fp = xfile_stream_input (stream);
  if (_fseeki64 (fp, 0, SEEK_SET))
    return;

  u_char head[4096];
  size_t n = fread (head, 1, sizeof head, fp);

  codec_spec spec;
  memset (&spec, 0, sizeof spec);
  int64_t skip = 0;
  bool decided = false;

  if (n >= 3 && head[0] == 0xef && head[1] == 0xbb && head[2] == 0xbf)
    {
      spec.type = encoding_utf8;
      skip = 3;
      decided = true;
    }
  else if (n >= 2 && ((head[0] == 0xff && head[1] == 0xfe)
                      || (head[0] == 0xfe && head[1] == 0xff)))
    {
      // The decoder takes the BOM and decides the byte order.
      spec.type = encoding_utf16;
      spec.flags = ENCODING_UTF_SIGNATURE;
      decided = true;
    }
  else
    {
      char name[128];
      if (scan_encoding_mark (head, n, name, sizeof name)
          && spec_by_name (name, spec))
        decided = true;
    }

  if (!decided)
    {
      utf8_validator v;
      v.feed (head, n);
      if (n == sizeof head)
        {
          u_char buf[65536];
          size_t k;
          while (v.ok && (k = fread (buf, 1, sizeof buf, fp)) > 0)
            v.feed (buf, k);
        }
      memset (&spec, 0, sizeof spec);
      spec.type = v.valid () && v.nonascii ? encoding_utf8 : encoding_sjis;
    }

  _fseeki64 (fp, skip, SEEK_SET);
  stream_conv *cv = make_stream_conv (spec, true, false);
  lstream *s = (lstream *)stream;
  delete s->conv;
  s->conv = cv;
}

static void
close_file_stream (lisp stream, int abort)
{
  if (!xfile_stream_alt_pathname (stream))
    return;
  xstream_open_p (stream) = 0;
  if (abort)
    WINFS::DeleteFile (xfile_stream_alt_pathname (stream));
  else
    {
      char path[PATH_MAX + 1];
      w2s (path, xfile_stream_pathname (stream));
      WINFS::DeleteFile (path);
      if (!WINFS::MoveFile (xfile_stream_alt_pathname (stream), path))
        file_error (GetLastError (), xfile_stream_pathname (stream));
    }
  xfree (xfile_stream_alt_pathname (stream));
  xfile_stream_alt_pathname (stream) = 0;
}

lstream::~lstream ()
{
  delete conv;
  conv = 0;
  if (file_stream_p (this))
    {
      if (input && input != stdin)
        fclose ((FILE *)input);
      if (output && output != input && output != stdout && output != stderr)
        fclose ((FILE *)output);
      if (alt_pathname)
        close_file_stream (this, 1);
    }
  else if (socket_stream_p (this))
    {
      if (xsocket_stream_sock (this))
        delete xsocket_stream_sock (this);
    }
}

static lstream *
make_stream (stream_type type)
{
  lstream *p = ldata <lstream, Tstream>::lalloc ();
  p->type = type;
  p->pending = lChar_EOF;
  p->column = 0;
  p->linenum = 1;
  p->pathname = Qnil;
  p->alt_pathname = 0;
  p->open_p = 1;
  p->encoding = lstream::ENCODE_CANON;
  p->conv = 0;
  return p;
}

static lisp
make_file_stream (stream_type type)
{
  lisp stream = make_stream (type);
  assert (file_stream_p (stream));
  xfile_stream_pathname (stream) = Qnil;
  xfile_stream_input (stream) = 0;
  xfile_stream_output (stream) = 0;
  return stream;
}

static lisp
make_composite_stream (stream_type type)
{
  lisp stream = make_stream (type);
  assert (composite_stream_p (stream));
  xcomposite_stream_input (stream) = Qnil;
  xcomposite_stream_output (stream) = Qnil;
  return stream;
}

static lisp
make_string_stream (stream_type type)
{
  lisp stream = make_stream (type);
  assert (string_stream_p (stream));
  xstring_stream_input (stream) = Qnil;
  xstring_stream_output (stream) = Qnil;
  return stream;
}

static inline lisp
make_status_stream ()
{
  return make_stream (st_status);
}

static lisp
make_buffer_stream ()
{
  lisp stream = make_stream (st_buffer);
  xbuffer_stream_eob (stream) = Qnil;
  xbuffer_stream_marker (stream) = Qnil;
  return stream;
}

static lisp
make_keyboard_stream ()
{
  return make_stream (st_keyboard);
}

static lisp
make_wstream_stream ()
{
  lisp stream = make_stream (st_wstream);
  xwstream_stream_wstream (stream) = 0;
  return stream;
}

static lisp
make_socket_stream ()
{
  lisp stream = make_stream (st_socket);
  xsocket_stream_sock (stream) = 0;
  return stream;
}

static inline lisp
make_debug_output_stream ()
{
  return make_stream (st_debug_output);
}

lisp
Fstreamp (lisp object)
{
  return boole (streamp (object));
}

lisp
Fsynonym_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_synonym);
}

lisp
Fbroadcast_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_broadcast);
}

lisp
Fconcatenated_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_concatenated);
}

lisp
Fecho_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_echo);
}

lisp
Ftwo_way_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_two_way);
}

lisp
Fstring_stream_p (lisp stream)
{
  return boole (streamp (stream) && string_stream_p (stream));
}

lisp
Fstring_input_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_string_input);
}

lisp
Fstring_output_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_string_output);
}

lisp
Fstatus_window_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_status);
}

lisp
Fbuffer_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_buffer);
}

lisp
Fkeyboard_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_keyboard);
}

lisp
Fsocket_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_socket);
}

lisp
Fgeneral_input_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_general_input);
}

lisp
Fgeneral_output_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_general_output);
}

lisp
Fdebug_output_stream_p (lisp stream)
{
  return boole (streamp (stream) && xstream_type (stream) == st_debug_output);
}

lisp
Fopen_stream_p (lisp stream)
{
  check_stream (stream);
  return boole (xstream_open_p (stream));
}

lisp
Finput_stream_p (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      if (xstream_type (stream) != st_synonym)
        return boole (input_stream_p (stream));
      assert (symbolp (xcomposite_stream_input (stream)));
      stream = symbol_value (xcomposite_stream_input (stream), selected_buffer ());
      QUIT;
    }
}

lisp
Foutput_stream_p (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      if (xstream_type (stream) != st_synonym)
        return boole (output_stream_p (stream));
      assert (symbolp (xcomposite_stream_output (stream)));
      stream = symbol_value (xcomposite_stream_output (stream), selected_buffer ());
      QUIT;
    }
}

static int
stream_encoding (lisp lencoding)
{
  if (lencoding == Kcanonical || lencoding == Ktext)
    return lstream::ENCODE_CANON;
  if (lencoding == Kraw)
    return lstream::ENCODE_RAW;
  if (lencoding == Kbinary)
    return lstream::ENCODE_BINARY;
  FEprogram_error (Einvalid_encoding_option, lencoding);
  return 0;
}

lisp
create_file_stream (lisp filename, lisp direction, lisp if_exists,
                    lisp if_does_not_exist, lisp lencoding,
                    lisp lshare, lisp lchar_encoding)
{
  char path[PATH_MAX + 1];
  pathname2cstr (filename, path);

  if (if_does_not_exist != Kerror && if_does_not_exist != Kcreate
      && if_does_not_exist != Qnil)
    FEprogram_error (Einvalid_if_does_not_exist_option, if_does_not_exist);

  int encoding = stream_encoding (lencoding);
  codec_spec cspec;
  conv_spec (lchar_encoding, cspec);
  int dont_create = if_does_not_exist != Kcreate;
  int need_alt = 0;
  int create, access;

  if (direction == Kinput || direction == Kprobe)
    {
      access = GENERIC_READ;
      create = dont_create ? OPEN_EXISTING : OPEN_ALWAYS;
      if_exists = Qnil;
    }
  else
    {
      access = direction == Kio ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
      if (if_exists == Kerror || if_exists == Qnil)
        {
          if (dont_create)
            {
              if (if_exists == Kerror)
                file_error (Ecannot_create_file, filename);
              return Qnil;
            }
          create = CREATE_NEW;
        }
      else if (if_exists == Knew_version)
        {
          need_alt = 1;
          create = CREATE_ALWAYS;
        }
      else if (if_exists == Koverwrite)
        {
          if (direction == Kio)
            create = dont_create ? OPEN_EXISTING : OPEN_ALWAYS;
          else
            create = dont_create ? TRUNCATE_EXISTING : CREATE_ALWAYS;
        }
      else if (if_exists == Kappend)
        create = dont_create ? OPEN_EXISTING : OPEN_ALWAYS;
      else if (if_exists == Krename
               || if_exists == Krename_and_delete
               || if_exists == Ksupersede)
        {
          need_alt = 1;
          create = CREATE_ALWAYS;
        }
      else
        FEprogram_error (Einvalid_if_exists_option, if_exists);
    }

  int share;
  if (lshare == Kread)
    share = FILE_SHARE_READ;
  else if (lshare == Kwrite)
    share = FILE_SHARE_WRITE;
  else if (lshare == Kread_write)
    share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  else if (lshare == Qnil)
    share = 0;
  else if (!lshare)
    share = access == GENERIC_READ ? FILE_SHARE_READ : 0;
  else
    FEprogram_error (Einvalid_share_option, lshare);

  if (need_alt && (if_exists == Kerror || dont_create))
    {
      HANDLE h = WINFS::CreateFile (path, 0, 0, 0, OPEN_EXISTING, 0, 0);
      if (h != INVALID_HANDLE_VALUE)
        {
          CloseHandle (h);
          if (if_exists == Kerror)
            file_error (ERROR_ALREADY_EXISTS, filename);
        }
      else
        {
          if (dont_create)
            {
              if (if_does_not_exist == Kerror)
                file_error (GetLastError (), filename);
              return Qnil;
            }
        }
    }

  lisp stream = make_file_stream (access == GENERIC_READ
                                  ? st_file_input
                                  : (access == GENERIC_WRITE
                                     ? st_file_output
                                     : st_file_io));
  xfile_stream_pathname (stream) = make_string (path);
  xfile_stream_encoding (stream) = encoding;

  if (need_alt)
    {
      // 同じディレクトリに一時ファイル（xyzXXXX.tmp）を作る。GetTempFileName は
      // 長いパスを扱えないので、自前の make_temp_file_name を使う。
      char buf[PATH_MAX + 16];
      const char *sl = find_last_slash (path);
      if (sl)
        {
          memcpy (buf, path, sl + 1 - path);
          buf[sl + 1 - path] = 0;
        }
      else
        strcpy (buf, "./");
      if (!make_temp_file_name (buf, "xyz", "tmp"))
        file_error (GetLastError ());
      xfile_stream_alt_pathname (stream) = xstrdup (buf);
    }

  HANDLE h = WINFS::CreateFile ((xfile_stream_alt_pathname (stream)
                                 ? xfile_stream_alt_pathname (stream) : path),
                                access, share, 0, create,
                                FILE_ATTRIBUTE_ARCHIVE, 0);
  if (h == INVALID_HANDLE_VALUE)
    {
      int e = GetLastError ();
      switch (e)
        {
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
          if (if_exists == Kerror)
            file_error (e, filename);
          return Qnil;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_PATHNAME:
          if (if_does_not_exist == Kerror)
            file_error (e, filename);
          return Qnil;

        default:
          file_error (e, filename);
        }
    }

  if (if_exists == Krename && xfile_stream_alt_pathname (stream))
    {
      xfree (xfile_stream_alt_pathname (stream));
      xfile_stream_alt_pathname (stream) = 0;
    }

  int fd = _open_osfhandle (intptr_t (h), access == GENERIC_READ ? _O_RDONLY : 0);
  if (fd == -1)
    {
      CloseHandle (h);
      FEsimple_crtl_error (errno, filename);
    }

  FILE *fp = _fdopen (fd, (access == GENERIC_READ
                           ? "rb"
                           : (access == GENERIC_WRITE ? "wb" : "r+b")));
  if (!fp)
    {
      int e = errno;
      _close (fd);
      FEsimple_crtl_error (e, filename);
    }

  if (if_exists == Kappend)
    fseek (fp, 0, SEEK_END);

  xfile_stream_input (stream) = (access & GENERIC_READ) ? fp : 0;
  xfile_stream_output (stream) = (access & GENERIC_WRITE) ? fp : 0;

  {
    stream_conv *cv = make_stream_conv (cspec, (access & GENERIC_READ) != 0,
                                        (access & GENERIC_WRITE) != 0);
    // Appending to a file that has a text: no signature in the middle.
    if (if_exists == Kappend && _ftelli64 (fp) > 0)
      {
        cv->no_signature = true;
        cv->make_codecs ();
      }
    ((lstream *)stream)->conv = cv;
  }

  if (direction == Kprobe)
    Fclose (stream, Qnil);

  return stream;
}

lisp
Fopen (lisp filename, lisp keys)
{
  lisp direction = find_keyword (Kdirection, keys, Kinput);
  lisp if_exists = find_keyword (Kif_exists, keys, 0);
  lisp if_does_not_exist = find_keyword (Kif_does_not_exist, keys, 0);
  lisp encoding = find_keyword (Kencoding, keys, Kcanonical);
  lisp share = find_keyword (Kshare, keys, 0);
  lisp char_encoding = find_keyword (Kchar_encoding, keys, 0);

  if (direction == Kinput)
    {
      if_exists = Qnil;
      if (!if_does_not_exist)
        if_does_not_exist = Kerror;
    }
  else if (direction == Koutput || direction == Kio)
    {
      if (!if_exists)
        if_exists = Knew_version;
      if (!if_does_not_exist)
        {
          if (if_exists == Koverwrite || if_exists == Kappend)
            if_does_not_exist = Kerror;
          else
            if_does_not_exist = Kcreate;
        }
    }
  else if (direction == Kprobe)
    {
      if_exists = Qnil;
      if (!if_does_not_exist)
        if_does_not_exist = Qnil;
    }
  else
    FEprogram_error (Einvalid_direction_option, direction);

  return create_file_stream (filename, direction, if_exists,
                             if_does_not_exist, encoding, share, char_encoding);
}

lisp
Ffile_position (lisp stream, lisp lpos)
{
  check_stream (stream);
  if (!file_stream_p (stream))
    FEtype_error (stream, Qfile_stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);

  FILE *fp;
  switch (xstream_type (stream))
    {
    case st_file_input:
    case st_file_io:
      fp = xfile_stream_input (stream);
      break;

    case st_file_output:
      fp = xfile_stream_output (stream);
      break;

    default:
      assert (0);
      return Qnil;
    }

  if (lpos && lpos != Qnil)
    {
      int origin = SEEK_SET;
      int64_t offset;
      if (lpos == Kstart)
        offset = 0;
      else if (lpos == Kend)
        {
          origin = SEEK_END;
          offset = 0;
        }
      else
        {
          offset = fixnum_value64 (lpos);
          if (offset < 0)
            FErange_error (lpos);
        }

      if (_fseeki64 (fp, offset, origin) == -1)
        return Qnil;
      if (((lstream *)stream)->conv)
        ((lstream *)stream)->conv->after_seek (origin == SEEK_SET && !offset);
      if (origin == SEEK_SET && !offset)
        {
          xstream_linenum (stream) = 0;
          xstream_column (stream) = 0;
        }
      return Qt;
    }
  else
    {
      int64_t offset = _ftelli64 (fp);
      return offset >= 0 ? make_integer (offset) : Qnil;
    }
}

lisp
Fset_end_of_file (lisp stream)
{
  check_stream (stream);
  if (!file_stream_p (stream))
    FEtype_error (stream, Qfile_stream);
  if (!output_stream_p (stream))
    FEtype_error (stream, Qoutput_stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);
  if (!SetEndOfFile (HANDLE (_get_osfhandle (_fileno (xfile_stream_output (stream))))))
    FEsimple_win32_error (GetLastError ());
  return Qnil;
}

lisp
Fmake_synonym_stream (lisp symbol)
{
  check_symbol (symbol);
  lisp stream = make_composite_stream (st_synonym);
  xcomposite_stream_input (stream) = symbol;
  xcomposite_stream_output (stream) = symbol;
  return stream;
}

lisp
Fmake_broadcast_stream (lisp streams)
{
  for (lisp p = streams; consp (p); p = xcdr (p))
    if (Foutput_stream_p (xcar (p)) == Qnil)
      FEtype_error (xcar (p), Qoutput_stream);
  lisp stream = make_composite_stream (st_broadcast);
  xcomposite_stream_input (stream) = Qnil;
  xcomposite_stream_output (stream) = Fcopy_list (streams);
  return stream;
}

lisp
Fmake_concatenated_stream (lisp streams)
{
  for (lisp p = streams; consp (p); p = xcdr (p))
    if (Finput_stream_p (xcar (p)) == Qnil)
      FEtype_error (xcar (p), Qinput_stream);
  lisp stream = make_composite_stream (st_concatenated);
  xcomposite_stream_input (stream) = Fcopy_list (streams);
  xcomposite_stream_output (stream) = Qnil;
  return stream;
}

lisp
Fmake_two_way_stream (lisp input, lisp output)
{
  if (Finput_stream_p (input) == Qnil)
    FEtype_error (input, Qinput_stream);
  if (Foutput_stream_p (output) == Qnil)
    FEtype_error (output, Qoutput_stream);
  lisp stream = make_composite_stream (st_two_way);
  xcomposite_stream_input (stream) = input;
  xcomposite_stream_output (stream) = output;
  return stream;
}

lisp
Fmake_echo_stream (lisp input, lisp output)
{
  lisp stream = Fmake_two_way_stream (input, output);
  xstream_type (stream) = st_echo;
  return stream;
}

lisp
Fmake_string_input_stream (lisp string, lisp lstart, lisp lend)
{
  check_string (string);
  int start, end;
  seq_start_end (xstring_length (string), start, end,
                 (lstart && lstart != Qnil) ? lstart : make_fixnum (0),
                 lend);
  lisp stream = make_string_stream (st_string_input);
  xstring_stream_input (stream) = string;
  xstring_stream_start (stream) = start;
  xstring_stream_end (stream) = end;
  return stream;
}

lisp
Fmake_string_output_stream ()
{
  lisp string = make_complex_string (Char (0), 0, 128, 1);
  lisp stream = make_string_stream (st_string_output);
  xstring_stream_output (stream) = string;
  return stream;
}

lisp
Fsi_make_string_output_stream_from_string (lisp string)
{
  check_string (string);
  if (!complex_string_p (string) || !xarray_has_fillp (string))
    FEprogram_error (Evector_has_no_fill_pointer, string);
  lisp stream = make_string_stream (st_string_output);
  xstring_stream_output (stream) = string;
  return stream;
}

lisp
Fsi_get_string_input_stream_index (lisp stream)
{
  if (Fstring_input_stream_p (stream) == Qnil)
    FEtype_error (stream, Qstring_input_stream);
  return make_fixnum (xstring_stream_start (stream));
}

lisp
Fget_output_stream_string (lisp stream)
{
  if (Fstring_output_stream_p (stream) == Qnil)
    FEtype_error (stream, Qstring_output_stream);
  lisp t = xstring_stream_output (stream);
  xstring_stream_output (stream) = make_complex_string (Char (0), 0, 128, 1);
  return t;
}

lisp
Fmake_buffer_stream (lisp buffer, lisp point, lisp eob)
{
  Buffer *bp = Buffer::coerce_to_buffer (buffer);
  lisp stream = make_buffer_stream ();
  lisp marker = Fmake_marker (bp->lbp);
  xbuffer_stream_marker (stream) = marker;
  if (!point || point == Qnil)
    xmarker_point (marker) = bp->b_contents.p1;
  else
    xmarker_point (marker) = bp->coerce_to_restricted_point (point);
  if (eob && eob != Qnil)
    {
      point_t p = bp->coerce_to_restricted_point (eob);
      eob = Fmake_marker (bp->lbp);
      xbuffer_stream_eob (stream) = eob;
      xmarker_point (eob) = p;
    }
  return stream;
}

static void
valid_buffer_stream_p (lisp stream)
{
  check_stream (stream);
  if (!buffer_stream_p (stream))
    FEtype_error (stream, Qbuffer_stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);
  lisp marker = xbuffer_stream_marker (stream);
  check_marker (marker);
  if (!xmarker_buffer (marker))
    FEprogram_error (Edeleted_marker);
  lisp eob = xbuffer_stream_eob (stream);
  if (eob != Qnil)
    {
      check_marker (eob);
      if (!xmarker_buffer (eob))
        FEprogram_error (Edeleted_marker);
    }
}

lisp
Fbuffer_stream_buffer (lisp stream)
{
  valid_buffer_stream_p (stream);
  return xmarker_buffer (xbuffer_stream_marker (stream))->lbp;
}

lisp
Fbuffer_stream_point (lisp stream)
{
  valid_buffer_stream_p (stream);
  return make_fixnum (xmarker_point (xbuffer_stream_marker (stream)));
}

lisp
Fbuffer_stream_set_point (lisp stream, lisp point)
{
  valid_buffer_stream_p (stream);
  lisp marker = xbuffer_stream_marker (stream);
  Buffer *bp = xmarker_buffer (marker);
  xmarker_point (marker) = bp->coerce_to_restricted_point (point);
  return Qt;
}

static Buffer *
buffer_stream_point (lisp stream, Point &point)
{
  valid_buffer_stream_p (stream);
  lisp marker = xbuffer_stream_marker (stream);
  Buffer *bp = xmarker_buffer (marker);
  if (bp->b_stream_cache.p_chunk)
    bp->goto_char (bp->b_stream_cache, xmarker_point (marker));
  else
    bp->set_point (bp->b_stream_cache, xmarker_point (marker));
  point = bp->b_stream_cache;
  return bp;
}

static point_t
buffer_stream_eob (const Buffer *bp, lisp stream)
{
  lisp eob = xbuffer_stream_eob (stream);
  if (eob == Qnil)
    return bp->b_contents.p2;
  return xmarker_point (eob);
}

static void
write_buffer_stream (lisp stream, const Char *b, size_t size)
{
  Point point;
  Buffer *bp = buffer_stream_point (stream, point);
  bp->check_read_only ();
  bp->insert_chars (point, b, size);
  xmarker_point (xbuffer_stream_marker (stream)) = point.p_point;
}

lisp
Fconnect (lisp lhost, lisp lport, lisp keys)
{
  lisp stream = Qnil;
  try
    {
      Fbegin_wait_cursor ();
      int encoding = stream_encoding (find_keyword (Kencoding, keys, Kcanonical));
      codec_spec cspec;
      conv_spec (find_keyword (Kchar_encoding, keys, 0), cspec);
      sockinet::saddr addr (lhost, lport);
      stream = make_socket_stream ();
      protect_gc gcpro (stream);
      ((lstream *)stream)->conv = make_stream_conv (cspec, true, true);
      if (find_keyword_bool (Kssl, keys))
        {
          lisp lverify_mode = find_keyword (Kssl_verify_mode, keys);
          xsocket_stream_sock (stream) = new sockssl (lhost, lverify_mode);
        }
      else
        {
          xsocket_stream_sock (stream) = new sockinet;
        }
      xsocket_stream_sock (stream)->set_eof_error_p (0);
      xsocket_stream_sock (stream)->create ();
      xsocket_stream_sock (stream)->connect (addr);
      xsocket_stream_encoding (stream) = encoding;
    }
  catch (sock_error &e)
    {
      Fend_wait_cursor ();
      FEsocket_error (e.error_code (), e.ope ());
    }
  Fend_wait_cursor ();
  return stream;
}

lisp
Fmake_listen_socket (lisp lhost, lisp lport, lisp keys)
{
  lisp stream = Qnil;
  try
    {
      Fbegin_wait_cursor ();
      sockinet::saddr addr (lhost, lport);
      lisp lbacklog = find_keyword (Kbacklog, keys);
      int backlog = !lbacklog || lbacklog == Qnil ? 1 : fixnum_value (lbacklog);
      stream = make_socket_stream ();
      protect_gc gcpro (stream);
      xsocket_stream_sock (stream) = new sockinet;
      xsocket_stream_sock (stream)->set_eof_error_p (0);
      xsocket_stream_sock (stream)->create ();
      if (find_keyword_bool (Kreuseaddr, keys))
        xsocket_stream_sock (stream)->reuseaddr (1);
      xsocket_stream_sock (stream)->bind (addr);
      xsocket_stream_sock (stream)->listen (backlog);
    }
  catch (sock_error &e)
    {
      Fend_wait_cursor ();
      FEsocket_error (e.error_code (), e.ope ());
    }
  Fend_wait_cursor ();
  return stream;
}

static void
valid_socket_stream_p (lisp stream)
{
  check_stream (stream);
  if (!socket_stream_p (stream))
    FEtype_error (stream, Qsocket_stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);
}

lisp
Fssl_do_handshake (lisp stream, lisp lserver_name, lisp keys)
{
  valid_socket_stream_p (stream);

  sockinet *so = xsocket_stream_sock (stream);
  if (so->sslp ())
    return Qnil;

  try
    {
      Fbegin_wait_cursor ();
      lisp lverify_mode = find_keyword (Kssl_verify_mode, keys);
      sockssl *ssl = new sockssl (so, lserver_name, lverify_mode);
      xsocket_stream_sock (stream) = ssl;
      ssl->handshake ();
    }
  catch (sock_error &e)
    {
      Fend_wait_cursor ();
      FEsocket_error (e.error_code (), e.ope ());
    }
  Fend_wait_cursor ();

  return Qt;
}

lisp
Faccept_connection (lisp stream, lisp keys)
{
  valid_socket_stream_p (stream);
  int encoding = stream_encoding (find_keyword (Kencoding, keys, Kcanonical));
  codec_spec cspec;
  conv_spec (find_keyword (Kchar_encoding, keys, 0), cspec);
  lisp new_stream = Qnil;
  try
    {
      new_stream = make_socket_stream ();
      protect_gc gcpro (new_stream);
      ((lstream *)new_stream)->conv = make_stream_conv (cspec, true, true);
      SOCKET so = xsocket_stream_sock (stream)->accept ();
      try
        {
          xsocket_stream_sock (new_stream) = new sockinet (so);
          xsocket_stream_sock (new_stream)->set_eof_error_p (0);
          xsocket_stream_encoding (new_stream) = encoding;
        }
      catch (nonlocal_jump &)
        {
          sock::close_socket (so);
          throw;
        }
    }
  catch (sock_error &e)
    {
      FEsocket_error (e.error_code (), e.ope ());
    }
  return new_stream;
}

static void
close_socket_stream (lisp stream, int abort)
{
  sockinet *so = xsocket_stream_sock (stream);
  xsocket_stream_sock (stream) = 0;;
  xstream_open_p (stream) = 0;
  try
    {
      if (!abort)
        {
          sock_sink sink = {so};
          conv_finish (((lstream *)stream)->conv, sink);
        }
      so->close (abort);
    }
  catch (sock_error &e)
    {
      delete so;
      FEsocket_error (e.error_code (), e.ope ());
    }
  delete so;
}

lisp
Fsocket_stream_local_address (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->localaddr (addr);
      return make_string (addr.addrstr ());
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_local_name (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->localaddr (addr);
      const char *name = addr.hostname ();
      return name ? make_string (name) : Qnil;
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_local_port (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->localaddr (addr);
      return make_fixnum (addr.port ());
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_peer_address (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->peeraddr (addr);
      return make_string (addr.addrstr ());
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_peer_name (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->peeraddr (addr);
      const char *name = addr.hostname ();
      return name ? make_string (name) : Qnil;
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_peer_port (lisp stream)
{
  valid_socket_stream_p (stream);
  try
    {
      sockinet::saddr addr;
      xsocket_stream_sock (stream)->peeraddr (addr);
      return make_fixnum (addr.port ());
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
}

lisp
Fsocket_stream_set_timeout (lisp stream, lisp ltimeout)
{
  valid_socket_stream_p (stream);
  int sec, usec;
  double d = ltimeout == Qnil ? -1.0 : coerce_to_double_float (ltimeout);
  if (d < 0)
    sec = usec = -1;
  else
    {
      double r, q;
      r = modf (d, &q);
      sec = int (q);
      usec = int (r * 1000000);
    }
  xsocket_stream_sock (stream)->send_timeout (sec, usec);
  xsocket_stream_sock (stream)->recv_timeout (sec, usec);
  return Qnil;
}

lisp
Fsocket_stream_get_timeout (lisp stream)
{
  valid_socket_stream_p (stream);
  const timeval &tv = xsocket_stream_sock (stream)->send_timeout ();
  if (tv.tv_sec < 0)
    return Qnil;
  if (tv.tv_usec)
    return make_double_float (tv.tv_sec + tv.tv_usec / 1000000.0);
  return make_fixnum (tv.tv_sec);
}

lisp
Fsocket_stream_set_oob_inline (lisp stream, lisp on)
{
  valid_socket_stream_p (stream);
  try
    {
      xsocket_stream_sock (stream)->oobinline (on != Qnil);
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
  return Qt;
}

lisp
Fsocket_stream_send_oob_data (lisp stream, lisp string)
{
  valid_socket_stream_p (stream);
  check_string (string);
  int l = w2sl (string);
  char *s = (char *)alloca (l + 1);  /* w2s writes the terminating NUL */
  w2s (s, string);
  try
    {
      xsocket_stream_sock (stream)->send (s, l, MSG_OOB);
    }
  catch (sock_error &e)
    {
      return FEsocket_error (e.error_code (), e.ope ());
    }
  return Qnil;
}

lisp
Fsocket_stream_ssl_p (lisp stream)
{
  valid_socket_stream_p (stream);
  return boole (xsocket_stream_sock (stream)->sslp ());
}

lisp
Fmake_general_input_stream (lisp io_callback, lisp close_callback,
                            lisp listen_callback)
{
  lisp stream = make_stream (st_general_input);
  xgeneral_stream_io_callback (stream) = io_callback;
  xgeneral_stream_close_callback (stream) =
    close_callback ? close_callback : Qnil;
  xgeneral_input_stream_listen_callback (stream) =
    listen_callback ? listen_callback : Qnil;
  xgeneral_input_stream_string (stream) = Qnil;
  xgeneral_input_stream_index (stream) = 0;
  return stream;
}

lisp
Fmake_general_output_stream (lisp io_callback, lisp close_callback,
                             lisp flush_callback)
{
  lisp stream = make_stream (st_general_output);
  xgeneral_stream_io_callback (stream) = io_callback;
  xgeneral_stream_close_callback (stream) =
    close_callback ? close_callback : Qnil;
  xgeneral_output_stream_flush_callback (stream) =
    flush_callback ? flush_callback : Qnil;
  return stream;
}

lisp
Fclose (lisp stream, lisp keys)
{
  check_stream (stream);
  if (!xstream_open_p (stream))
    return Qnil;

  switch (xstream_type (stream))
    {
    case st_file_input:
      assert (xfile_stream_input (stream));
      assert (!xfile_stream_output (stream));
      fclose (xfile_stream_input (stream));
      xfile_stream_input (stream) = 0;
      break;

    case st_file_output:
      assert (!xfile_stream_input (stream));
      assert (xfile_stream_output (stream));
      {
        file_sink sink = {xfile_stream_output (stream)};
        conv_finish (((lstream *)stream)->conv, sink);
      }
      fclose (xfile_stream_output (stream));
      xfile_stream_output (stream) = 0;
      close_file_stream (stream, find_keyword_bool (Kabort, keys));
      break;

    case st_file_io:
      assert (xfile_stream_input (stream));
      assert (xfile_stream_output (stream));
      {
        file_sink sink = {xfile_stream_output (stream)};
        conv_finish (((lstream *)stream)->conv, sink);
      }
      fclose (xfile_stream_input (stream));
      if (xfile_stream_input (stream) != xfile_stream_output (stream))
        fclose (xfile_stream_output (stream));
      xfile_stream_input (stream) = 0;
      xfile_stream_output (stream) = 0;
      close_file_stream (stream, find_keyword_bool (Kabort, keys));
      break;

    case st_string_input:
      assert (xstring_stream_input (stream) != Qnil);
      assert (xstring_stream_output (stream) == Qnil);
      xstring_stream_input (stream) = Qnil;
      break;

    case st_string_output:
      assert (xstring_stream_input (stream) == Qnil);
      assert (xstring_stream_output (stream) != Qnil);
      xstring_stream_output (stream) = Qnil;
      break;

    case st_synonym:
    case st_two_way:
    case st_echo:
    case st_broadcast:
    case st_concatenated:
      break;

    case st_status:
    case st_keyboard:
    case st_debug_output:
      return Qt;

    case st_wstream:
      xwstream_stream_wstream (stream) = 0;
      break;

    case st_buffer:
      xbuffer_stream_marker (stream) = Qnil;
      xbuffer_stream_eob (stream) = Qnil;
      break;

    case st_socket:
      assert (xsocket_stream_sock (stream));
      close_socket_stream (stream, find_keyword_bool (Kabort, keys));
      break;

    case st_general_input:
    case st_general_output:
      if (xgeneral_stream_close_callback (stream) != Qnil)
        funcall_1 (xgeneral_stream_close_callback (stream),
                   find_keyword (Kabort, keys));
      break;

    default:
      assert (0);
      break;
    }

  xstream_open_p (stream) = 0;
  return Qt;
}

lisp
Fbroadcast_stream_streams (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_broadcast)
    FEtype_error (stream, Qbroadcast_stream);
  return Fcopy_list (xcomposite_stream_output (stream));
}

lisp
Fconcatenated_stream_streams (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_concatenated)
    FEtype_error (stream, Qconcatenated_stream);
  return Fcopy_list (xcomposite_stream_input (stream));
}

lisp
Fecho_stream_input_stream (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_echo)
    FEtype_error (stream, Qecho_stream);
  return xcomposite_stream_input (stream);
}

lisp
Fecho_stream_output_stream (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_echo)
    FEtype_error (stream, Qecho_stream);
  return xcomposite_stream_output (stream);
}

lisp
Ftwo_way_stream_input_stream (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_two_way)
    FEtype_error (stream, Qtwo_way_stream);
  return xcomposite_stream_input (stream);
}

lisp
Ftwo_way_stream_output_stream (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_two_way)
    FEtype_error (stream, Qtwo_way_stream);
  return xcomposite_stream_output (stream);
}

lisp
Fsynonym_stream_symbol (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) != st_synonym)
    FEtype_error (stream, Qsynonym_stream);
  return xcomposite_stream_input (stream);
}

lisp
Finteractive_stream_p (lisp stream)
{
  check_stream (stream);
  if (!xstream_open_p (stream))
    return Qnil;
  switch (xstream_type (stream))
    {
    case st_file_input:
      return boole (_isatty (_fileno (xfile_stream_input (stream))));

    case st_file_output:
      return boole (_isatty (_fileno (xfile_stream_output (stream))));

    case st_file_io:
      return boole (_isatty (_fileno (xfile_stream_input (stream)))
                    && _isatty (_fileno (xfile_stream_output (stream))));

    case st_keyboard:
      return Qt;

    default:
      return Qnil;
    }
}

lisp
Fstream_encoding (lisp stream)
{
  check_stream (stream);
  int encoding;
  switch (xstream_type (stream))
    {
    case st_file_input:
    case st_file_output:
    case st_file_io:
      encoding = xfile_stream_encoding (stream);
      break;

    case st_socket:
      encoding = xsocket_stream_encoding (stream);
      break;

    default:
      return Qnil;
    }

  switch (encoding)
    {
    case lstream::ENCODE_CANON:
      return Kcanonical;

    case lstream::ENCODE_RAW:
      return Kraw;

    default:
      return Kbinary;
    }
}

lisp
Fset_stream_encoding (lisp stream, lisp lencoding)
{
  check_stream (stream);
  int encoding = stream_encoding (lencoding);
  switch (xstream_type (stream))
    {
    case st_file_input:
    case st_file_output:
    case st_file_io:
      xfile_stream_encoding (stream) = encoding;
      return Qt;

    case st_socket:
      xsocket_stream_encoding (stream) = encoding;
      return Qt;

    default:
      return Qnil;
    }
}

lChar
readc_stream (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      if (!xstream_open_p (stream))
        FEtype_error (stream, Qopen_stream);

      if (xstream_type (stream) == st_keyboard)
        {
          check_kbd_enable ();
          return app.kbdq.fetch (0, 0);
        }

      lChar cc = xstream_pending (stream);
      if (cc != lChar_EOF)
        {
          xstream_pending (stream) = lChar_EOF;
          if (cc == '\n')
            xstream_linenum (stream)++;
          return cc;
        }

      switch (xstream_type (stream))
        {
        case st_file_io:
        case st_file_input:
          {
            file_src src = {xfile_stream_input (stream)};
            lChar c;
            if (xfile_stream_encoding (stream) == lstream::ENCODE_BINARY)
              {
                int b = src.get ();
                c = b == EOF ? lChar_EOF : lChar (b);
              }
            else
              c = conv_readc (conv_of (stream), xfile_stream_encoding (stream), src);
            if (c == lChar_EOF)
              return lChar_EOF;
            if (c == '\n')
              xstream_linenum (stream)++;
            return c;
          }

        case st_file_output:
        case st_string_output:
        case st_broadcast:
        case st_status:
        case st_wstream:
        case st_general_output:
        case st_debug_output:
          FEtype_error (stream, Qinput_stream);
          return 0;

        case st_string_input:
          {
            lisp string = xstring_stream_input (stream);
            if (xstring_stream_start (stream) >= xstring_length (string)
                || xstring_stream_start (stream) >= xstring_stream_end (stream))
              return lChar_EOF;
            cc = xstring_contents (string) [xstring_stream_start (stream)++];
            if (cc == '\n')
              xstream_linenum (stream)++;
            return cc;
          }

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_input (stream), selected_buffer ());
          break;

        case st_two_way:
          QUIT;
          stream = xcomposite_stream_input (stream);
          break;

        case st_echo:
          QUIT;
          cc = readc_stream (xcomposite_stream_input (stream));
          if (cc != lChar_EOF)
            {
              if constexpr (XYZZY_CHAR_UNICODE)
                {
                  // A key is not text: it is not echoed.
                  if (text_char_p (cc))
                    writec_stream (xcomposite_stream_output (stream), Char (cc));
                }
              else
                writec_stream (xcomposite_stream_output (stream), text_char (cc));
            }
          if (cc == '\n')
            xstream_linenum (stream)++;
          return cc;

        case st_concatenated:
          while (consp (xcomposite_stream_input (stream)))
            {
              cc = readc_stream (xcar (xcomposite_stream_input (stream)));
              if (cc != lChar_EOF || !consp (xcomposite_stream_input (stream)))
                {
                  if (cc == '\n')
                    xstream_linenum (stream)++;
                  return cc;
                }
              QUIT;
              xcomposite_stream_input (stream) = xcdr (xcomposite_stream_input (stream));
            }
          return lChar_EOF;

        case st_buffer:
          {
            Point point;
            Buffer *bp = buffer_stream_point (stream, point);
            if (point.p_point >= buffer_stream_eob (bp, stream))
              return lChar_EOF;
            xmarker_point (xbuffer_stream_marker (stream)) = point.p_point + 1;
            return point.ch ();
          }

        case st_socket:
          try
            {
              sock_src src = {xsocket_stream_sock (stream)};
              lChar c;
              if (xsocket_stream_encoding (stream) == lstream::ENCODE_BINARY)
                {
                  int b = src.get ();
                  c = b == EOF ? lChar_EOF : lChar (b);
                }
              else
                c = conv_readc (conv_of (stream), xsocket_stream_encoding (stream), src);
              if (c == lChar_EOF)
                return lChar_EOF;
              if (c == '\n')
                xstream_linenum (stream)++;
              return c;
            }
          catch (sock_error &e)
            {
              FEsocket_error (e.error_code (), e.ope ());
            }

        case st_general_input:
          if (!stringp (xgeneral_input_stream_string (stream))
              || (xgeneral_input_stream_index (stream)
                  >= xstring_length (xgeneral_input_stream_string (stream))))
            {
              suppress_gc sgc;
              lisp r = Ffuncall (xgeneral_stream_io_callback (stream), Qnil);
              if (r == Qnil)
                return lChar_EOF;
              if (charp (r))
                {
                  cc = xchar_code (r);
                  if (cc == '\n')
                    xstream_linenum (stream)++;
                  return cc;
                }
              if (!stringp (r))
                FEtype_error (r, xsymbol_value (Qor_string_character));
              if (!xstring_length (r))
                return lChar_EOF;
              xgeneral_input_stream_string (stream) = r;
              xgeneral_input_stream_index (stream) = 0;
            }
          cc = xstring_contents (xgeneral_input_stream_string (stream))
            [xgeneral_input_stream_index (stream)++];
          if (cc == '\n')
            xstream_linenum (stream)++;
          return cc;

        default:
          assert (0);
          return lChar_EOF;
        }
    }
}

int
listen_stream (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      if (!xstream_open_p (stream))
        FEtype_error (stream, Qopen_stream);

      if (xstream_type (stream) == st_keyboard)
        {
          check_kbd_enable ();
          return app.kbdq.listen ();
        }

      lChar cc = xstream_pending (stream);
      if (cc != lChar_EOF)
        return 1;

      switch (xstream_type (stream))
        {
        case st_file_io:
        case st_file_input:
          {
            // UCRT hides FILE internals (no _cnt); regular files never block.
            if (conv_pending_p (((lstream *)stream)->conv))
              return 1;
            HANDLE h = HANDLE (_get_osfhandle (_fileno (xfile_stream_input (stream))));
            if (GetFileType (h) != FILE_TYPE_DISK
                && WaitForSingleObject (h, 0) == WAIT_TIMEOUT)
              return 0;

            int c = getc (xfile_stream_input (stream));
            ungetc (c, xfile_stream_input (stream));
            return c != EOF;
          }

        case st_file_output:
        case st_string_output:
        case st_broadcast:
        case st_status:
        case st_wstream:
        case st_general_output:
        case st_debug_output:
          FEtype_error (stream, Qinput_stream);
          return 0;

        case st_string_input:
          {
            lisp string = xstring_stream_input (stream);
            return (xstring_stream_start (stream) < xstring_length (string)
                    || xstring_stream_start (stream) < xstring_stream_end (stream));
          }

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_input (stream), selected_buffer ());
          break;

        case st_two_way:
        case st_echo:
          QUIT;
          stream = xcomposite_stream_input (stream);
          break;

        case st_concatenated:
          if (!consp (xcomposite_stream_input (stream)))
            return 1;
          QUIT;
          stream = xcar (xcomposite_stream_input (stream));
          break;

        case st_buffer:
          {
            Point point;
            Buffer *bp = buffer_stream_point (stream, point);
            return point.p_point < buffer_stream_eob (bp, stream);
          }

        case st_socket:
          try
            {
              if (conv_pending_p (((lstream *)stream)->conv))
                return 1;
              xsocket_stream_sock (stream)->sflush ();
              return xsocket_stream_sock (stream)->no_hang_p ();
            }
          catch (sock_error &e)
            {
              FEsocket_error (e.error_code (), e.ope ());
            }

        case st_general_input:
          if (stringp (xgeneral_input_stream_string (stream))
              && (xgeneral_input_stream_index (stream)
                  < xstring_length (xgeneral_input_stream_string (stream))))
            return 1;
          if (xgeneral_input_stream_listen_callback (stream) != Qnil)
            {
              suppress_gc sgc;
              return Ffuncall (xgeneral_input_stream_listen_callback (stream), Qnil) != Qnil;
            }
          return 0;

        default:
          assert (0);
          return 0;
        }
    }
}

lChar
peekc_stream (lisp stream)
{
  check_stream (stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);

  lChar cc;
  switch (xstream_type (stream))
    {
    case st_keyboard:
      check_kbd_enable ();
      cc = app.kbdq.fetch (0, 0);
      app.kbdq.push_back (cc);
      break;

    case st_buffer:
      {
        Point point;
        Buffer *bp = buffer_stream_point (stream, point);
        cc = point.p_point < buffer_stream_eob (bp, stream) ? point.ch () : lChar_EOF;
        break;
      }

    default:
      cc = xstream_pending (stream);
      if (cc == lChar_EOF)
        {
          cc = readc_stream (stream);
          xstream_pending (stream) = cc;
        }
      break;
    }

  return cc;
}

int
unreadc_stream (lChar cc, lisp stream)
{
  check_stream (stream);
  if (!xstream_open_p (stream))
    FEtype_error (stream, Qopen_stream);
  switch (xstream_type (stream))
    {
    case st_keyboard:
      app.kbdq.push_back (cc);
      break;

    case st_buffer:
      {
        Point point;
        Buffer *bp = buffer_stream_point (stream, point);
        if (point.p_point <= bp->b_contents.p1)
          return 0;
        xmarker_point (xbuffer_stream_marker (stream)) = point.p_point - 1;
        break;
      }

    default:
      if (xstream_pending (stream) == lChar_EOF)
        {
          xstream_pending (stream) = cc;
          if (cc == '\n')
            xstream_linenum (stream)--;
        }
      else
        return 0;
      break;
    }
  return 1;
}

long
stream_linenum (lisp stream)
{
  check_stream (stream);
  if (xstream_type (stream) == st_buffer)
    {
      Point point;
      Buffer *bp = buffer_stream_point (stream, point);
      return bp->point_linenum (point.p_point);
    }
  else
    return xstream_linenum (stream);
}

static void
put_file_chars (lisp stream, const Char *b, size_t n)
{
  file_sink sink = {xfile_stream_output (stream)};
  int eol = xfile_stream_encoding (stream);
  if (eol == lstream::ENCODE_BINARY)
    for (size_t i = 0; i < n; i++)
      put_binary (b[i], sink);
  else
    conv_write (conv_of (stream), eol, b, n, sink);
}

static void
put_sock_chars (lisp stream, const Char *b, size_t n)
{
  sock_sink sink = {xsocket_stream_sock (stream)};
  int eol = xsocket_stream_encoding (stream);
  if (eol == lstream::ENCODE_BINARY)
    for (size_t i = 0; i < n; i++)
      put_binary (b[i], sink);
  else
    conv_write (conv_of (stream), eol, b, n, sink);
}

void
writec_stream (lisp stream, Char cc)
{
  while (1)
    {
      check_stream (stream);
      if (!xstream_open_p (stream))
        FEtype_error (stream, Qopen_stream);

      switch (xstream_type (stream))
        {
        case st_file_input:
        case st_string_input:
        case st_concatenated:
        case st_keyboard:
        case st_general_input:
          FEtype_error (stream, Qoutput_stream);
          return;

        case st_file_output:
        case st_file_io:
          put_file_chars (stream, &cc, 1);
          xstream_column (stream) = update_column (xstream_column (stream), cc);
          return;

        case st_string_output:
          Fvector_push_extend (make_char (cc), xstring_stream_output (stream),
                               make_fixnum (128));
          xstream_column (stream) = update_column (xstream_column (stream), cc);
          return;

        case st_broadcast:
          {
            for (lisp p = xcomposite_stream_output (stream); consp (p); p = xcdr (p))
              {
                QUIT;
                writec_stream (xcar (p), cc);
              }
            return;
          }

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_output (stream), selected_buffer ());
          break;

        case st_two_way:
        case st_echo:
          QUIT;
          stream = xcomposite_stream_output (stream);
          break;

        case st_status:
          app.status_window.putc (cc);
          xstream_column (stream) = update_column (xstream_column (stream), cc);
          return;

        case st_buffer:
          write_buffer_stream (stream, &cc, 1);
          return;

        case st_wstream:
          if (xwstream_stream_wstream (stream))
            xwstream_stream_wstream (stream)->add (cc);
          return;

        case st_socket:
          try
            {
              put_sock_chars (stream, &cc, 1);
              xstream_column (stream) = update_column (xstream_column (stream), cc);
            }
          catch (sock_error &e)
            {
              FEsocket_error (e.error_code (), e.ope ());
            }
          return;

        case st_general_output:
          funcall_1 (xgeneral_stream_io_callback (stream),
                     make_string (&cc, 1));
          xstream_column (stream) = update_column (xstream_column (stream), cc);
          return;

        case st_debug_output:
          Debug (&cc, 1);
          return;

        default:
          assert (0);
          return;
        }
    }
}

void
write_stream (lisp stream, const Char *b, size_t size)
{
  while (1)
    {
      check_stream (stream);
      if (!xstream_open_p (stream))
        FEtype_error (stream, Qopen_stream);

      switch (xstream_type (stream))
        {
        case st_file_input:
        case st_string_input:
        case st_concatenated:
        case st_keyboard:
        case st_general_input:
          FEtype_error (stream, Qoutput_stream);
          return;

        case st_file_output:
        case st_file_io:
          {
            put_file_chars (stream, b, size);
            xstream_column (stream) = update_column (xstream_column (stream), b, size);
            return;
          }

        case st_string_output:
          {
            lisp string = xstring_stream_output (stream);
            int need = xstring_length (string) + size;
            int room = xstring_dimension (string);
            if (need > room)
              {
                if (!xarray_adjustable (string))
                  FEprogram_error (Evector_is_not_adjustable, string);
                need = (need + 128) & ~127;
                realloc_element (string, need - room, sizeof (Char));
              }
            bcopy (b, &xstring_contents (string) [xstring_length (string)], size);
            xstring_length (string) += size;
            xstream_column (stream) = update_column (xstream_column (stream), b, size);
            return;
          }

        case st_broadcast:
          {
            for (lisp p = xcomposite_stream_output (stream); consp (p); p = xcdr (p))
              {
                QUIT;
                write_stream (xcar (p), b, size);
              }
            return;
          }

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_output (stream), selected_buffer ());
          break;

        case st_two_way:
        case st_echo:
          QUIT;
          stream = xcomposite_stream_output (stream);
          break;

        case st_status:
          app.status_window.puts (b, size);
          xstream_column (stream) = update_column (xstream_column (stream), b, size);
          return;

        case st_buffer:
          write_buffer_stream (stream, b, size);
          return;

        case st_wstream:
          if (xwstream_stream_wstream (stream))
            xwstream_stream_wstream (stream)->add (b, size);
          return;

        case st_socket:
          try
            {
              put_sock_chars (stream, b, size);
              xstream_column (stream) = update_column (xstream_column (stream), b, size);
            }
          catch (sock_error &e)
            {
              FEsocket_error (e.error_code (), e.ope ());
            }
          return;

        case st_general_output:
          funcall_1 (xgeneral_stream_io_callback (stream),
                     make_string (b, size));
          xstream_column (stream) = update_column (xstream_column (stream), b, size);
          return;

        case st_debug_output:
          Debug (b, size);
          return;

        default:
          assert (0);
          return;
        }
    }
}

int
get_stream_column (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      switch (xstream_type (stream))
        {
        case st_file_input:
        case st_string_input:
        case st_concatenated:
        case st_keyboard:
        case st_general_input:
          FEtype_error (stream, Qoutput_stream);
          return 0;

        case st_file_output:
        case st_file_io:
        case st_string_output:
        case st_status:
        case st_socket:
        case st_general_output:
          return xstream_column (stream);

        case st_wstream:
          if (xwstream_stream_wstream (stream))
            return xwstream_stream_wstream (stream)->columns ();
          return 0;

        case st_broadcast:
          if (!consp (xcomposite_stream_output (stream)))
            return 0;
          QUIT;
          stream = xcar (xcomposite_stream_output (stream));
          break;

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_output (stream), selected_buffer ());
          break;

        case st_two_way:
        case st_echo:
          QUIT;
          stream = xcomposite_stream_output (stream);
          break;

        case st_buffer:
          {
            Point point;
            Buffer *bp = buffer_stream_point (stream, point);
            return bp->point_column (point);
          }

        case st_debug_output:
          return 0;

        default:
          assert (0);
          return 0;
        }
    }
}

void
flush_stream (lisp stream)
{
  while (1)
    {
      check_stream (stream);
      if (!xstream_open_p (stream))
        FEtype_error (stream, Qopen_stream);
      switch (xstream_type (stream))
        {
        case st_file_input:
        case st_string_input:
        case st_concatenated:
        case st_keyboard:
        case st_general_input:
          FEtype_error (stream, Qoutput_stream);
          return;

        case st_file_output:
        case st_file_io:
          fflush (xfile_stream_output (stream));
          return;

        case st_string_output:
          return;

        case st_broadcast:
          {
            for (lisp p = xcomposite_stream_output (stream); consp (p); p = xcdr (p))
              {
                QUIT;
                flush_stream (xcar (p));
              }
            return;
          }

        case st_synonym:
          QUIT;
          stream = symbol_value (xcomposite_stream_output (stream), selected_buffer ());
          break;

        case st_two_way:
        case st_echo:
          QUIT;
          stream = xcomposite_stream_output (stream);
          break;

        case st_status:
          app.status_window.flush ();
          return;

        case st_buffer:
          return;

        case st_wstream:
          return;

        case st_socket:
          try
            {
              xsocket_stream_sock (stream)->sflush ();
            }
          catch (sock_error &e)
            {
              FEsocket_error (e.error_code (), e.ope ());
            }
          return;

        case st_general_output:
          if (xgeneral_output_stream_flush_callback (stream) != Qnil)
            {
              suppress_gc sgc;
              Ffuncall (xgeneral_output_stream_flush_callback (stream), Qnil);
            }
          return;

        case st_debug_output:
          return;

        default:
          assert (0);
          return;
        }
    }
}

static lisp
create_std_stream (stream_type type, FILE *fi, FILE *fo)
{
  lisp stream = make_file_stream (type);
  xfile_stream_input (stream) = fi;
  xfile_stream_output (stream) = fo;
  return stream;
}

void
create_std_streams ()
{
  xsymbol_value (Vterminal_io) = create_std_stream (st_file_io, stdin, stdout);
#if 0
  xsymbol_value (Vstandard_input) = create_std_stream (st_file_input, stdin, 0);
  xsymbol_value (Vstandard_output) = create_std_stream (st_file_output, 0, stdout);
#else
  xsymbol_value (Qkeyboard) = make_keyboard_stream ();
  xsymbol_value (Vstandard_input) = xsymbol_value (Qkeyboard);
  xsymbol_value (Vstatus_window) = make_status_stream ();
  xsymbol_value (Vstandard_output) = xsymbol_value (Vstatus_window);
  xsymbol_value (Vdebug_output) = make_debug_output_stream ();
#endif
  xsymbol_value (Verror_output) = create_std_stream (st_file_output, 0, stderr);
  xsymbol_value (Vquery_io) = xsymbol_value (Vterminal_io);
  xsymbol_value (Vdebug_io) = xsymbol_value (Vterminal_io);
  xsymbol_value (Vtrace_output) = xsymbol_value (Vdebug_io);
  xsymbol_value (Vwstream_stream) = make_wstream_stream ();
}
