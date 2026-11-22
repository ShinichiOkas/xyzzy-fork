// Lisp functions that call the codec layer directly. They are the user
// interface of the codec layer before it is connected to the readers and
// writers (the tests use them).
//
//   (si:codec-decode encoding octets [memory [chunk]])
//       => code-point vector, memory
//   (si:codec-encode encoding code-points [memory [chunk]])
//       => octet vector
//   (si:codec-can-encode encoding code-point)
//       => t or nil
//
// octets and code-points are lists or vectors of integers. The code points
// are Unicode (0..#x10ffff); #x110080..#x1100ff are raw bytes.
//
// memory is the memory of the duplicated codes of cp932 (codec.h): an alist
// of (code-point . cp932-code). Without it (omitted, or t) a converter is the
// normalization of Windows. With it (nil is the empty memory) a decoder
// returns the memory with the codes that it read, and an encoder writes by
// the memory.
//
// chunk gives the input to the converter in pieces of that many bytes (code
// points): the way a stream uses it. The result is the same as without it.
#include "stdafx.h"
#include "ed.h"
#include "vector.h"
#include "list.h"
#include "codec-priv.h"

static void
get_integers (lisp x, long min, long max, std::vector <long> &v)
{
  if (simple_vector_p (x))
    {
      int n = xvector_length (x);
      for (int i = 0; i < n; i++)
        v.push_back (fixnum_value (xvector_contents (x)[i]));
    }
  else if (listp (x))
    {
      for (; consp (x); x = xcdr (x))
        {
          QUIT;
          v.push_back (fixnum_value (xcar (x)));
        }
    }
  else
    FEtype_error (x, Qsequence);

  for (size_t i = 0; i < v.size (); i++)
    if (v[i] < min || v[i] > max)
      FErange_error (make_fixnum (v[i]));
}

static size_t
chunk_size (lisp chunk)
{
  if (!chunk || chunk == Qnil)
    return 0;
  long n = fixnum_value (chunk);
  if (n < 1)
    FErange_error (chunk);
  return size_t (n);
}

static void
load_memory (lisp memory, dup_memory &dm)
{
  for (lisp p = memory; consp (p); p = xcdr (p))
    {
      lisp e = xcar (p);
      if (!consp (e))
        FEtype_error (e, Qcons);
      long u = fixnum_value (xcar (e));
      long code = fixnum_value (xcdr (e));
      if (u < 0 || code < 0 || code > 0xffff)
        FErange_error (e);
      dm.dup_remember (codec_char (u), int (code));
    }
}

static lisp
memory_to_alist (const dup_memory &dm)
{
  lisp r = Qnil;
  for (size_t i = dm.size (); i > 0; i--)
    r = xcons (xcons (make_fixnum (dm.key (i - 1)),
                      make_fixnum (dm.value (i - 1))), r);
  return r;
}

lisp
Fsi_codec_decode (lisp encoding, lisp octets, lisp memory, lisp chunk)
{
  check_char_encoding (encoding);
  std::vector <long> in;
  get_integers (octets, 0, 255, in);

  codec_spec spec;
  if (!codec_spec_from_encoding (encoding, spec))
    FEtype_error (encoding, Qchar_encoding);

  dup_memory dm;
  bool use_memory = memory != 0 && memory != Qt;
  if (use_memory)
    load_memory (memory, dm);

  char_decoder *d = make_decoder (spec, use_memory ? &dm : 0);
  if (!d)
    FEtype_error (encoding, Qchar_encoding);

  std::vector <u_char> bytes (in.size ());
  for (size_t i = 0; i < in.size (); i++)
    bytes[i] = u_char (in[i]);
  std::vector <codec_char> out;
  codec_decode_all (d, bytes.empty () ? 0 : &bytes[0], bytes.size (), out, chunk_size (chunk));
  delete d;

  lisp r = make_vector (int (out.size ()), Qnil);
  for (size_t i = 0; i < out.size (); i++)
    xvector_contents (r)[i] = make_fixnum (out[i]);
  multiple_value::count () = 2;
  multiple_value::value (1) = use_memory ? memory_to_alist (dm) : Qnil;
  return r;
}

lisp
Fsi_codec_encode (lisp encoding, lisp points, lisp memory, lisp chunk)
{
  check_char_encoding (encoding);
  std::vector <long> in;
  get_integers (points, 0, 0x1fffff, in);

  codec_spec spec;
  if (!codec_spec_from_encoding (encoding, spec))
    FEtype_error (encoding, Qchar_encoding);

  dup_memory dm;
  bool use_memory = memory != 0 && memory != Qt;
  if (use_memory)
    load_memory (memory, dm);

  char_encoder *e = make_encoder (spec, use_memory ? &dm : 0);
  if (!e)
    FEtype_error (encoding, Qchar_encoding);

  std::vector <codec_char> chars (in.size ());
  for (size_t i = 0; i < in.size (); i++)
    chars[i] = codec_char (in[i]);
  std::vector <u_char> out;
  codec_encode_all (e, chars.empty () ? 0 : &chars[0], chars.size (), out, chunk_size (chunk));
  delete e;

  lisp r = make_vector (int (out.size ()), Qnil);
  for (size_t i = 0; i < out.size (); i++)
    xvector_contents (r)[i] = make_fixnum (out[i]);
  return r;
}

lisp
Fsi_codec_can_encode (lisp encoding, lisp code)
{
  check_char_encoding (encoding);
  long c = fixnum_value (code);
  if (c < 0 || c > 0x1fffff)
    FErange_error (code);

  codec_spec spec;
  if (!codec_spec_from_encoding (encoding, spec))
    FEtype_error (encoding, Qchar_encoding);
  char_encoder *e = make_encoder (spec, 0);
  if (!e)
    FEtype_error (encoding, Qchar_encoding);
  bool r = e->can_encode (codec_char (c));
  delete e;
  return boole (r);
}
