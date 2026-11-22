#ifndef _byte_stream_h_
#define _byte_stream_h_

#include <vector>
#include "StrBuf.h"
#include "encoding.h"
#include "codec.h"

typedef xoutput_stream <u_char> byte_output_stream;
typedef xinput_stream <u_char> byte_input_stream;
typedef xoutput_stream <Char> Char_output_stream;
typedef xinput_stream <Char> Char_input_stream;

// The octets of characters, by the rule of "a string of bytes" (design 4.7).
// This is the side of XYZZY_CHAR_UNICODE 1: a Char is a code point.
//
// - With an encoding (a char-encoding object), the characters are converted
//   by the converter of that encoding.
// - Without it:
//   - a string whose characters are all U+00FF or below (or raw byte
//     characters) is a string of bytes: one character is one byte;
//   - any other string is converted by *default-fileio-encoding*;
//   - characters read from a stream are decided one by one: U+00FF or below
//     is a byte, and the others go to the converter of
//     *default-fileio-encoding*.
// A raw byte character is always written as its own byte.
class octet_encoder
{
  lisp o_encoding;              // 0: not given
  char_encoder *o_enc;
  int o_mode;                   // 0: one by one, 1: bytes, 2: the converter
  void make ();
  void encode (const Char *, size_t, std::vector <u_char> &);
public:
  octet_encoder (lisp encoding = 0);
  ~octet_encoder ();
  // The whole string is known. Decides between bytes and the converter.
  void scan (const Char *, size_t);
  void convert (const Char *, size_t, std::vector <u_char> &);
  void finish (std::vector <u_char> &);
};

class byte_input_string_stream: public byte_input_stream
{
  u_char s_buf[1024];
  const Char *s_wp;
  const Char *const s_we;
  octet_encoder s_oe;
  std::vector <u_char> s_out;
  bool s_finished;
  virtual int refill ();
public:
  byte_input_string_stream (const Char *b, int l, lisp encoding = 0)
       : s_wp (b), s_we (b + l), s_oe (encoding), s_finished (false)
    {s_oe.scan (b, l);}
  byte_input_string_stream (lisp string, lisp encoding = 0)
       : s_wp (xstring_contents (string)),
         s_we (xstring_contents (string) + xstring_length (string)),
         s_oe (encoding), s_finished (false)
    {s_oe.scan (s_wp, s_we - s_wp);}
};

class byte_input_streams_stream: public byte_input_stream
{
  u_char s_buf[1024];
  lisp s_stream;
  octet_encoder s_oe;
  std::vector <u_char> s_out;
  bool s_finished;
  virtual int refill ();
public:
  byte_input_streams_stream (lisp stream, lisp encoding = 0)
       : s_stream (stream), s_oe (encoding), s_finished (false) {}
};

class byte_output_wstream: public byte_output_stream
{
  u_char s_buf[1024];
protected:
  byte_output_wstream () : byte_output_stream (s_buf, s_buf + sizeof s_buf) {}
  virtual u_char *sflush (u_char *, u_char *, int);
  virtual void swrite (const Char *, int) = 0;
};

class byte_output_string_stream: public byte_output_wstream, public StrBuf
{
  char s_buf[2040];
  virtual void swrite (const Char *b, int l) {add (b, l);}
public:
  byte_output_string_stream () : StrBuf (s_buf, sizeof s_buf) {}
};

class byte_output_streams_stream: public byte_output_wstream
{
  lisp s_stream;
  virtual void swrite (const Char *b, int l)
    {write_stream (s_stream, b, l);}
public:
  byte_output_streams_stream (lisp stream) : s_stream (stream) {}
};

class Char_input_string_stream: public Char_input_stream
{
public:
  Char_input_string_stream (const Char *b, int l)
       : Char_input_stream (b, b + l) {}
  Char_input_string_stream (lisp string)
       : Char_input_stream (xstring_contents (string),
                            xstring_contents (string) + xstring_length (string)) {}
};

class Char_input_streams_stream: public Char_input_stream
{
  Char s_buf[1024];
  lisp s_stream;
  virtual int refill ();
public:
  Char_input_streams_stream (lisp stream) : s_stream (stream) {}
};

class Char_output_wstream: public Char_output_stream
{
  Char s_buf[1024];
protected:
  Char_output_wstream () : Char_output_stream (s_buf, s_buf + numberof (s_buf)) {}
  virtual Char *sflush (Char *b, Char *e, int)
    {if (b != e) swrite (b, e - b); return b;}
  virtual void swrite (const Char *, int) = 0;
};

class Char_output_string_stream: public Char_output_wstream, public StrBuf
{
  char s_buf[2040];
  virtual void swrite (const Char *b, int l) {add (b, l);}
public:
  Char_output_string_stream () : StrBuf (s_buf, sizeof s_buf) {}
};

class Char_output_streams_stream: public Char_output_wstream
{
  lisp s_stream;
  virtual void swrite (const Char *b, int l)
    {write_stream (s_stream, b, l);}
public:
  Char_output_streams_stream (lisp stream) : s_stream (stream) {}
};

class xstream_obyte_helper
{
  union
    {
      XBUFDEF (byte_output_string_stream);
      XBUFDEF (byte_output_streams_stream);
    } s_xbuf;
  byte_output_stream *s_stream;
  int s_string_stream_p;
public:
  xstream_obyte_helper (lisp);
  ~xstream_obyte_helper () {delete s_stream;}
  operator byte_output_stream & () const {return *s_stream;}
  byte_output_stream *operator -> () const {return s_stream;}
  lisp result () const
    {return (s_string_stream_p
             ? static_cast <byte_output_string_stream *> (s_stream)->make_string () : Qt);}
};

class xstream_iChar_helper
{
  union
    {
      XBUFDEF (Char_input_string_stream);
      XBUFDEF (Char_input_streams_stream);
    } s_xbuf;
  Char_input_stream *s_stream;
public:
  xstream_iChar_helper (lisp);
  ~xstream_iChar_helper () {delete s_stream;}
  operator Char_input_stream & () const {return *s_stream;}
};

class xstream_oChar_helper
{
  union
    {
      XBUFDEF (Char_output_string_stream);
      XBUFDEF (Char_output_streams_stream);
    } s_xbuf;
  Char_output_stream *s_stream;
  int s_string_stream_p;
public:
  xstream_oChar_helper (lisp);
  ~xstream_oChar_helper () {delete s_stream;}
  operator Char_output_stream & () const {return *s_stream;}
  lisp result () const
    {return (s_string_stream_p
             ? static_cast <Char_output_string_stream *> (s_stream)->make_string () : Qt);}
};

// The octets of characters by the old internal code with an encoding
// (XYZZY_CHAR_UNICODE 0): the same as convert-encoding-from-internal.
class byte_input_encoded_stream: public byte_input_stream
{
  u_char s_buf[1024];
  xstream_iChar_helper s_ichar;
  encoding_output_stream_helper s_enc;
  virtual int refill ();
public:
  byte_input_encoded_stream (lisp obj, lisp encoding)
       : s_ichar (obj), s_enc (encoding, s_ichar, eol_noconv) {}
};

// The input of the functions that handle octets (base64, md5, ...).
// obj is a string or an input stream. encoding (0 or nil: not given) tells
// how the characters become octets (see octet_encoder).
class xstream_ibyte_helper
{
  union
    {
      XBUFDEF (byte_input_string_stream);
      XBUFDEF (byte_input_streams_stream);
      XBUFDEF (byte_input_encoded_stream);
    } s_xbuf;
  byte_input_stream *s_stream;
public:
  xstream_ibyte_helper (lisp, lisp encoding = 0);
  ~xstream_ibyte_helper () {delete s_stream;}
  operator byte_input_stream & () const {return *s_stream;}
  byte_input_stream *operator -> () const {return s_stream;}
};

// The octets of a string (see octet_encoder; encoding 0: not given).
void string_to_octets (lisp string, lisp encoding, std::vector <u_char> &);

#undef MAX__

void copy_xstream (xread_stream &, byte_output_stream &);
void copy_xstream (xread_stream &, Char_output_stream &);
void copy_xstream (xwrite_stream &, byte_output_stream &);

#endif /* _byte_stream_h_ */
