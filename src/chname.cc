#include "stdafx.h"
#include "chtype.h"
#include "keycode.h"

struct print_char_name
{
  const char *name;
  int l;
  u_int32_t code;
};

#define X(A, B) {A, sizeof A - 1, B}

static const print_char_name standard_char_names[] =
{
  X ("TAB", CC_TAB),
  X ("SPC", CC_SPC),
  X ("LFD", CC_NL),
  X ("RET", CC_CR),
  X ("ESC", CC_ESC),
  X ("DEL", CC_DEL),
  X ("NUL", 0),

  // Common Lisp compat char name
  X ("Backspace", CC_BS),
  X ("Newline", CC_NL),
  X ("Linefeed", CC_NL),
  X ("Page", CC_FF),
  X ("Return", CC_CR),
  X ("Space", CC_SPC),
  X ("Rubout", CC_DEL),
};

static const print_char_name function_char_names[] =
{
  X ("PageUp", keycode_value (KEY_PRIOR)),
  X ("PageDown", keycode_value (KEY_NEXT)),
  X ("End", keycode_value (KEY_END)),
  X ("Home", keycode_value (KEY_HOME)),
  X ("Left", keycode_value (KEY_LEFT)),
  X ("Up", keycode_value (KEY_UP)),
  X ("Right", keycode_value (KEY_RIGHT)),
  X ("Down", keycode_value (KEY_DOWN)),
  X ("Pause", keycode_value (KEY_PAUSE)),
  X ("MouseMove", keycode_value (KEY_MOUSEMOVE)),
  X ("Scroll", keycode_value (KEY_SCROLL)),
  X ("Apps", keycode_value (KEY_APPS)),
  X ("Insert", keycode_value (KEY_INSERT)),
  X ("Delete", keycode_value (KEY_DELETE)),
  X ("Help", keycode_value (KEY_HELP)),
  X ("F24", keycode_value (KEY_F24)),
  X ("F23", keycode_value (KEY_F23)),
  X ("F22", keycode_value (KEY_F22)),
  X ("F21", keycode_value (KEY_F21)),
  X ("F20", keycode_value (KEY_F20)),
  X ("F19", keycode_value (KEY_F19)),
  X ("F18", keycode_value (KEY_F18)),
  X ("F17", keycode_value (KEY_F17)),
  X ("F16", keycode_value (KEY_F16)),
  X ("F15", keycode_value (KEY_F15)),
  X ("F14", keycode_value (KEY_F14)),
  X ("F13", keycode_value (KEY_F13)),
  X ("F12", keycode_value (KEY_F12)),
  X ("F11", keycode_value (KEY_F11)),
  X ("F10", keycode_value (KEY_F10)),
  X ("F9", keycode_value (KEY_F9)),
  X ("F8", keycode_value (KEY_F8)),
  X ("F7", keycode_value (KEY_F7)),
  X ("F6", keycode_value (KEY_F6)),
  X ("F5", keycode_value (KEY_F5)),
  X ("F4", keycode_value (KEY_F4)),
  X ("F3", keycode_value (KEY_F3)),
  X ("F2", keycode_value (KEY_F2)),
  X ("F1", keycode_value (KEY_F1)),
  X ("LBtnDown", keycode_value (KEY_LBTNDOWN)),
  X ("LBtnUp", keycode_value (KEY_LBTNUP)),
  X ("LBtnMove", keycode_value (KEY_LBTNMOVE)),
  X ("RBtnDown", keycode_value (KEY_RBTNDOWN)),
  X ("RBtnUp", keycode_value (KEY_RBTNUP)),
  X ("RBtnMove", keycode_value (KEY_RBTNMOVE)),
  X ("MBtnDown", keycode_value (KEY_MBTNDOWN)),
  X ("MBtnUp", keycode_value (KEY_MBTNUP)),
  X ("MBtnMove", keycode_value (KEY_MBTNMOVE)),
  X ("XBtn1Down", keycode_value (KEY_XBTN1DOWN)),
  X ("XBtn1Up", keycode_value (KEY_XBTN1UP)),
  X ("XBtn1Move", keycode_value (KEY_XBTN1MOVE)),
  X ("XBtn2Down", keycode_value (KEY_XBTN2DOWN)),
  X ("XBtn2Up", keycode_value (KEY_XBTN2UP)),
  X ("XBtn2Move", keycode_value (KEY_XBTN2MOVE)),
};

static const print_char_name char_bit_names[] =
{
  X ("C-", CCF_CTRL_BIT),
  X ("M-", CCF_META),
  X ("S-", CCF_SHIFT_BIT),
};

static inline int
seql (const Char *p1, const u_char *p2)
{
  for (; *p2; p1++, p2++)
    if (char_upcase (*p1) != _char_upcase (*p2))
      return 0;
  return 1;
}

static inline int
sequal (const Char *p1, const u_char *p2)
{
  for (; *p2; p1++, p2++)
    if (*p1 != *p2)
      return 0;
  return 1;
}

Char
standard_char_name2Char (const Char *name, int l)
{
  for (const print_char_name *p = standard_char_names,
       *pe = p + numberof (standard_char_names);
       p < pe; p++)
    if (l == p->l && seql (name, (const u_char *)p->name))
      return Char (p->code);
  return CHAR_NONE;
}

// The key of a name of a function key ("F1", "PageUp").  False if it is not a name.
int
function_key_name2key (const Char *name, int l, keycode_t &k)
{
  for (const print_char_name *p = function_char_names,
       *pe = p + numberof (function_char_names);
       p < pe; p++)
    if (l == p->l && sequal (name, (const u_char *)p->name))
      {
        k = make_keycode (p->code);
        return 1;
      }
  return 0;
}

Char
char_bit_name2Char (const Char *name, int l, int &xl)
{
  for (const print_char_name *p = char_bit_names,
       *pe = p + numberof (char_bit_names);
       p < pe; p++)
    if (l >= p->l && seql (name, (const u_char *)p->name))
      {
        xl = p->l;
        return Char (p->code);
      }
  return 0;
}

const char *
function_key2name (keycode_t c)
{
  for (const print_char_name *p = function_char_names,
       *pe = p + numberof (function_char_names);
       p < pe; p++)
    if (p->code == keycode_value (c))
      return p->name;
  return 0;
}

const char *
standard_Char2name (Char c)
{
  for (const print_char_name *p = standard_char_names,
       *pe = p + numberof (standard_char_names);
       p < pe; p++)
    if (p->code == c)
      return p->name;
  return 0;
}
