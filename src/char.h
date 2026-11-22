// -*-C++-*-
#ifndef _char_h_
# define _char_h_

# include "chtype.h"
# include "keycode.h"
# include "charset.h"

// A Lisp character is either a text character (a Char: it can be in a string
// or a buffer) or a key (a keycode_t: meta + ASCII, function keys, mouse).
// The value of a key is >= KEY_BASE (keycode.h), so the two never collide.
// A key that has no modifier is a text character.

// A text character.
inline lisp
make_char (Char c)
{
  return make_immediate (Lchar, c);
}

// A key.  A key that is a text character gives the same value as make_char.
inline lisp
make_key_char (keycode_t k)
{
  return make_immediate (Lchar, keycode_value (k));
}

// A character from a value (a text character or a key).  The marks of the key
// queue (LCHAR_MOUSE, LCHAR_MENU) are dropped.  The result of readc_stream
// (the keyboard) is made a character with this.
inline lisp
make_char_value (lChar c)
{
  return make_immediate (Lchar, u_int32_t (c) & ~LCHAR_MARK_MASK);
}

inline int
charp (lisp x)
{
  return lowbits (pointer_t (x)) == Lchar;
}

// The value of a character (a text character or a key).
inline u_int32_t
xchar_value (lisp x)
{
  assert (charp (x));
  return ximmediate_data (x);
}

// A key (a text character is the key of the character).
inline keycode_t
xkeycode (lisp x)
{
  return make_keycode (xchar_value (x));
}

// Is the character a key (meta, function, mouse...)?
inline int
key_char_p (lisp x)
{
  return !keycode_text_p (xkeycode (x));
}

// A text character only.  A key cannot be in a string or a buffer.
inline Char
xchar_code (lisp x)
{
  u_int32_t v = xchar_value (x);
  if (v >= KEY_BASE)
    FEtype_error (x, Vtext_character);
  return Char (v);
}

inline void
check_char (lisp x)
{
  if (!charp (x))
    FEtype_error (x, Qcharacter);
}

inline u_int32_t
char_value_upcase (u_int32_t v)
{
  return v < 128 ? u_int32_t (char_upcase (int (v))) : v;
}

// code-char: the character with the code, or nil if there is none.  A text
// character is below CHAR_LIMIT.  A key is KEY_BASE + the 16bit layout and has
// to be a key that exists.
inline lisp
code_char (int64_t n)
{
  if (n < 0)
    return Qnil;
  if (n < CHAR_LIMIT)
    return make_char (Char (n));
  if (n < int64_t (KEY_LIMIT) && keycode_valid_p (u_int32_t (n)))
    return make_key_char (make_keycode (u_int32_t (n)));
  return Qnil;
}

#endif
