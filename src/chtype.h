// -*-C++-*-
#ifndef _chtype_h_
#define _chtype_h_

#include "cdecl.h"

#define CC_BEL 7
#define CC_BS 8
#define CC_HT 9
#define CC_TAB CC_HT
#define CC_NL 10
#define CC_LFD CC_NL
#define CC_VT 11
#define CC_FF 12
#define CC_CR 13
#define CC_RET CC_CR
#define CC_SO 14
#define CC_SI 15
#define CC_ESC 27
#define CC_SPC 32
#define CC_DEL 127
#define CC_SS2 142
#define CC_SS3 143

#define _CTN 1
#define _CTU 2
#define _CTL 4
#define _CTK 8
#define _CTK1 0x10
#define _CTK2 0x20

#define UTF7_SET_D 1
#define UTF7_SET_O 2
#define UTF7_SET_B 4
#define UTF7_WHITE 8
#define UTF7_IMAP4_MAILBOX_NAME 16
#define UTF7_SHIFT_CHAR 32
#define UTF7_IMAP4_SHIFT_CHAR 64

#ifndef NOT_COMPILE_TIME

inline int
_char_type (int c)
{
  extern u_char char_type_table[];
  return (char_type_table + 1) [c];
}

inline int digit_char_p (int c) {return _char_type (c) & _CTN;}
inline int upper_char_p (int c) {return _char_type (c) & _CTU;}
inline int lower_char_p (int c) {return _char_type (c) & _CTL;}
inline int alpha_char_p (int c) {return _char_type (c) & (_CTL | _CTU);}
inline int alphanumericp (int c) {return _char_type (c) & (_CTL | _CTU | _CTN);}
inline int kana_char_p (int c) {return _char_type (c) & _CTK;}
inline int kanji_char_p (int c) {return _char_type (c) & _CTK1;}
inline int kanji2_char_p (int c) {return _char_type (c) & _CTK2;}
inline int SJISP (int c) {return kanji_char_p (c);}
inline int SJIS2P (int c) {return kanji2_char_p (c);}

inline int ascii_char_p (int c) {return u_int (c) < 128;}

inline int SBCP (Char c) {return c < 256;}
inline int DBCP (Char c) {return c >= 256;}
inline int digit_char_p (Char c)
  {return ascii_char_p (c) && digit_char_p (int (c));}
inline int upper_char_p (Char c)
  {return ascii_char_p (c) && upper_char_p (int (c));}
inline int lower_char_p (Char c)
  {return ascii_char_p (c) && lower_char_p (int (c));}
inline int alpha_char_p (Char c)
  {return ascii_char_p (c) && alpha_char_p (int (c));}
inline int alphanumericp (Char c)
  {return ascii_char_p (c) && alphanumericp (int (c));}
inline int kana_char_p (Char c)
  {return SBCP (c) && kana_char_p (int (c));}
inline int kanji_char_p (Char c)
  {return DBCP (c);}

inline int SBCP (lChar c)
  {return c < 256;}
inline int DBCP (lChar c)
  {return c >= 256 && c < CHAR_LIMIT;}
inline int digit_char_p (lChar c)
  {return ascii_char_p (c) && digit_char_p (int (c));}
inline int upper_char_p (lChar c)
  {return ascii_char_p (c) && upper_char_p (int (c));}
inline int lower_char_p (lChar c)
  {return ascii_char_p (c) && lower_char_p (int (c));}
inline int alpha_char_p (lChar c)
  {return ascii_char_p (c) && alpha_char_p (int (c));}
inline int alphanumericp (lChar c)
  {return ascii_char_p (c) && alphanumericp (int (c));}
inline int kana_char_p (lChar c)
  {return SBCP (c) && kana_char_p (int (c));}
inline int kanji_char_p (lChar c)
  {return DBCP (c);}

inline int
_char_downcase (int c)
{
  extern u_char char_translate_downcase_table[];
  return char_translate_downcase_table[c];
}

inline int
_char_upcase (int c)
{
  extern u_char char_translate_upcase_table[];
  return char_translate_upcase_table[c];
}

inline int
_char_transpose_case (int c)
{
  return c ^ 0x20;
}

inline int char_downcase (int c)
  {return ascii_char_p (c) ? _char_downcase (c) : c;}
inline int char_upcase (int c)
  {return ascii_char_p (c) ? _char_upcase (c) : c;}
inline int char_transpose_case (int c)
  {return alpha_char_p (c) ? _char_transpose_case (c) : c;}
inline Char char_downcase (Char c)
  {return ascii_char_p (c) ? Char (_char_downcase (c)) : c;}
inline Char char_upcase (Char c)
  {return ascii_char_p (c) ? Char (_char_upcase (c)) : c;}
inline Char char_transpose_case (Char c)
  {return alpha_char_p (c) ? (Char)_char_transpose_case (c) : c;}

inline int
_digit_char (int c)
{
  extern char char_numeric_table[];
  return char_numeric_table[c];
}

inline int
digit_char (int c)
{
  return ascii_char_p (c) ? _digit_char (c) : 36;
}

inline int
_digit_char_p (int c, int base)
{
  int n = _digit_char (c);
  return n < base ? n : -1;
}

inline int
digit_char_p (int c, int base)
{
  return ascii_char_p (c) ? _digit_char_p (c, base) : -1;
}

extern char upcase_digit_char[];
extern char downcase_digit_char[];

inline int
base64_decode (int c)
{
  extern u_char base64_decode_table[];
  return c < 128 ? base64_decode_table[c] : 65;
}

inline int
imap4_base64_decode (int c)
{
  extern u_char imap4_base64_decode_table[];
  return c < 128 ? imap4_base64_decode_table[c] : 65;
}

inline u_char
utf7_set (int c)
{
  extern u_char utf7_set_table[];
  return utf7_set_table[c];
}

inline int
hqx_decode (int c)
{
  extern u_char hqx_decode_table[];
  return c < 128 ? hqx_decode_table[c] : 64;
}

extern u_char pseudo_char2ctl_table[];
extern u_char pseudo_ctl2char_table[];

#endif /* not NOT_COMPILE_TIME */

#endif
