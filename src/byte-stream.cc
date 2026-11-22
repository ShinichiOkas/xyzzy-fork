#include "stdafx.h"
#include "ed.h"
#include "byte-stream.h"

// ---------------------------------------------------------------------------
// octet_encoder

static inline bool
octet_byte_p (Char c)
{
  return c <= 0xff || codec_raw_byte_p (c);
}

static inline u_char
octet_byte (Char c)
{
  return u_char (c <= 0xff ? c : c - CODEC_RAW_BYTE_BASE);
}

octet_encoder::octet_encoder (lisp encoding)
  : o_encoding (0), o_enc (0), o_mode (0)
{
  if (encoding && encoding != Qnil)
    {
      check_char_encoding (encoding);
      if (xchar_encoding_type (encoding) == encoding_auto_detect)
        FEtype_error (encoding, Qchar_encoding);
      o_encoding = encoding;
      o_mode = 2;
      make ();
    }
}

octet_encoder::~octet_encoder ()
{
  delete o_enc;
}

void
octet_encoder::make ()
{
  if (o_enc)
    return;
  if (o_encoding)
    {
      o_enc = make_encoder (o_encoding);
      if (!o_enc)
        FEtype_error (o_encoding, Qchar_encoding);
      return;
    }
  o_enc = make_encoder (symbol_value_char_encoding (Vdefault_fileio_encoding));
  if (!o_enc)
    o_enc = make_encoder (xsymbol_value (Qencoding_sjis));
}

void
octet_encoder::scan (const Char *s, size_t n)
{
  if (o_encoding)
    return;
  o_mode = 1;
  for (const Char *se = s + n; s < se; s++)
    if (!octet_byte_p (*s))
      {
        o_mode = 2;
        break;
      }
}

void
octet_encoder::encode (const Char *s, size_t n, std::vector <u_char> &out)
{
  make ();
  const codec_char *cs = reinterpret_cast <const codec_char *> (s);
  const codec_char *cse = cs + n;
  u_char buf[1024];
  while (cs < cse)
    {
      u_char *dst = buf;
      const codec_char *cs0 = cs;
      o_enc->encode (cs, cse, dst, buf + sizeof buf);
      out.insert (out.end (), buf, dst);
      if (cs == cs0 && dst == buf)
        break;
    }
}

void
octet_encoder::convert (const Char *s, size_t n, std::vector <u_char> &out)
{
  if (o_mode == 2)
    {
      encode (s, n, out);
      return;
    }
  if (o_mode == 1)
    {
      for (const Char *se = s + n; s < se; s++)
        out.push_back (octet_byte (*s));
      return;
    }
  // One by one: a run of bytes, and a run of the others.
  const Char *se = s + n;
  while (s < se)
    {
      const Char *p = s;
      bool b = octet_byte_p (*p);
      while (p < se && octet_byte_p (*p) == b)
        p++;
      if (b)
        for (; s < p; s++)
          out.push_back (octet_byte (*s));
      else
        encode (s, p - s, out);
      s = p;
    }
}

void
octet_encoder::finish (std::vector <u_char> &out)
{
  if (!o_enc)
    return;
  u_char buf[CODEC_ENCODE_ROOM * 2];
  u_char *dst = buf;
  o_enc->finish (dst, buf + sizeof buf);
  out.insert (out.end (), buf, dst);
}

void
string_to_octets (lisp string, lisp encoding, std::vector <u_char> &out)
{
  check_string (string);
  xstream_ibyte_helper is (string, encoding);
  int c;
  while ((c = is->get ()) != xstream::eof)
    out.push_back (u_char (c));
}

int
byte_input_string_stream::refill ()
{
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      while (1)
        {
          s_out.clear ();
          if (s_wp < s_we)
            {
              size_t n = s_we - s_wp;
              if (n > 256)
                n = 256;
              s_oe.convert (s_wp, n, s_out);
              s_wp += n;
            }
          else if (!s_finished)
            {
              s_finished = true;
              s_oe.finish (s_out);
            }
          else
            return setbuf (s_buf, s_buf);
          if (!s_out.empty ())
            return setbuf (&s_out[0], &s_out[0] + s_out.size ());
        }
    }
  u_char *b = s_buf, *const be = s_buf + sizeof s_buf - 1;
  while (b < be && s_wp < s_we)
    {
      Char cc = *s_wp++;
      if (DBCP (cc))
        *b++ = u_char (cc >> 8);
      *b++ = u_char (cc);
    }
  return setbuf (s_buf, b);
}

int
byte_input_encoded_stream::refill ()
{
  u_char *b = s_buf, *const be = s_buf + sizeof s_buf;
  while (b < be)
    {
      int c = s_enc->get ();
      if (c == xstream::eof)
        break;
      *b++ = u_char (c);
    }
  return setbuf (s_buf, b);
}

int
byte_input_streams_stream::refill ()
{
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      while (1)
        {
          s_out.clear ();
          if (!s_finished)
            {
              Char chars[32];
              int n = 0;
              while (n < int (numberof (chars)))
                {
                  lChar lcc = readc_stream (s_stream);
                  if (lcc == lChar_EOF)
                    {
                      s_finished = true;
                      break;
                    }
                  chars[n++] = text_char (lcc);
                }
              s_oe.convert (chars, n, s_out);
              if (s_finished)
                s_oe.finish (s_out);
            }
          else
            return setbuf (s_buf, s_buf);
          if (!s_out.empty ())
            return setbuf (&s_out[0], &s_out[0] + s_out.size ());
        }
    }
  u_char *b = s_buf, *const be = s_buf + sizeof s_buf - 1;
  while (b < be)
    {
      lChar lcc = readc_stream (s_stream);
      if (lcc == lChar_EOF)
        break;
      if (DBCP (text_char (lcc)))
        *b++ = u_char (lcc >> 8);
      *b++ = u_char (lcc);
    }
  return setbuf (s_buf, b);
}

u_char *
byte_output_wstream::sflush (u_char *b0, u_char *be, int eofp)
{
  u_char *b = b0;
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      // A string of bytes: one byte is one character (U+0000..U+00FF).
      Char wbuf[sizeof s_buf];
      Char *w = wbuf;
      while (b < be)
        *w++ = *b++;
      if (w != wbuf)
        swrite (wbuf, int (w - wbuf));
      return b0;
    }
  Char *w, wbuf[sizeof s_buf];
  for (w = wbuf; b < be;)
    {
      if (SJISP (*b))
        {
          if (b + 1 == be)
            {
              if (eofp)
                *w++ = *b;
              else
                *b0++ = *b;
              break;
            }
          *w++ = (*b << 8) | b[1];
          b += 2;
        }
      else
        *w++ = *b++;
    }
  if (w - wbuf)
    swrite (wbuf, w - wbuf);
  return b0;
}

int
Char_input_streams_stream::refill ()
{
  Char *b = s_buf, *const be = s_buf + numberof (s_buf);
  while (b < be)
    {
      lChar lcc = readc_stream (s_stream);
      if (lcc == lChar_EOF)
        break;
      *b++ = text_char (lcc);
    }
  return setbuf (s_buf, b);
}

xstream_ibyte_helper::xstream_ibyte_helper (lisp obj, lisp encoding)
{
  bool enc = encoding && encoding != Qnil;
  if (enc)
    {
      check_char_encoding (encoding);
      if (xchar_encoding_type (encoding) == encoding_auto_detect)
        FEtype_error (encoding, Qchar_encoding);
    }
  if (!(stringp (obj) || obj == Qnil || obj == Qt || streamp (obj)))
    FEtype_error (obj, xsymbol_value (Qor_string_stream));

  if (enc && !XYZZY_CHAR_UNICODE)
    {
      // The old internal code: through the stream of the encoding.
      s_stream = new (&s_xbuf) byte_input_encoded_stream
        (stringp (obj) ? obj : input_stream (obj), encoding);
      return;
    }
  if (stringp (obj))
    s_stream = new (&s_xbuf) byte_input_string_stream (obj, enc ? encoding : 0);
  else
    s_stream = new (&s_xbuf) byte_input_streams_stream (input_stream (obj),
                                                         enc ? encoding : 0);
}

xstream_obyte_helper::xstream_obyte_helper (lisp obj)
{
  if (!obj || obj == Qnil)
    {
      s_stream = new (&s_xbuf) byte_output_string_stream;
      s_string_stream_p = 1;
    }
  else
    {
      if (obj == Qt)
        obj = xsymbol_value (Vstandard_output);
      check_stream (obj);
      s_stream = new (&s_xbuf) byte_output_streams_stream (obj);
      s_string_stream_p = 0;
    }
}

xstream_iChar_helper::xstream_iChar_helper (lisp obj)
{
  if (stringp (obj))
    s_stream = new (&s_xbuf) Char_input_string_stream (obj);
  else if (obj == Qnil || obj == Qt || streamp (obj))
    s_stream = new (&s_xbuf) Char_input_streams_stream (input_stream (obj));
  else
    FEtype_error (obj, xsymbol_value (Qor_string_stream));
}

xstream_oChar_helper::xstream_oChar_helper (lisp obj)
{
  if (!obj || obj == Qnil)
    {
      s_stream = new (&s_xbuf) Char_output_string_stream;
      s_string_stream_p = 1;
    }
  else
    {
      if (obj == Qt)
        obj = xsymbol_value (Vstandard_output);
      check_stream (obj);
      s_stream = new (&s_xbuf) Char_output_streams_stream (obj);
      s_string_stream_p = 0;
    }
}

void
copy_xstream (xread_stream &i, byte_output_stream &o)
{
  int c;
  while ((c = i.get ()) != xstream::eof)
    o.put (c);
  o.flush (1);
}

void
copy_xstream (xread_stream &is, Char_output_stream &os)
{
  int c;
  while ((c = is.get ()) != xstream::eof)
    os.put (c);
  os.flush (1);
}

void
copy_xstream (xwrite_stream &is, byte_output_stream &os)
{
  int c;
  while ((c = is.get ()) != xstream::eof)
    os.put (c);
  os.flush (1);
}
