#ifndef _font_h_
#define _font_h_

#define FONT_SIZE_MIN_PIXEL 8
#define FONT_SIZE_MAX_PIXEL 48

class FontObject
{
protected:
  HFONT fo_hfont;
  int fo_need_pad;
  POINT fo_offset;
  SIZE fo_size;
  int fo_ascent;
  LOGFONT fo_logfont;
public:
  FontObject () : fo_hfont (0) {}
  ~FontObject () {if (fo_hfont) DeleteObject (fo_hfont);}
  int create (const LOGFONT &);
  int create (const char *, int, int);
  operator HFONT () const {return fo_hfont;}
  const HFONT hfont () const {return fo_hfont;}
  int need_pad_p () const {return fo_need_pad;}
  void require_pad () {fo_need_pad = 1;}
  void get_metrics ();
  void get_metrics (HDC, SIZE &, SIZE &);
  void calc_offset (const SIZE &);
  const SIZE &size () const {return fo_size;}
  const POINT &offset () const {return fo_offset;}
  int ascent () const {return fo_ascent;}
  const LOGFONT &logfont () const {return fo_logfont;}
  static const bool update (LOGFONT &lf, const lisp keys, const bool recommend_size_p);
  static const int dpi ()
    {
      HDC hdc = GetDC (0);
      int dpi = GetDeviceCaps (hdc, LOGPIXELSY);
      ReleaseDC (0, hdc);
      return dpi;
    }
  static const int pixel_to_point (int pixel)
    {
      return MulDiv (pixel, 72, dpi ());
    }
  static const int point_to_pixel (int point)
    {
      return MulDiv (point, dpi (), 72);
    }
};

#define FONT_ASCII          0
#define FONT_JP             1
#define FONT_LATIN          2
#define FONT_CYRILLIC       3
#define FONT_GREEK          4
#define FONT_CN_SIMPLIFIED  5
#define FONT_CN_TRADITIONAL 6
#define FONT_HANGUL         7
#define FONT_GEORGIAN       8
#define FONT_MAX            9

struct FontSetParam
{
  LOGFONT fs_logfont[FONT_MAX];
  int fs_use_backsl;
  int fs_line_spacing;
  int fs_recommend_size;
  int fs_size_pixel;
};

class FontSet
{
protected:
  void create_bitmap ();
  void paint_newline_bitmap (HDC);
  void paint_backsl_bitmap (HDC);
  void paint_tab_bitmap (HDC);
  void paint_fullspc_bitmap (HDC);
  void paint_halfspc_bitmap (HDC);
  void paint_sep_bitmap (HDC);
  void paint_blank (HDC);
  void paint_fold_bitmap (HDC);
  void save_params (const FontSetParam &);
  void load_params (FontSetParam &);

  static const UINT fs_lang_id[];
  static const lisp *const fs_lang_key[];
  static const char *const fs_regent[];
  struct fontface {const char *disp, *print; int charset;};
  static const fontface fs_default_face[];
public:
  enum
    {
      backsl,
      newline,
      htab,
      fullspc1,
      fullspc2,
      sep,
      blank,
      wblank1,
      wblank2,
      halfspc,
      bold_backsl,
      fold_sep0,
      fold_sep1,
      fold_mark_sep0,
      fold_mark_sep1,
      max_bitmap
    };

protected:
  FontObject fs_font[FONT_MAX];
  HBITMAP fs_hbm;
  SIZE fs_size;
  SIZE fs_cell;
  int fs_ascent;
  int fs_need_pad;
  int fs_line_spacing;
  int fs_use_backsl;
  int fs_line_width;
  int fs_recommend_size;
  int fs_size_pixel;

public:
  FontSet () : fs_hbm (0) {}
  ~FontSet () {if (fs_hbm) DeleteObject (fs_hbm);}
  int create (const FontSetParam &);
  void init ();
  lisp make_alist () const;
  const bool update (FontSetParam &param, const lisp lfontset) const;
  const FontObject &font (int n) const {return fs_font[n];}
  const HBITMAP &hbm () const {return fs_hbm;}
  const SIZE &size () const {return fs_size;}
  const SIZE &cell () const {return fs_cell;}
  int need_pad_p () const {return fs_need_pad;}
  int use_backsl_p () const {return fs_use_backsl;}
  int line_width () const {return fs_line_width;}
  int line_spacing () const {return fs_line_spacing;}
  int recommend_size_p () const {return fs_recommend_size;}
  int size_pixel_p () const {return fs_size_pixel;}

  static const char *regent (int n) {return fs_regent[n];}
  static const char *default_face (int n, int print)
    {return (!print || !fs_default_face[n].print
             ? fs_default_face[n].disp : fs_default_face[n].print);}
  static int default_charset (int n) {return fs_default_face[n].charset;}
  static UINT lang_id (int n) {return fs_lang_id[n];}
  static const lisp lang_key (int n) {return *fs_lang_key[n];}
  static const int lang_key_index (lisp llang)
    {
      for (int i = 0; i < FONT_MAX; i++)
        {
          if (lang_key (i) == llang)
            return i;
        }
      return -1;
    }
};

int get_font_height (HWND hwnd);
bool font_exist_p (const HDC hdc, const char *face, BYTE charset);

// Not a FontSet slot: the Lucida Sans Unicode face that draws U+02B0..U+036F
// (a pseudo slot after the last real one).  It is created when it is drawn.
#define FONT_LUCIDA FONT_MAX
// The slot for the characters that no other slot is for (design 6.2 FALLBACK).
// The slot of its own (and its ini key) is added by C3; until then, LATIN.
#define FONT_FALLBACK FONT_LATIN

// How a character is shown: the one place that decides the font slot, the
// display width and the UTF-16 units of a character (design 6.2, 10.2 R3).
// The screen (disp.cc), the printer (print.cc) and the status area use it.
struct char_face
{
  u_char font;                  // FONT_* or FONT_LUCIDA
  u_char width;                 // cells: 1 or 2
  u_char nwc;                   // UTF-16 units in wc; 0: nothing to draw
  ucs2_t wc[2];
  ucs2_t subst;                 // what the printer draws when nwc is 0 (0: nothing)
};

// han_font: the slot of the Han characters of the buffer (FONT_JP,
// FONT_HANGUL, FONT_CN_SIMPLIFIED or FONT_CN_TRADITIONAL), see
// encoding_han_font.  It matters only when a Char is a Unicode code point
// (XYZZY_CHAR_UNICODE).
void char_face_of (Char, int han_font, char_face &);
// The display width of a character other than a control character.
int char_display_width (Char);
// The Unicode code point of a character, or CHAR_NONE.
Char char_unicode_of (Char);
// The slot that a buffer with this char-encoding draws its Han characters with.
int encoding_han_font (lisp encoding);
// The slot for the characters that the encoding makes (the IME composition
// string), or -1.
int encoding_font_slot (lisp encoding, int han);

#endif /* _font_h_ */
