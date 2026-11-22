// -*-C++-*-
// The codec layer: converters between byte sequences and Unicode code points.
//
// - A converter knows only bytes and code points (32 bit). It does not know
//   Char (16 bit today), the internal character code of xyzzy, or the end of
//   line conversion. The tables are generated from src/unicode/*.TXT
//   (gen-codec.cc) and never depend on the code page of the OS.
// - See docs/refactoring/s4-char-design.md sections 4 and 5.
#ifndef _codec_h_
#define _codec_h_

#include <vector>
#include "ces.h"

typedef u_int32_t codec_char;

// A byte that could not be converted (an invalid byte, or a code that has no
// definition in the character set) is kept as a "raw byte" character. Only
// the bytes 0x80..0xff become raw byte characters; an invalid byte below
// 0x80 is kept as the character of the same value. Any encoder writes a raw
// byte character back as the original byte.
#define CODEC_RAW_BYTE_BASE 0x110000
#define CODEC_RAW_BYTE_MIN 0x110080
#define CODEC_RAW_BYTE_MAX 0x1100ff

inline bool
codec_raw_byte_p (codec_char c)
{
  return c >= CODEC_RAW_BYTE_MIN && c <= CODEC_RAW_BYTE_MAX;
}

inline codec_char
codec_raw_byte (u_char b)
{
  return b >= 0x80 ? CODEC_RAW_BYTE_BASE + b : codec_char (b);
}

// The character that an encoder writes for a character it cannot encode.
#define CODEC_DEFCHAR '?'

// decode () and encode () stop when the room of the output is smaller than
// these. A caller must pass an output at least this large to make progress.
#define CODEC_DECODE_ROOM 16
#define CODEC_ENCODE_ROOM 48

// Memory of the duplicated codes of cp932 (section 5.4 of the design).
//
// 398 codes of cp932 are not the code that Windows writes for the same
// character (NEC selected IBM extension ED/EE, IBM extension FA..FC and NEC
// row 13 duplicates). A decoder tells the memory "this character was read
// as this code" for such a code, and an encoder looks the memory up before it
// takes the canonical code. This keeps the bytes of a file that is opened and
// saved. A converter works without a context: then it is exactly the
// normalization of Windows.
class codec_context
{
public:
  virtual ~codec_context () {}
  virtual int dup_lookup (codec_char u) = 0;            // -1: no memory
  virtual void dup_remember (codec_char u, int code) = 0;
};

// The memory itself (at most 396 entries). The first one that is remembered
// for a character stays.
class dup_memory: public codec_context
{
  struct entry {codec_char u; int code;};
  std::vector <entry> m_v;
public:
  virtual int dup_lookup (codec_char u);
  virtual void dup_remember (codec_char u, int code);
  size_t size () const {return m_v.size ();}
  codec_char key (size_t i) const {return m_v[i].u;}
  int value (size_t i) const {return m_v[i].code;}
  void clear () {m_v.clear ();}
};

// Bytes -> code points.
class char_decoder
{
public:
  virtual ~char_decoder () {}
  // Converts as much of [src, se) as possible and writes to dst. src and dst
  // are advanced. It returns when the input is used up, when the room of dst
  // is smaller than CODEC_DECODE_ROOM, or (final = false) when the input ends
  // in the middle of a sequence; the bytes of that sequence stay in the input.
  // With final = true, a sequence cut at the end of the input becomes raw
  // bytes.
  virtual void decode (const u_char *&src, const u_char *se,
                       codec_char *&dst, codec_char *de, bool final) = 0;
  // UTF with a signature: a byte order mark was found and dropped.
  virtual bool has_signature () const {return false;}
};

// Code points -> bytes.
class char_encoder
{
public:
  virtual ~char_encoder () {}
  // Converts as much of [src, se) as possible and writes to dst. It returns
  // when the input is used up or when the room of dst is smaller than
  // CODEC_ENCODE_ROOM. A character that cannot be encoded becomes '?'.
  virtual void encode (const codec_char *&src, const codec_char *se,
                       u_char *&dst, u_char *de) = 0;
  // Writes what is pending at the end of the text (the return to ASCII of
  // ISO-2022, the end of a UTF-7 shift). The room of dst must be at least
  // CODEC_ENCODE_ROOM.
  virtual void finish (u_char *&dst, u_char *de) {}
  virtual bool can_encode (codec_char) const = 0;
};

// What a converter is made from. It is the part of a char-encoding object
// that the converters read.
struct codec_spec
{
  encoding_type type;
  int flags;                    // ENCODING_ISO_* or ENCODING_UTF_* | UTF7_*
  u_int designatable[4];
  u_char initial[4];
  int lang;                     // ENCODING_LANG_*
  int vender;                   // ENCODING_ISO_VENDER_* (resolved)
  int iso8859_charset;          // ccs_iso8859_*
  int windows_codepage;
  bool utf16_default_big_endian;
};

// Fills spec from a char-encoding object. It returns false for the encodings
// that have no converter (auto and a few unused Windows code pages).
bool codec_spec_from_encoding (lisp encoding, codec_spec &spec);

// Both return 0 when the encoding has no converter. The context is used by
// the cp932 converters only. A context must live as long as the converter.
char_decoder *make_decoder (const codec_spec &, codec_context * = 0);
char_encoder *make_encoder (const codec_spec &, codec_context * = 0);
char_decoder *make_decoder (lisp encoding, codec_context * = 0);
char_encoder *make_encoder (lisp encoding, codec_context * = 0);

// Converts a whole sequence. The result is appended to out. With chunk > 0
// the converter gets the input in pieces of that size, as a stream does with
// a buffer; the result must be the same.
void codec_decode_all (char_decoder *, const u_char *, size_t,
                       std::vector <codec_char> &out, size_t chunk = 0);
void codec_encode_all (char_encoder *, const codec_char *, size_t,
                       std::vector <u_char> &out, size_t chunk = 0);

// Lisp: (si:codec-decode encoding octets [memory]) etc. See codec-fns.cc.

#endif
