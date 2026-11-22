// -*-C++-*-
#ifndef _keycode_h_
#define _keycode_h_

// Key codes (S4-k).
//
// A key is a Lisp character, but it never lives in a string or a buffer.  Its
// value is KEY_BASE + the old 16bit layout, so it can never collide with a
// text character (Unicode, raw bytes).  In C++ a key is a keycode_t, which does
// not convert to or from Char implicitly.  Only this file knows the layout.
//
//   KEY_BASE | 0x8000 | ASCII     meta + ASCII ("M-x")
//   KEY_BASE | 0xff00 | n         function key, mouse button, pseudo control
//                                 (n: 6 bits, 0x40 = Shift, 0x80 = Ctrl)
//   KEY_BASE | 0xfe00 | n         meta + the above
//
// A key that has no modifier is just the text character ("a", C-x, ...), so a
// keycode_t is either a text character (value < KEY_BASE) or one of the above.

#include "cdecl.h"

enum class keycode_t : u_int32_t {};

const u_int32_t KEY_BASE = 0x01000000;
const u_int32_t KEY_LIMIT = KEY_BASE + 0x10000;

// Marks in the key queue (kbd_queue).  They are not part of a key and never
// reach Lisp.
const u_int32_t LCHAR_MOUSE = 0x10000000;
const u_int32_t LCHAR_MENU = 0x20000000;
const u_int32_t LCHAR_MARK_MASK = 0xf0000000;

// The marks sit above every value that a character can have: Unicode
// (0x10FFFF), raw bytes (0x1100FF) and keys.  A text character (also one beyond
// 16bit, after W) is therefore never taken for a mouse or menu event.
static_assert (KEY_LIMIT <= LCHAR_MOUSE && 0x1100ff < LCHAR_MOUSE,
               "the marks of the key queue overlap the characters");
static_assert ((LCHAR_MOUSE & LCHAR_MENU) == 0
               && ((KEY_LIMIT - 1) & LCHAR_MARK_MASK) == 0,
               "the marks of the key queue overlap each other");

// The layout of the low 16 bits.
#define CC_META_BIT 0x8000
#define CCF_CHAR_MASK 0x003f
#define CCF_SHIFT_BIT 0x0040
#define CCF_CTRL_BIT 0x0080
#define CCF_FUNCTION_MASK 0xff00
#define CCF_META 0xfe00

constexpr u_int32_t
keycode_value (keycode_t k)
{
  return static_cast <u_int32_t> (k);
}

constexpr keycode_t
make_keycode (u_int32_t v)
{
  return static_cast <keycode_t> (v);
}

// See WINUSER.H
constexpr keycode_t KEY_PRIOR = keycode_t (KEY_BASE | 0xff00);
constexpr keycode_t KEY_NEXT = keycode_t (KEY_BASE | 0xff01);
constexpr keycode_t KEY_END = keycode_t (KEY_BASE | 0xff02);
constexpr keycode_t KEY_HOME = keycode_t (KEY_BASE | 0xff03);
constexpr keycode_t KEY_LEFT = keycode_t (KEY_BASE | 0xff04);
constexpr keycode_t KEY_UP = keycode_t (KEY_BASE | 0xff05);
constexpr keycode_t KEY_RIGHT = keycode_t (KEY_BASE | 0xff06);
constexpr keycode_t KEY_DOWN = keycode_t (KEY_BASE | 0xff07);
constexpr keycode_t KEY_SCROLL = keycode_t (KEY_BASE | 0xff08);
constexpr keycode_t KEY_MOUSEMOVE = keycode_t (KEY_BASE | 0xff09);
constexpr keycode_t KEY_PAUSE = keycode_t (KEY_BASE | 0xff0a);
constexpr keycode_t KEY_APPS = keycode_t (KEY_BASE | 0xff0b);
constexpr keycode_t KEY_INSERT = keycode_t (KEY_BASE | 0xff0c);
constexpr keycode_t KEY_DELETE = keycode_t (KEY_BASE | 0xff0d);
constexpr keycode_t KEY_HELP = keycode_t (KEY_BASE | 0xff0e);
constexpr keycode_t KEY_F1 = keycode_t (KEY_BASE | 0xff0f);
constexpr keycode_t KEY_F2 = keycode_t (KEY_BASE | 0xff10);
constexpr keycode_t KEY_F3 = keycode_t (KEY_BASE | 0xff11);
constexpr keycode_t KEY_F4 = keycode_t (KEY_BASE | 0xff12);
constexpr keycode_t KEY_F5 = keycode_t (KEY_BASE | 0xff13);
constexpr keycode_t KEY_F6 = keycode_t (KEY_BASE | 0xff14);
constexpr keycode_t KEY_F7 = keycode_t (KEY_BASE | 0xff15);
constexpr keycode_t KEY_F8 = keycode_t (KEY_BASE | 0xff16);
constexpr keycode_t KEY_F9 = keycode_t (KEY_BASE | 0xff17);
constexpr keycode_t KEY_F10 = keycode_t (KEY_BASE | 0xff18);
constexpr keycode_t KEY_F11 = keycode_t (KEY_BASE | 0xff19);
constexpr keycode_t KEY_F12 = keycode_t (KEY_BASE | 0xff1a);
constexpr keycode_t KEY_F13 = keycode_t (KEY_BASE | 0xff1b);
constexpr keycode_t KEY_F14 = keycode_t (KEY_BASE | 0xff1c);
constexpr keycode_t KEY_F15 = keycode_t (KEY_BASE | 0xff1d);
constexpr keycode_t KEY_F16 = keycode_t (KEY_BASE | 0xff1e);
constexpr keycode_t KEY_F17 = keycode_t (KEY_BASE | 0xff1f);
constexpr keycode_t KEY_F18 = keycode_t (KEY_BASE | 0xff20);
constexpr keycode_t KEY_F19 = keycode_t (KEY_BASE | 0xff21);
constexpr keycode_t KEY_F20 = keycode_t (KEY_BASE | 0xff22);
constexpr keycode_t KEY_F21 = keycode_t (KEY_BASE | 0xff23);
constexpr keycode_t KEY_F22 = keycode_t (KEY_BASE | 0xff24);
constexpr keycode_t KEY_F23 = keycode_t (KEY_BASE | 0xff25);
constexpr keycode_t KEY_F24 = keycode_t (KEY_BASE | 0xff26);
constexpr keycode_t KEY_LBTNDOWN = keycode_t (KEY_BASE | 0xff27);
constexpr keycode_t KEY_LBTNMOVE = keycode_t (KEY_BASE | 0xff28);
constexpr keycode_t KEY_LBTNUP = keycode_t (KEY_BASE | 0xff29);
constexpr keycode_t KEY_RBTNDOWN = keycode_t (KEY_BASE | 0xff2a);
constexpr keycode_t KEY_RBTNMOVE = keycode_t (KEY_BASE | 0xff2b);
constexpr keycode_t KEY_RBTNUP = keycode_t (KEY_BASE | 0xff2c);
constexpr keycode_t KEY_MBTNDOWN = keycode_t (KEY_BASE | 0xff2d);
constexpr keycode_t KEY_MBTNMOVE = keycode_t (KEY_BASE | 0xff2e);
constexpr keycode_t KEY_MBTNUP = keycode_t (KEY_BASE | 0xff2f);
// Pseudo control characters ("C-!" and so on).  They take the number of the
// Ctrl/Shift bit as a part of their number.
constexpr keycode_t KEY_EXCLAM = keycode_t (KEY_BASE | 0xff30);
constexpr keycode_t KEY_DQUOTE = keycode_t (KEY_BASE | 0xff31);
constexpr keycode_t KEY_NUMBER = keycode_t (KEY_BASE | 0xff32);
constexpr keycode_t KEY_DOLLAR = keycode_t (KEY_BASE | 0xff33);
constexpr keycode_t KEY_PERCENT = keycode_t (KEY_BASE | 0xff34);
constexpr keycode_t KEY_AMPER = keycode_t (KEY_BASE | 0xff35);
constexpr keycode_t KEY_QUOTE = keycode_t (KEY_BASE | 0xff36);
constexpr keycode_t KEY_LPAREN = keycode_t (KEY_BASE | 0xff37);
constexpr keycode_t KEY_RPAREN = keycode_t (KEY_BASE | 0xff38);
constexpr keycode_t KEY_XBTN1DOWN = keycode_t (KEY_BASE | 0xff39);
constexpr keycode_t KEY_XBTN1UP = keycode_t (KEY_BASE | 0xff3a);
constexpr keycode_t KEY_XBTN1MOVE = keycode_t (KEY_BASE | 0xff3b);
constexpr keycode_t KEY_XBTN2DOWN = keycode_t (KEY_BASE | 0xff3c);
constexpr keycode_t KEY_XBTN2UP = keycode_t (KEY_BASE | 0xff3d);
constexpr keycode_t KEY_XBTN2MOVE = keycode_t (KEY_BASE | 0xff3e);
constexpr keycode_t KEY_ASTER = keycode_t (KEY_BASE | 0xff70);
constexpr keycode_t KEY_PLUS = keycode_t (KEY_BASE | 0xff71);
constexpr keycode_t KEY_COMMA = keycode_t (KEY_BASE | 0xff72);
constexpr keycode_t KEY_MINUS = keycode_t (KEY_BASE | 0xff73);
constexpr keycode_t KEY_DOT = keycode_t (KEY_BASE | 0xff74);
constexpr keycode_t KEY_SLASH = keycode_t (KEY_BASE | 0xff75);
constexpr keycode_t KEY_0 = keycode_t (KEY_BASE | 0xff76);
constexpr keycode_t KEY_1 = keycode_t (KEY_BASE | 0xff77);
constexpr keycode_t KEY_2 = keycode_t (KEY_BASE | 0xff78);
constexpr keycode_t KEY_3 = keycode_t (KEY_BASE | 0xffb0);
constexpr keycode_t KEY_4 = keycode_t (KEY_BASE | 0xffb1);
constexpr keycode_t KEY_5 = keycode_t (KEY_BASE | 0xffb2);
constexpr keycode_t KEY_6 = keycode_t (KEY_BASE | 0xffb3);
constexpr keycode_t KEY_7 = keycode_t (KEY_BASE | 0xffb4);
constexpr keycode_t KEY_8 = keycode_t (KEY_BASE | 0xffb5);
constexpr keycode_t KEY_9 = keycode_t (KEY_BASE | 0xffb6);
constexpr keycode_t KEY_COLON = keycode_t (KEY_BASE | 0xffb7);
constexpr keycode_t KEY_SEMI = keycode_t (KEY_BASE | 0xffb8);
constexpr keycode_t KEY_LT = keycode_t (KEY_BASE | 0xfff0);
constexpr keycode_t KEY_EQ = keycode_t (KEY_BASE | 0xfff1);
constexpr keycode_t KEY_GT = keycode_t (KEY_BASE | 0xfff2);
//constexpr keycode_t KEY_QUESTION  --> DEL
constexpr keycode_t KEY_BACKQ = keycode_t (KEY_BASE | 0xfff3);
constexpr keycode_t KEY_LBRACE = keycode_t (KEY_BASE | 0xfff4);
constexpr keycode_t KEY_VER = keycode_t (KEY_BASE | 0xfff5);
constexpr keycode_t KEY_RBRACE = keycode_t (KEY_BASE | 0xfff6);
constexpr keycode_t KEY_TILDE = keycode_t (KEY_BASE | 0xfff7);
constexpr keycode_t KEY_EMPTY_CHAR = keycode_t (KEY_BASE | 0xfff8); // XXX

constexpr keycode_t KEY_CHAR_MIN = KEY_PRIOR;
constexpr keycode_t KEY_CHAR_MAX = KEY_XBTN2MOVE;
constexpr keycode_t KEY_Fn_MAX = KEY_F24;
constexpr keycode_t KEY_CTLCHAR_MIN = KEY_EXCLAM;
constexpr keycode_t KEY_CTLCHAR_MAX = KEY_RPAREN;

#define NFUNCTION_KEYS \
  (int (keycode_value (KEY_CHAR_MAX) - keycode_value (KEY_CHAR_MIN) + 1))

inline u_int
keycode_low (keycode_t k)
{
  return keycode_value (k) & 0xffff;
}

// A text character as a key.
inline keycode_t
keycode_from_char (Char c)
{
  return make_keycode (c);
}

// Is it a plain character (not a meta or function key)?
inline bool
keycode_text_p (keycode_t k)
{
  return keycode_value (k) < KEY_BASE;
}

inline Char
keycode_to_char (keycode_t k)
{
  return Char (keycode_value (k));
}

// A character that was read from a stream, as text.  The keyboard stream gives
// keys (KEY_BASE and above, and the marks of the key queue), and a key is not
// text.  Keep the low 16 bits, which is what a 16bit Char did to a key, so that
// the reader and the echo stream never get a value that no table can take.
// Every place that makes a text Char (a string, a buffer, a token) from an
// lChar that may come from the keyboard stream goes through this function.
inline Char
text_char (lChar c)
{
  return Char (c >= KEY_BASE ? c & 0xffff : c);
}

// The key part (KEY_BASE..KEY_LIMIT), whatever its low bits are.
inline bool
keycode_in_key_range_p (keycode_t k)
{
  return keycode_value (k) >= KEY_BASE && keycode_value (k) < KEY_LIMIT;
}

// Meta + ASCII.
inline bool
keycode_meta_ascii_p (keycode_t k)
{
  return (keycode_value (k) & ~0x7fu) == (KEY_BASE | CC_META_BIT);
}

inline keycode_t
keycode_meta_ascii (u_int32_t c)
{
  return make_keycode (KEY_BASE | CC_META_BIT | (c & 0x7f));
}

inline keycode_t
keycode_meta_ascii_to_char (keycode_t k)
{
  return make_keycode (keycode_value (k) & 0x7f);
}

inline bool
keycode_function_p (keycode_t k)
{
  if (!keycode_in_key_range_p (k))
    return false;
  u_int c = keycode_low (k);
  if (c == keycode_low (KEY_EMPTY_CHAR))
    return false;
  c &= ~(CCF_CTRL_BIT | CCF_SHIFT_BIT);
  return (c >= keycode_low (KEY_CHAR_MIN) && c <= keycode_low (KEY_CHAR_MAX));
}

inline keycode_t
keycode_function_to_meta_function (keycode_t k)
{
  return make_keycode ((keycode_value (k) & ~u_int32_t (CCF_FUNCTION_MASK))
                       | CCF_META);
}

inline keycode_t
keycode_meta_function_to_function (keycode_t k)
{
  return make_keycode (keycode_value (k) | CCF_FUNCTION_MASK);
}

inline bool
keycode_meta_function_p (keycode_t k)
{
  return (keycode_in_key_range_p (k)
          && (keycode_low (k) & CCF_FUNCTION_MASK) == CCF_META
          && keycode_function_p (keycode_meta_function_to_function (k)));
}

inline bool
keycode_pseudo_ctl_p (keycode_t k)
{
  if (!keycode_in_key_range_p (k))
    return false;
  u_int c = keycode_low (k) & ~(CCF_CTRL_BIT | CCF_SHIFT_BIT);
  return (c >= keycode_low (KEY_CTLCHAR_MIN) && c <= keycode_low (KEY_CTLCHAR_MAX));
}

inline bool
keycode_meta_pseudo_ctl_p (keycode_t k)
{
  return (keycode_in_key_range_p (k)
          && (keycode_low (k) & CCF_FUNCTION_MASK) == CCF_META
          && keycode_pseudo_ctl_p (keycode_meta_function_to_function (k)));
}

// Is the value a key that exists (for code-char)?
inline bool
keycode_valid_p (u_int32_t v)
{
  keycode_t k = make_keycode (v);
  return (keycode_meta_ascii_p (k)
          || keycode_function_p (k)
          || keycode_meta_function_p (k));
}

inline keycode_t
keycode_add_bits (keycode_t k, u_int bits)
{
  return make_keycode (keycode_value (k) | bits);
}

inline keycode_t
keycode_offset (keycode_t k, int n)
{
  return make_keycode (keycode_value (k) + n);
}

// set-meta-bit.  Keys that cannot have a meta bit are returned as they are.
inline keycode_t
keycode_set_meta (keycode_t k, bool meta)
{
  if (meta)
    {
      if (keycode_value (k) < 128)
        return keycode_meta_ascii (keycode_value (k));
      if (keycode_function_p (k))
        return keycode_function_to_meta_function (k);
    }
  else
    {
      if (keycode_meta_ascii_p (k))
        return keycode_meta_ascii_to_char (k);
      if (keycode_meta_function_p (k))
        return keycode_meta_function_to_function (k);
    }
  return k;
}

// Index in a dense keymap: ASCII as it is, a function key 128 + (low 6 bits),
// plus a step for Shift and Ctrl.  (u_int) -1 if it has no index.
inline u_int
keycode_keymap_index (keycode_t k)
{
  if (keycode_function_p (k))
    {
      u_int c = keycode_low (k);
      u_int x = 128 + (c & CCF_CHAR_MASK);
      if (c & CCF_SHIFT_BIT)
        x += NFUNCTION_KEYS;
      if (c & CCF_CTRL_BIT)
        x += 2 * NFUNCTION_KEYS;
      return x;
    }
  return keycode_value (k) < 128 ? keycode_value (k) : u_int (-1);
}

// The inverse of keycode_keymap_index.  Returns false if the index is out of
// range.
inline bool
keycode_from_keymap_index (u_int i, keycode_t &k)
{
  if (i < 128)
    {
      k = make_keycode (i);
      return true;
    }
  i -= 128;
  u_int n = i % NFUNCTION_KEYS;
  u_int step = i / NFUNCTION_KEYS;
  if (step >= 4)
    return false;
  u_int low = keycode_low (KEY_CHAR_MIN) + n;
  if (step & 1)
    low |= CCF_SHIFT_BIT;
  if (step & 2)
    low |= CCF_CTRL_BIT;
  k = make_keycode (KEY_BASE | low);
  return true;
}

// Keys in old strings (kbd macros in the history file, written before S4-k)
// are the 16bit values.  0x8000-0x807F and 0xFE00-0xFFFF were never text.
inline keycode_t
keycode_from_legacy (u_int32_t v)
{
  if ((v & ~0x7fu) == CC_META_BIT || (v >= 0xfe00 && v <= 0xffff))
    return make_keycode (KEY_BASE | v);
  return make_keycode (v);
}

// An element of the key queue: the key plus the marks above.
inline bool
lchar_menu_p (u_int32_t c)
{
  return (c & LCHAR_MENU) != 0;
}

inline bool
lchar_mouse_p (u_int32_t c)
{
  return (c & LCHAR_MOUSE) != 0;
}

inline keycode_t
lchar_to_keycode (u_int32_t c)
{
  return make_keycode (c & ~LCHAR_MARK_MASK);
}

#endif
