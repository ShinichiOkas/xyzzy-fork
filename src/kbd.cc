#include "stdafx.h"
#include "ed.h"
#include "filer.h"
#include "safe_ptr.h"
#include "encoding.h"
#include "environ.h"
#include "codec.h"

kbd_queue::kbd_queue ()
     : head (0), tail (0), pending (lChar_EOF), last_ime_status (-1),
       current_mode (im_normal), kbd_macro (0), delayed_activate (0), in_hook (0),
       last_command_key_index (0), command_key_keeped (0), st_timeout (-1),
       reconv (0), putc_pending (-1), ime_prop (0), unicode_kbd (0), gc_timer (0)
{
  GetKeyboardLayout =
    (GETKEYBOARDLAYOUT)GetProcAddress (GetModuleHandle ("USER32"),
                                       "GetKeyboardLayout");

  for (int i = 0; i < QUEUE_MAX; i++)
    cc[i] = lChar_EOF;
}

kbd_queue::~kbd_queue ()
{
  xfree (reconv);
}

void
kbd_queue::process_events ()
{
  disable_kbd dkbd (*this);
  MSG msg;
  while (PeekMessage (&msg, 0, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
        {
          PostQuitMessage (0);
          break;
        }
      XyzzyTranslateMessage (&msg);
      DispatchMessage (&msg);
    }
}

int
kbd_queue::putraw (lChar c)
{
  if ((tail + 1) % QUEUE_MAX == head)
    {
      DBG_PRINT (("KBD BUFFER OVER FLOW\n"));
      return 0;
    }
  cc[tail] = c;
  tail = (tail + 1) % QUEUE_MAX;
  return 1;
}

int
kbd_queue::putc (lChar c)
{
  if (c >= 256)
    return putraw (c);

  if (putc_pending >= 0)
    {
      char b[2];
      b[0] = char (putc_pending);
      b[1] = char (c);
      putc_pending = -1;
      return puts (b, 2);
    }

  if (c < 128)
    return putraw (c);

  if (kbd_mblead_p (c))
    {
      putc_pending = c;
      return 1;
    }

  char cc = char (c);
  return puts (&cc, 1);
}

int
kbd_queue::putw (int w)
{
  if (w < 256)
    return putc (w);
  char b[2];
  b[0] = w >> 8;
  b[1] = w;
  return puts (b, 2);
}

int
kbd_utf16_decoder::feed (ucs2_t u, Char *out, bool unicode, const Char *tab, bool half_width)
{
  if (!unicode)
    {
      Char cc;
      if ((!tab || (cc = tab[u]) == CHAR_NONE)
          && (cc = half_width ? w2i_half_width (u) : w2i (u)) == CHAR_NONE)
        {
          out[0] = utf16_ucs2_to_undef_pair_high (u);
          out[1] = utf16_ucs2_to_undef_pair_low (u);
          return 2;
        }
      out[0] = cc;
      return 1;
    }

  int n = 0;
  if (ud_high)
    {
      if (utf16_surrogate_low_p (u))
        {
          out[0] = utf16_pair_to_ucs4 (ud_high, u);
          ud_high = 0;
          return 1;
        }
      out[n++] = ud_high;
      ud_high = 0;
    }
  if (utf16_surrogate_high_p (u))
    {
      ud_high = u;
      return n;
    }
  out[n++] = u;
  return n;
}

int
kbd_utf16_decoder::flush (Char *out, bool unicode)
{
  if (!unicode || !ud_high)
    return 0;
  out[0] = ud_high;
  ud_high = 0;
  return 1;
}

int
kbd_queue::put_char (Char c)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    return putraw (c);
  else
    return putc (c);
}

// A UTF-16 code unit from the keyboard or the IME.  An ASCII character takes
// the translation of the keys (kbd-translate-table, Alt = Meta) as a
// character from WM_CHAR does.
int
kbd_queue::put_utf16 (ucs2_t u)
{
  Char out[kbd_utf16_decoder::OUT_MAX];
  int n = utf16_in.feed (u, out, XYZZY_CHAR_UNICODE != 0, 0, true);
  for (int i = 0; i < n; i++)
    {
      Char c = out[i];
      if (XYZZY_CHAR_UNICODE ? c < 128 : n == 1)
        c = decode_chars (c);
      if (!put_char (c))
        return 0;
    }
  return 1;
}

int
kbd_queue::puts (const char *string, int l)
{
  if constexpr (XYZZY_CHAR_UNICODE)
    {
      // The bytes of an ANSI window (WM_CHAR, WM_IME_CHAR, a non-Unicode IME)
      // are in the code page of the system.  Characters are made from the
      // UTF-16 that they mean.
      if (l <= 0)
        return 1;
      std::vector <wchar_t> w (l);
      int wl = MultiByteToWideChar (CP_ACP, MB_ERR_INVALID_CHARS, string, l,
                                    w.data (), l);
      kbd_utf16_decoder dec;
      Char out[kbd_utf16_decoder::OUT_MAX];
      if (wl <= 0)
        {
          // not a character of the code page: keep the bytes as they are
          for (int i = 0; i < l; i++)
            if (!putraw ((u_char (string[i]) < 128 ? 0 : CODEC_RAW_BYTE_BASE)
                         + u_char (string[i])))
              return 0;
          return 1;
        }
      for (int i = 0; i < wl; i++)
        {
          int n = dec.feed (w[i], out, true);
          for (int j = 0; j < n; j++)
            if (!putraw (out[j]))
              return 0;
        }
      int n = dec.flush (out, true);
      for (int j = 0; j < n; j++)
        if (!putraw (out[j]))
          return 0;
      return 1;
    }

  xinput_strstream in (string, l);
  encoding_input_stream_helper is (xsymbol_value (Vkbd_encoding), in);
  int c;
  while ((c = is->get ()) != xstream::eof)
    if (!putraw (c))
      return 0;
  return 1;
}

lChar
kbd_queue::peek (int req_mouse_move)
{
  lChar c;
  if (pending != lChar_EOF)
    {
      c = pending;
      pending = lChar_EOF;
    }
  else
    {
      if (kbd_macro)
        c = macro_char ();
      else
        {
          if (head == tail)
            return lChar_EOF;
          c = cc[head];
          head = (head + 1) % QUEUE_MAX;
          if (!lchar_menu_p (c)
              && !req_mouse_move && char_mouse_move_p (lchar_to_keycode (c)))
            return lChar_EOF;
          if (save_p () && !lchar_menu_p (c)
              && !char_mouse_move_p (lchar_to_keycode (c)))
            {
              if (nsaved == KBDMACRO_MAX)
                stop_macro ();
              else
                saved[nsaved++] = lchar_to_keycode (c);
            }
        }
    }
  return c;
}

void
kbd_queue::clear ()
{
  process_events ();
  head = tail;
  pending = lChar_EOF;
  stop_macro ();
}

lChar
kbd_queue::fetch (int in_main, int req_mouse_move)
{
  lChar c;
  in_main_loop = in_main;
  gc_timer = 0;

  if (delayed_activate && idlep ())
    run_activate_hook ();

  while (1)
    {
      c = peek (req_mouse_move);
      if (c != lChar_EOF)
        break;
      MSG msg;
      if (!GetMessage (&msg, 0, 0, 0))
        {
          in_main_loop = 0;
          PostQuitMessage (0);
          return lChar_EOF;
        }
      XyzzyTranslateMessage (&msg);
      DispatchMessage (&msg);
      in_main_loop = in_main;
      if (in_main && gc_timer)
        {
          gc_timer = 0;
          if (ldataP::ld_nwasted)
            gc (-1);
        }
    }
  in_main_loop = 0;
  if (!(c & (LCHAR_MOUSE | LCHAR_MENU)))
    process_events ();
  xsymbol_value (Vquit_flag) = Qnil;
  return c;
}

kbd_queue::sleep_timer::sleep_timer (kbd_queue &q)
     : s_kbdq (q)
{
  s_save_start_time = s_kbdq.st_start_time;
  s_save_timeout = s_kbdq.st_timeout;
}

kbd_queue::sleep_timer::~sleep_timer ()
{
  KillTimer (app.toplev, TID_SLEEP);
  app.sleep_timer_exhausted = 0;
  s_kbdq.st_start_time = s_save_start_time;
  s_kbdq.st_timeout = s_save_timeout;
  if (s_kbdq.st_timeout >= 0)
    {
      DWORD req = s_kbdq.st_start_time + s_kbdq.st_timeout;
      DWORD curtime = GetTickCount ();
      if (req >= curtime)
        app.sleep_timer_exhausted = 1;
      else
        SetTimer (app.toplev, TID_SLEEP, req - curtime, 0);
    }
}

void
kbd_queue::sleep_timer::wait (int timeout, int kbdp)
{
  disable_kbd dkbd (s_kbdq);
  s_kbdq.st_start_time = GetTickCount ();
  s_kbdq.st_timeout = timeout;
  app.sleep_timer_exhausted = 0;
  SetTimer (app.toplev, TID_SLEEP, timeout, 0);
  while ((!kbdp || s_kbdq.head == s_kbdq.tail) && !app.sleep_timer_exhausted)
    {
      QUIT;
      MSG msg;
      if (!GetMessage (&msg, 0, 0, 0))
        {
          PostQuitMessage (0);
          break;
        }
      XyzzyTranslateMessage (&msg);
      DispatchMessage (&msg);
    }
}

void
kbd_queue::sit_for (DWORD timeout)
{
  if (kbd_macro || pending != lChar_EOF || head != tail)
    return;
  sleep_timer st (*this);
  st.wait (timeout, 1);
}

void
kbd_queue::sleep_for (DWORD timeout)
{
  sleep_timer st (*this);
  st.wait (timeout, 0);
}

int
kbd_queue::listen ()
{
  if (kbd_macro || pending != lChar_EOF || head != tail)
    return 1;
  process_events ();
  xsymbol_value (Vquit_flag) = Qnil;
  return head != tail;
}

int
kbd_queue::wait_event (HANDLE h, int enable_quit)
{
  disable_kbd dkbd (*this);
  while (1)
    {
      if (MsgWaitForMultipleObjects (1, &h, 0, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0)
        return 1;

      MSG msg;
      while (PeekMessage (&msg, 0, 0, 0, PM_REMOVE))
        {
          if (msg.message == WM_QUIT)
            {
              PostQuitMessage (0);
              return 0;
            }
          XyzzyTranslateMessage (&msg);
          DispatchMessage (&msg);
          if (enable_quit && xsymbol_value (Vquit_flag) != Qnil)
            return 0;
        }
    }
}

void
kbd_queue::start_macro ()
{
  assert (!save_p ());
  current_mode = input_mode (disablep () | im_save);
  Window::modify_all_mode_line ();
  nsaved = 0;
  last_command_key_index = 0;
  command_key_keeped = 0;
}

lisp
kbd_queue::end_macro ()
{
  assert (save_p ());
  stop_macro ();
  int n = min (nsaved, last_command_key_index);
  lisp macro = make_vector (n, Qnil);
  for (int i = 0; i < n; i++)
    xvector_contents (macro)[i] = make_key_char (saved[i]);
  return macro;
}

// A kbd macro is a vector of characters.  A string (before S4-k) is also a
// kbd macro; its elements are the 16bit values.
int
kbd_macro_p (lisp x)
{
  if (stringp (x))
    return 1;
  if (!general_vector_p (x) || !xvector_length (x))
    return 0;
  for (int i = 0, n = xvector_length (x); i < n; i++)
    if (!charp (xvector_contents (x)[i]))
      return 0;
  return 1;
}

int
kbd_macro_length (lisp x)
{
  return stringp (x) ? xstring_length (x) : xvector_length (x);
}

void
kbd_queue::stop_macro ()
{
  if (save_p ())
    Window::modify_all_mode_line ();
  current_mode = input_mode (disablep () | im_normal);
}

lChar
kbd_queue::macro_char ()
{
  assert (kbd_macro);
  lisp macro = kbd_macro->string;
  assert (kbd_macro->index < kbd_macro_length (macro));
  lChar c;
  if (stringp (macro))
    c = keycode_value (keycode_from_legacy (xstring_contents (macro) [kbd_macro->index]));
  else
    c = keycode_value (xkeycode (xvector_contents (macro) [kbd_macro->index]));
  kbd_macro->index++;
  if (kbd_macro->index >= kbd_macro_length (macro))
    kbd_macro = kbd_macro->last;
  return c;
}

int
kbd_queue::track_popup_menu (HMENU hmenu, int button, const POINT &p)
{
  int result = 0;
  in_main_loop = 1;
  if (idlep ())
    {
      result = TrackPopupMenu (hmenu, TPM_RETURNCMD | TPM_LEFTALIGN | button,
                               p.x, p.y, 0, app.toplev, 0);
      if (result == -1)
        result = 0;
    }
  in_main_loop = 0;
  return result;
}

void
kbd_queue::close_ime ()
{
  if (pending == lChar_EOF && head == tail
      && !macro_is_running ())
    {
      if (xsymbol_value (Vime_control) == Qnil)
        last_ime_status = -1;
      else if (last_ime_status == -1)
        {
          HIMC imc = gime.ImmGetContext (app.toplev);
          if (imc)
            {
              last_ime_status = gime.ImmGetOpenStatus (imc);
              if (last_ime_status)
                gime.ImmSetOpenStatus (imc, 0);
              gime.ImmReleaseContext (app.toplev, imc);
            }
        }
    }
}

void
kbd_queue::restore_ime ()
{
  if (pending == lChar_EOF && head == tail
      && !macro_is_running ()
      && xsymbol_value (Vime_control) != Qnil
      && last_ime_status != -1)
    {
      HIMC imc = gime.ImmGetContext (app.toplev);
      if (imc)
        {
          gime.ImmSetOpenStatus (imc, last_ime_status);
          gime.ImmReleaseContext (app.toplev, imc);
          last_ime_status = -1;
        }
    }
}

int
kbd_queue::toggle_ime (int new_stat, int update_last_ime_status)
{
  HIMC imc = gime.ImmGetContext (app.toplev);
  if (!imc)
    return 0;
  int old_stat = gime.ImmGetOpenStatus (imc) ? IME_MODE_ON : IME_MODE_OFF;
  if (new_stat == IME_MODE_TOGGLE || new_stat != old_stat)
    gime.ImmSetOpenStatus (imc, old_stat != IME_MODE_ON);
  gime.ImmReleaseContext (app.toplev, imc);
  if (update_last_ime_status)
    last_ime_status = -1;
  return old_stat;
}

void
kbd_queue::run_hook (lisp hook, int f)
{
  if ((delayed_activate & f) == f)
    {
      delayed_activate &= ~f;
      in_hook = 1;
      selected_buffer ()->safe_run_hook (hook, 1);
      in_hook = 0;
    }
}

void
kbd_queue::run_activate_hook ()
{
  if (in_hook)
    return;
  run_hook (Vactivate_hook, ACTIVATE | LAST_ACTIVE);
  run_hook (Vdeactivate_hook, DEACTIVATE | LAST_ACTIVE);
  run_hook (Vactivate_hook, ACTIVATE);
  run_hook (Vdeactivate_hook, DEACTIVATE);
  refresh_screen (0);
}

void
kbd_queue::activate (int active)
{
  if (active)
    delayed_activate |= ACTIVATE | LAST_ACTIVE;
  else
    {
      delayed_activate |= DEACTIVATE;
      delayed_activate &= ~LAST_ACTIVE;
    }
  if (idlep ())
    run_activate_hook ();
}

int
kbd_queue::lookup_kbd_macro (lisp string) const
{
  for (kbd_macro_context *p = kbd_macro; p; p = p->last)
    if (p->string == string)
      return 1;
  return 0;
}

void
check_kbd_enable ()
{
  if (IsWindowEnabled (app.toplev))
    return;
  FEprogram_error (Ekbd_input_is_disabled);
}

#define VK_HANKAKU 0xf3
#define VK_ZENKAKU 0xf4

static int
exkey_index (UINT c, int syskey)
{
  static const UINT exkey[] =
    {
      VK_BACK,
      VK_TAB,
      VK_RETURN,
      VK_ESCAPE,
      VK_ZENKAKU,
      VK_SPACE,
    };
  if (syskey && c == VK_SPACE && xsymbol_value (Venable_meta_key) == Qnil)
    return -1;

  // for DEC LK411-AJ
  if ((c == VK_ZENKAKU || c == VK_HANKAKU)
      && LOWORD (MapVirtualKey (c, 2)))
    return -1;

  if (c == VK_HANKAKU)
    c = VK_ZENKAKU;

  for (int i = 0; i < numberof (exkey); i++)
    if (c == exkey[i])
      return i;
  return -1;
}

static int
to_ascii_char (int vk, int sc)
{
  if (vk < VK_SPACE)
    return -1;
  if (GetKeyState (VK_CONTROL) >= 0)
    return -1;
  static BYTE state[256];
  state[VK_SHIFT] = GetKeyState (VK_SHIFT) < 0 ? 0x80 : 0;
  state[VK_CONTROL] = 0;
  WORD key[2] = {0};
  if (ToAscii (vk, sc, state, key, 0) != 1)
    return -1;
  if (*key >= 128)
    return -1;
  return *key;
}

static int
maybe_ctrl_char_p (int c)
{
  return c == 0x3f || c == 0x40 || (c >= 0x5b && c <= 0x5f);
}

static int
translate_unicode (HWND hwnd, WPARAM wparam, LPARAM lparam)
{
  BYTE b[256];
  GetKeyboardState (b);
  wchar_t w[2];
  switch (ToUnicode (wparam, lparam, b, w, 2, 0))
    {
    default:
      return 0;

    case 1:
    case -1:
      PostMessage (hwnd, WM_PRIVATE_WCHAR, w[0], lparam);
      return 1;

    case 2:
      PostMessage (hwnd, WM_PRIVATE_WCHAR, w[0], lparam);
      PostMessage (hwnd, WM_PRIVATE_WCHAR, w[1], lparam);
      return 1;
    }
}

BOOL
XyzzyTranslateMessage (const MSG *msg)
{
  if (msg->hwnd == app.toplev || Filer::filer_ancestor_p (msg->hwnd))
    {
      switch (msg->message)
        {
        case WM_SYSKEYDOWN:
          if (msg->wParam >= VK_NUMPAD0 && msg->wParam <= VK_NUMPAD9)
            {
              PostMessage (msg->hwnd, WM_SYSCHAR,
                           msg->wParam - VK_NUMPAD0 + '0', msg->lParam);
              return 1;
            }
          /* fall thru... */
        case WM_KEYDOWN:
          if (msg->wParam == VK_PROCESSKEY
              && xsymbol_value (Vime_does_not_process_control_backslach) != Qnil)
            {
              int key = to_ascii_char (app.kbdq.gime.ImmGetVirtualKey (msg->hwnd), msg->lParam);
              if (key == '\\' || key == '_')
                {
                  PostMessage (msg->hwnd, msg->message == WM_KEYDOWN ? WM_CHAR : WM_SYSCHAR,
                               key - '@', msg->lParam);
                  return 1;
                }
            }
          else
            {
              if (app.kbdq.unicode_kbd_p () && msg->message == WM_KEYDOWN
                  && exkey_index (msg->wParam, 0) < 0
                  && translate_unicode (msg->hwnd, msg->wParam, msg->lParam))
                return 1;

              int key = to_ascii_char (msg->wParam, msg->lParam);
              if (maybe_ctrl_char_p (key))
                {
                  PostMessage (msg->hwnd, msg->message == WM_KEYDOWN ? WM_CHAR : WM_SYSCHAR,
                               key >= 0x40 ? key - 0x40 : key + 0x40, msg->lParam);
                  return 1;
                }
              else if (key != -1 && !alpha_char_p (key))
                return 0;
            }
          /* fall thru... */
        case WM_KEYUP:
        case WM_SYSKEYUP:
          if (exkey_index (msg->wParam, (msg->message == WM_SYSKEYDOWN
                                         || msg->message == WM_SYSKEYUP)) >= 0)
            return 0;
          break;
        }
    }
  return app.kbdq.gime.TranslateMessage (msg);
}

static inline int
lkey_function_p (lChar c)
{
  return keycode_function_p (make_keycode (c));
}

static inline lChar
lkey_to_meta (lChar c)
{
  return keycode_value (keycode_function_to_meta_function (make_keycode (c)));
}

static lChar
pre_decode_vkeys (UINT vkey, int syskey)
{
  int i = exkey_index (vkey, syskey);
  if (i < 0)
    return lChar_EOF;

  lisp vec = xsymbol_value (Vextended_key_translate_table);
  if (!general_vector_p (vec))
    return lChar_EOF;

  i <<= 3;
  if (GetKeyState (VK_SHIFT) < 0)
    i |= 1;
  if (GetKeyState (VK_CONTROL) < 0)
    i |= 2;
  if (GetKeyState (VK_MENU) < 0)
    i |= 4;

  if (i >= xvector_length (vec))
    return lChar_EOF;

  lisp cc = xvector_contents (vec)[i];
  if (!charp (cc))
    return lChar_EOF;

  return xchar_value (cc);
}

lChar
lookup_translate_table (lChar cc)
{
  if (cc >= 128)
    return cc;
  lisp vec = xsymbol_value (Vkbd_translate_table);
  if (!general_vector_p (vec))
    return cc;
  if (xvector_length (vec) != 128)
    return cc;
  lisp lcc = xvector_contents (vec)[cc];
  return charp (lcc) ? xchar_value (lcc) : cc;
}

static lChar
decode_vkeys (UINT vkey)
{
  if (vkey == VK_APPS)
    return keycode_value (KEY_APPS);
  if (vkey == VK_PAUSE)
    return keycode_value (KEY_PAUSE);
  if (vkey == VK_SCROLL)
    return keycode_value (KEY_SCROLL);
  if (vkey >= VK_SELECT && vkey <= VK_SNAPSHOT)
    return lChar_EOF;
  if (vkey >= VK_PRIOR && vkey <= VK_HELP)
    return keycode_value (KEY_PRIOR) + vkey - VK_PRIOR;
  if (vkey >= VK_F1 && vkey <= VK_F1 + keycode_value (KEY_Fn_MAX) - keycode_value (KEY_F1))
    return keycode_value (KEY_F1) + vkey - VK_F1;
  return lChar_EOF;
}

static lChar
decode_ctlchars (UINT vk, UINT sc)
{
  int key = to_ascii_char (vk, sc);
  if (key < 0 || maybe_ctrl_char_p (vk))
    return lChar_EOF;
  if (!pseudo_char2ctl_table[key])
    return lChar_EOF;
  return keycode_value (KEY_CHAR_MIN) + pseudo_char2ctl_table[key];
}

lChar
decode_syschars (WPARAM wparam)
{
  lChar cc = lookup_translate_table (wparam);
  if (GetKeyState (VK_MENU) < 0)
    {
      if (ascii_char_p (cc))
        cc = keycode_value (keycode_meta_ascii (cc));
      else if (lkey_function_p (cc))
        cc = lkey_to_meta (cc);
    }
  return cc;
}

lChar
decode_chars (WPARAM wparam)
{
  return decode_syschars (wparam);
}

lChar
decode_syskeys (WPARAM wparam, LPARAM lparam)
{
  if (wparam == VK_TAB)
    return lChar_EOF;

  lChar c = pre_decode_vkeys (wparam, 1);
  if (c != lChar_EOF)
    return c;

  c = decode_vkeys (wparam);
  if (c != lChar_EOF)
    {
      if (GetKeyState (VK_SHIFT) < 0)
        c |= CCF_SHIFT_BIT;
      if (GetKeyState (VK_CONTROL) < 0)
        c |= CCF_CTRL_BIT;
      if (GetKeyState (VK_MENU) < 0)
        c = lkey_to_meta (c);
    }
  else
    {
      c = decode_ctlchars (wparam, lparam);
      if (c != lChar_EOF)
        {
          c = lookup_translate_table (c);
          if (GetKeyState (VK_MENU) < 0 && lkey_function_p (c))
            c = lkey_to_meta (c);
        }
    }
  return c;
}

lChar
decode_keys (WPARAM wparam, LPARAM lparam)
{
  lChar c = pre_decode_vkeys (wparam, 0);
  if (c != lChar_EOF)
    return c;

  c = decode_vkeys (wparam);
  if (c != lChar_EOF)
    {
      if (GetKeyState (VK_SHIFT) < 0)
        c |= CCF_SHIFT_BIT;
      if (GetKeyState (VK_CONTROL) < 0)
        c |= CCF_CTRL_BIT;
      if (GetKeyState (VK_MENU) < 0)  // ???
        c = lkey_to_meta (c);
    }
  else
    {
      c = decode_ctlchars (wparam, lparam);
      if (c != lChar_EOF)
        {
          c = lookup_translate_table (c);
          if (GetKeyState (VK_MENU) < 0 && lkey_function_p (c))
            c = lkey_to_meta (c);
        }
      else
        {
          // handle Meta-Ctrl-XXX
          if (!(lparam & (1L << 29)))
            return lChar_EOF;

          BYTE state[256];
          WORD key[2];
          GetKeyboardState (state);
          if (ToAscii (wparam, lparam, state, key, 0) > 0)
            return lChar_EOF;

          if (wparam >= '@' && wparam < '@' + 0x20)
            c = wparam - '@';
          else
            return lChar_EOF;
          c = lookup_translate_table (c);
          if (GetKeyState (VK_MENU) < 0)
            {
              if (ascii_char_p (c))
                c = keycode_value (keycode_meta_ascii (c));
              else if (lkey_function_p (c))
                c = lkey_to_meta (c);
            }
        }
    }
  return c;
}

int
kbd_queue::copy_queue (keycode_t *b0, int size) const
{
  keycode_t *b = b0 + size;
  for (int i = (head + QUEUE_MAX - 1) % QUEUE_MAX; i != tail && b > b0;
       i = (i + QUEUE_MAX - 1) % QUEUE_MAX)
    if (cc[i] != lChar_EOF && !lchar_menu_p (cc[i]))
      *--b = lchar_to_keycode (cc[i]);
  int l = b0 + size - b;
  if (pending != lChar_EOF && l)
    l--;
  memmove (b0, b, l * sizeof *b);
  return l;
}

void
kbd_queue::paste () const
{
  if (idlep ())
    selected_buffer ()->safe_run_hook (Vsi_paste_hook, 1);
}

int
kbd_queue::reconvert (RECONVERTSTRING *rsbuf, int unicode_p)
{
  if (rsbuf)
    {
      if (!reconv || rsbuf->dwSize < reconv->dwSize)
        {
          xfree (reconv);
          reconv = 0;
          return 0;
        }
      memcpy (rsbuf, reconv, reconv->dwSize);
      xfree (reconv);
      reconv = 0;
      return rsbuf->dwSize;
    }
  else
    {
      if (reconv)
        {
          xfree (reconv);
          reconv = 0;
        }

      if (!idlep ())
        return 0;

      lisp hook = xsymbol_value (Vime_reconvert_helper);
      if (hook == Qunbound || hook == Qnil)
        return 0;

      try
        {
          suppress_gc sgc;
          lisp r = Ffuncall (hook, Qnil);
          int n = multiple_value::count ();
          multiple_value::clear ();
          if (n != 4)
            return 0;

          Buffer *bp = selected_buffer ();
          point_t p1 = bp->coerce_to_restricted_point (r);
          point_t p2 = bp->coerce_to_restricted_point (multiple_value::value (1));
          point_t r1 = bp->coerce_to_restricted_point (multiple_value::value (2));
          point_t r2 = bp->coerce_to_restricted_point (multiple_value::value (3));
          if (p1 == p2 || r1 == r2)
            return 0;
          if (p1 > p2)
            swap (p1, p2);
          if (r1 > r2)
            swap (r1, r2);
          if (r1 < p1 || r2 > p2)
            return 0;

          int size = p2 - p1;
          safe_ptr <Char> data (new Char [size + 1]);
          bp->substring (p1, size, data);
          if constexpr (XYZZY_CHAR_UNICODE)
            {
              if (unicode_p)
                {
                  // A Char is a code point: UTF-16 is made from the Chars
                  // directly, without the bytes of the ACP.
                  size_t l1 = chars_to_utf16 (0, data, r1 - p1);
                  size_t l2 = chars_to_utf16 (0, data + (r1 - p1), r2 - r1);
                  size_t l3 = chars_to_utf16 (0, data + (r2 - p1), p2 - r2);
                  size_t l = l1 + l2 + l3;
                  size = sizeof *reconv + (l + 1) * sizeof (wchar_t);
                  reconv = (RECONVERTSTRING *)xmalloc (size);
                  reconv->dwSize = size;
                  reconv->dwVersion = 0;
                  reconv->dwStrLen = DWORD (l);
                  reconv->dwStrOffset = sizeof *reconv;
                  reconv->dwCompStrLen = DWORD (l2);
                  reconv->dwCompStrOffset = DWORD (l1 * sizeof (wchar_t));
                  reconv->dwTargetStrLen = DWORD (l2);
                  reconv->dwTargetStrOffset = DWORD (l1 * sizeof (wchar_t));
                  wchar_t *w = (wchar_t *)(reconv + 1);
                  chars_to_utf16 (w, data, r1 - p1);
                  chars_to_utf16 (w + l1, data + (r1 - p1), r2 - r1);
                  chars_to_utf16 (w + l1 + l2, data + (r2 - p1), p2 - r2);
                  w[l] = 0;
                  return reconv->dwSize;
                }
            }
          // The bytes for Win32 (the ACP) go to their own buffer: a Char is not
          // a char, and does not have a fixed width.
          safe_ptr <char> bytes (new char [w2s_max (size)]);
          char *b0 = bytes;
          char *b1 = w2s (b0, data, r1 - p1);
          char *b2 = w2s (b1, data + (r1 - p1), r2 - r1);
          char *b3 = w2s (b2, data + (r2 - p1), p2 - r2);
          if (!unicode_p)
            {
              reconv = (RECONVERTSTRING *)xmalloc (sizeof *reconv + (b3 - b0) + 1);
              reconv->dwSize = sizeof *reconv + (b3 - b0) + 1;
              reconv->dwVersion = 0;
              reconv->dwStrLen = b3 - b0;
              reconv->dwStrOffset = sizeof *reconv;
              reconv->dwCompStrLen = b2 - b1;
              reconv->dwCompStrOffset = b1 - b0;
              reconv->dwTargetStrLen = b2 - b1;
              reconv->dwTargetStrOffset = b1 - b0;
              strcpy ((char *)(reconv + 1), b0);
            }
          else
            {
              int l1 = MultiByteToWideChar (CP_ACP, 0, b0, b1 - b0, 0, 0);
              int l2 = MultiByteToWideChar (CP_ACP, 0, b1, b2 - b1, 0, 0);
              int l3 = MultiByteToWideChar (CP_ACP, 0, b2, b3 - b2, 0, 0);
              int l = l1 + l2 + l3;
              size = sizeof *reconv + (l + 1) * sizeof (wchar_t);
              reconv = (RECONVERTSTRING *)xmalloc (size);
              reconv->dwSize = size;
              reconv->dwVersion = 0;
              reconv->dwStrLen = l;
              reconv->dwStrOffset = sizeof *reconv;
              reconv->dwCompStrLen = l2;
              reconv->dwCompStrOffset = l1 * sizeof (wchar_t);
              reconv->dwTargetStrLen = l2;
              reconv->dwTargetStrOffset = l1 * sizeof (wchar_t);
              MultiByteToWideChar (CP_ACP, 0, b0, -1, (wchar_t *)(reconv + 1), l + 1);
            }
          return reconv->dwSize;
        }
      catch (nonlocal_jump &)
        {
          print_condition (nonlocal_jump::data ());
          return 0;
        }
    }
  return 0;
}

int
kbd_queue::documentfeed (RECONVERTSTRING *rsbuf, int unicode_p)
{
  if (!idlep ())
    return 0;

  lisp hook = xsymbol_value (Vime_documentfeed_helper);
  if (hook == Qunbound || hook == Qnil)
    return 0;

  try
    {
      suppress_gc sgc;
      lisp r = Ffuncall (hook, Qnil);
      int n = multiple_value::count ();
      multiple_value::clear ();
      if (n != 2)
        return 0;

      lisp b = multiple_value::value (0);
      lisp c = multiple_value::value (1);

      if constexpr (XYZZY_CHAR_UNICODE)
        {
          if (unicode_p)
            {
              // A Char is a code point: UTF-16 is made from the Chars
              // directly. The lengths are the same as the ones of the ACP
              // path below (dwStrLen counts the bytes with the NUL).
              size_t numc = chars_to_utf16 (0, xstring_contents (c), xstring_length (c));
              size_t numo = chars_to_utf16 (0, xstring_contents (b), xstring_length (b));
              long wlen = long ((numc + 1) * sizeof (wchar_t));
              long woffset = long (numo * sizeof (wchar_t));
              long wsize = sizeof *rsbuf + wlen;
              if (!rsbuf)
                return wsize;

              rsbuf->dwSize = wsize;
              rsbuf->dwVersion = 0;
              rsbuf->dwStrLen = wlen;
              rsbuf->dwStrOffset = sizeof *rsbuf;
              rsbuf->dwCompStrLen = 0;
              rsbuf->dwCompStrOffset = 0;
              rsbuf->dwTargetStrLen = 0;
              rsbuf->dwTargetStrOffset = woffset;
              wchar_t *w = (wchar_t *)(rsbuf + 1);
              chars_to_utf16 (w, xstring_contents (c), xstring_length (c));
              w[numc] = 0;
              return wsize;
            }
        }

      // w2s (lisp) returns a malloc'ed buffer.  Free it at every exit.
      struct sjis_buffer
        {
          char *p;
          sjis_buffer (char *p_) : p (p_) {}
          ~sjis_buffer () {xfree (p);}
        };
      sjis_buffer content_guard (w2s (c));
      sjis_buffer before_guard (w2s (b));
      const char *content = content_guard.p;
      const char *before = before_guard.p;
      long len = strlen (content);
      long offset = strlen (before);
      if (unicode_p)
        {
          int numc = MultiByteToWideChar (CP_ACP, 0, content, len, 0, 0);
          int numo = MultiByteToWideChar (CP_ACP, 0, before, offset, 0, 0);
          len = (numc + 1) * sizeof (wchar_t);
          offset = numo * sizeof (wchar_t);
        }
      long size = sizeof *rsbuf + len;

      if (!rsbuf)
        return size;

      rsbuf->dwSize = size;
      rsbuf->dwVersion = 0;
      rsbuf->dwStrLen = len;
      rsbuf->dwStrOffset = sizeof *rsbuf;
      rsbuf->dwCompStrLen = 0;
      rsbuf->dwCompStrOffset = 0;
      rsbuf->dwTargetStrLen = 0;
      rsbuf->dwTargetStrOffset = offset;

      if (!unicode_p)
        strncpy ((char *)(rsbuf + 1), content, len);
      else
        MultiByteToWideChar (CP_ACP, 0, content, -1, (wchar_t *)(rsbuf + 1), strlen (content));

      return size;
    }
  catch (nonlocal_jump &)
    {
      print_condition (nonlocal_jump::data ());
      return 0;
    }
  return 0;
}

int
kbd_queue::kbd_mblead_p (int c) const
{
  switch (PRIMARYLANGID (kbd_langid ()))
    {
    case LANG_JAPANESE:
      return SJISP (c);

    case LANG_KOREAN:
    case LANG_CHINESE:
      return c >= 0x80;

    default:
      return 0;
    }
}

HKL
kbd_queue::get_kbd_layout () const
{
  if (GetKeyboardLayout)
    return GetKeyboardLayout (0);

  char b[KL_NAMELENGTH];
  if (GetKeyboardLayoutName (b))
    return HKL (LONG_PTR (strtol (b, 0, 16)));
  return HKL (MAKELANGID (LANG_JAPANESE, SUBLANG_DEFAULT));
}

void
kbd_queue::init_kbd_encoding (LANGID langid)
{
  set_kbd_langid (langid);
  ime_prop = app.kbdq.gime.ImmGetProperty (get_kbd_layout (), IGP_PROPERTY);

  unicode_kbd = 0;
  for (lisp p = xsymbol_value (Vkeyboard_layout_list); consp (p); p = xcdr (p))
    {
      long n;
      lisp x = xcar (p);
      if (consp (x)
          && (xcar (x) == Smultiply
              || (safe_fixnum_value (xcar (x), &n)
                  && n == PRIMARYLANGID (langid)))
          && (x = xcdr (x), consp (x))
          && (xcar (x) == Smultiply
              || (safe_fixnum_value (xcar (x), &n)
                  && n == SUBLANGID (langid)))
          && (x = xcdr (x), consp (x)))
        {
          x = xcar (x);
          if (symbolp (x))
            x = xsymbol_value (x);
          if (char_encoding_p (x)
              && xchar_encoding_type (x) != encoding_auto_detect)
            {
              xsymbol_value (Vkbd_encoding) = x;
              return;
            }
          if (x == Qnil)
            {
              unicode_kbd = 1;
              break;
            }
        }
    }
  xsymbol_value (Vkbd_encoding) =
    symbol_value_char_encoding (Vencoding_windows_latin1);
}

const FontObject &
kbd_queue::kbd_encoding_font ()
{
  // the slot comes from font.cc, the same table that chooses the slot of the
  // Han characters of a buffer
  int slot = encoding_font_slot (xsymbol_value (Vkbd_encoding), 0);
  return app.text_font.font (slot < 0 ? FONT_LATIN : slot);
}

key_sequence::key_sequence ()
     : k_used (0), k_last_used (0)
{
}

inline void
key_sequence::store (keycode_t c)
{
  if (k_used < MAX_KEYSEQ)
    k_seq[k_used] = c;
  k_used++;
}

void
key_sequence::notice (int n, int cont)
{
  char buf[256];
  char *b = buf, *be = b + sizeof buf - 32;
  n--;
  for (int i = 0; b < be; i++)
    {
      b = print_key_sequence (b, be, k_seq[i]);
      if (i >= n)
        {
          if (cont)
            *b++ = '-';
          break;
        }
      *b++ = ' ';
    }
  *b = 0;
  app.status_window.puts (buf, 1);
}

void
key_sequence::push (keycode_t c, int f)
{
  store (c);
  if (f)
    notice (k_used, 1);
}

void
key_sequence::done (keycode_t c, int f)
{
  store (c);
  k_last_used = k_used;
  k_used = 0;
  if (k_last_used == 1)
    return;
  if (f)
    notice (k_last_used, 0);
}

void
key_sequence::again (int f)
{
  k_used = k_last_used;
  if (f)
    notice (k_used, 1);
}

static int
count_mblen (const char *string, int l)
{
  xinput_strstream in (string, l);
  encoding_input_stream_helper is (xsymbol_value (Vkbd_encoding), in);
  int len;
  for (len = 0; is->get () != xstream::eof; len++)
    ;
  return len;
}

static void
store_mbs (Char *b, const char *string, int l)
{
  xinput_strstream in (string, l);
  encoding_input_stream_helper is (xsymbol_value (Vkbd_encoding), in);
  int c;
  while ((c = is->get ()) != xstream::eof)
    *b++ = c;
}

void
ime_comp_queue::push (const char *comp, int compl, const char *read, int readl)
{
  int cl = count_mblen (comp, compl);
  int rl = count_mblen (read, readl);
  Char *b = (Char *)malloc ((cl + rl) * sizeof (Char));
  if (!b)
    return;
  qindex = (qindex + 1) % MAX_QUEUE;
  xfree (qbuf[qindex].comp);
  qbuf[qindex].comp = b;
  qbuf[qindex].compl = cl;
  qbuf[qindex].read = b + cl;
  qbuf[qindex].readl = rl;
  store_mbs (qbuf[qindex].comp, comp, compl);
  store_mbs (qbuf[qindex].read, read, readl);
}

static int
store_wcs (Char *b0, const ucs2_t *w, int l, const Char *tab)
{
  Char *b = b0;
  for (const ucs2_t *we = w + l; w < we; w++)
    {
      Char cc;
      if ((!tab || (cc = tab[*w]) == CHAR_NONE)
          && (cc = w2i (*w)) == CHAR_NONE)
        {
          *b++ = utf16_ucs2_to_undef_pair_high (*w);
          cc = utf16_ucs2_to_undef_pair_low (*w);
        }
      *b++ = cc;
    }
  return b - b0;
}

void
ime_comp_queue::push (const ucs2_t *comp, int compl, const ucs2_t *read, int readl,
                      const Char *tab)
{
  Char *b = (Char *)malloc ((compl + readl) * (sizeof (Char) * 2));
  if (!b)
    return;
  qindex = (qindex + 1) % MAX_QUEUE;
  xfree (qbuf[qindex].comp);
  qbuf[qindex].comp = b;
  qbuf[qindex].compl = store_wcs (qbuf[qindex].comp, comp, compl, tab);
  qbuf[qindex].read = b + compl * 2;
  qbuf[qindex].readl = store_wcs (qbuf[qindex].read, read, readl, tab);
}

const ime_comp_queue::pair *
ime_comp_queue::fetch () const
{
  return qbuf[qindex].comp ? &qbuf[qindex] : 0;
}

const ime_comp_queue::pair *
ime_comp_queue::pop ()
{
  const ime_comp_queue::pair *p = fetch ();
  if (p)
    qindex = (qindex - 1 + MAX_QUEUE) % MAX_QUEUE;
  return p;
}

lisp
Fsit_for (lisp timeout, lisp nodisp)
{
  if (!app.kbdq.macro_is_running ())
    {
      if (!nodisp || nodisp == Qnil)
        refresh_screen (0);
      app.kbdq.sit_for (DWORD (coerce_to_double_float (timeout) * 1000));
    }
  return Qnil;
}

lisp
Fsleep_for (lisp timeout)
{
  app.kbdq.sleep_for (DWORD (coerce_to_double_float (timeout) * 1000));
  return Qnil;
}

lisp
Fdo_events ()
{
  app.kbdq.process_events ();
  return Qnil;
}

lisp
Freset_prefix_args (lisp arg, lisp value)
{
  xsymbol_value (Vnext_prefix_args) = arg;
  xsymbol_value (Vnext_prefix_value) = value;
  app.keyseq.again (!app.kbdq.macro_is_running ());
  app.kbdq.close_ime ();
  app.kbdq.keep_next_command_key ();
  Fcontinue_pre_selection ();
  return Qt;
}

lisp
Fset_next_prefix_args (lisp arg, lisp value, lisp c)
{
  xsymbol_value (Vnext_prefix_args) = arg;
  xsymbol_value (Vnext_prefix_value) = value;
  if (c && c != Qnil)
    {
      check_char (c);
      app.keyseq.push (xkeycode (c), !app.kbdq.macro_is_running ());
      Fcontinue_pre_selection ();
      app.kbdq.close_ime ();
      app.kbdq.keep_next_command_key ();
    }
  return Qt;
}

lisp
Fstart_save_kbd_macro ()
{
  if (app.kbdq.save_p ())
    return Qnil;
  app.kbdq.start_macro ();
  return Qt;
}

lisp
Fstop_save_kbd_macro ()
{
  if (!app.kbdq.save_p ())
    return Qnil;
  return app.kbdq.end_macro ();
}

lisp
Fkbd_macro_saving_p ()
{
  return boole (app.kbdq.save_p ());
}

lisp
Ftoggle_ime (lisp arg)
{
  return boole (app.kbdq.toggle_ime (arg
                                     ? (arg == Qnil
                                        ? kbd_queue::IME_MODE_OFF
                                        : kbd_queue::IME_MODE_ON)
                                     : kbd_queue::IME_MODE_TOGGLE));
}

lisp
Fget_recent_keys ()
{
  keycode_t b[128];
  int n = app.kbdq.copy_queue (b, numberof (b));
  lisp keys = make_vector (n, Qnil);
  for (int i = 0; i < n; i++)
    xvector_contents (keys)[i] = make_key_char (b[i]);
  return keys;
}

lisp
Fget_ime_mode ()
{
  return boole (app.ime_open_mode == kbd_queue::IME_MODE_ON);
}

lisp
Fget_ime_composition_string ()
{
  const ime_comp_queue::pair *p = app.ime_compq.fetch ();
  if (!p)
    return Qnil;
  return xcons (make_string (p->comp, p->compl),
                make_string (p->read, p->readl));
}

lisp
Fpop_ime_composition_string ()
{
  const ime_comp_queue::pair *p = app.ime_compq.pop ();
  if (!p)
    return Qnil;
  return xcons (make_string (p->comp, p->compl),
                make_string (p->read, p->readl));
}

lisp
Fset_ime_read_string (lisp string)
{
  char *read;
  if (!string || string == Qnil)
    {
      const ime_comp_queue::pair *p = app.ime_compq.fetch ();
      if (!p)
        return Qnil;
      read = (char *)alloca (w2sl (p->read, p->readl) + 1);
      w2s (read, p->read, p->readl);
    }
  else
    {
      check_string (string);
      read = (char *)alloca (w2sl (string) + 1);
      w2s (read, string);
    }
  HIMC hIMC = app.kbdq.gime.ImmGetContext (app.toplev);
  if (!hIMC)
    return Qnil;
  int f = app.kbdq.gime.ImmSetCompositionString (hIMC, SCS_SETSTR, 0, 0,
                                                 read, strlen (read));
  app.kbdq.gime.ImmReleaseContext (app.toplev, hIMC);
  return boole (f);
}

lisp
Fime_register_word_dialog (lisp lcomp, lisp lread)
{
  REGISTERWORD rw;
  rw.lpWord = rw.lpReading = 0;
  if (lcomp && lcomp != Qnil)
    {
      check_string (lcomp);
      rw.lpWord = (char *)alloca (w2sl (lcomp) + 1);
      w2s (rw.lpWord, lcomp);
    }
  if (lread && lread != Qnil)
    {
      check_string (lread);
      rw.lpReading = (char *)alloca (w2sl (lread) + 1);
      w2s (rw.lpReading, lread);
    }
  return boole (app.kbdq.gime.ImmConfigureIME (GetKeyboardLayout (0), app.toplev,
                                               IME_CONFIG_REGISTERWORD, &rw));
}

lisp
Fenable_global_ime (lisp f)
{
  if (f == Qnil)
    app.kbdq.gime.disable ();
  else if (!app.kbdq.gime.enable (&app.atom_toplev, 1))
    FEsimple_win32_error (GetLastError ());
  return Qt;
}

static int
get_kbd_layout_name (HKL hkl, char *buf, int size)
{
  char k[256];
  sprintf (k, "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%08x", u_int (UINT_PTR (hkl)));
  ReadRegistry r (HKEY_LOCAL_MACHINE, k);
  return ((!r.fail () && r.get ("Layout Text", buf, size) > 0)
          || app.kbdq.gime.ImmGetDescription (hkl, buf, size) > 0);
}

typedef UINT (WINAPI *GETKEYBOARDLAYOUTLIST)(int, HKL *);
static const GETKEYBOARDLAYOUTLIST pGetKeyboardLayoutList =
  (GETKEYBOARDLAYOUTLIST)GetProcAddress (GetModuleHandle ("user32"),
                                         "GetKeyboardLayoutList");

lisp
Flist_kbd_layout ()
{
  if (!pGetKeyboardLayoutList)
    return Qnil;
  int n = (*pGetKeyboardLayoutList)(0, 0);
  if (!n)
    return Qnil;
  HKL *h = (HKL *)alloca (sizeof *h * n);
  n = GetKeyboardLayoutList (n, h);
  if (!n)
    FEsimple_win32_error (GetLastError ());

  lisp r = Qnil;
  for (int i = 0; i < n; i++)
    {
      char buf[256];
      if (get_kbd_layout_name (h[i], buf, sizeof buf)
          || get_kbd_layout_name (HKL (HIWORD (h[i])), buf, sizeof buf))
        r = xcons (xcons (make_fixnum (int (LONG_PTR (h[i]))),
                          make_string (buf)),
                   r);
    }
  return r;
}

lisp
Fselect_kbd_layout (lisp layout)
{
  HKL hkl;
  if (stringp (layout))
    {
      if (!pGetKeyboardLayoutList)
        return Qnil;
      int n = (*pGetKeyboardLayoutList)(0, 0);
      if (!n)
        return Qnil;
      HKL *h = (HKL *)alloca (sizeof *h * n);
      n = GetKeyboardLayoutList (n, h);
      if (!n)
        FEsimple_win32_error (GetLastError ());

      char name[256];
      w2s (name, name + sizeof name,
           xstring_contents (layout), xstring_length (layout));

      int i;
      for (i = 0; i < n; i++)
        {
          char buf[256];
          if ((get_kbd_layout_name (h[i], buf, sizeof buf)
               || get_kbd_layout_name (HKL (LOWORD (h[i])), buf, sizeof buf))
              && !strcmp (buf, name))
            {
              hkl = h[i];
              break;
            }
        }
      if (i == n)
        return Qnil;
    }
  else if (layout == Qnil)
    hkl = HKL (HKL_NEXT);
  else if (layout == Qt)
    hkl = HKL (HKL_PREV);
  else
    {
      if (!short_int_long_p (layout))
        FEtype_error (layout, xsymbol_value (Qor_string_integer));
      hkl = HKL (LONG_PTR (fixnum_value (layout)));
    }
  if (!ActivateKeyboardLayout (hkl, 0))
    FEsimple_win32_error (GetLastError ());
  return Qt;
}

lisp
Fcurrent_kbd_layout ()
{
  HKL hkl = app.kbdq.get_kbd_layout ();
  char buf[256];
  if (get_kbd_layout_name (hkl, buf, sizeof buf)
      || get_kbd_layout_name (HKL (HIWORD (hkl)), buf, sizeof buf))
    return xcons (make_fixnum (int (LONG_PTR (hkl))), make_string (buf));
  return xcons (make_fixnum (int (LONG_PTR (hkl))), Qnil);
}

int
char_mouse_move_p (keycode_t cc)
{
  if (keycode_function_p (cc))
    cc = keycode_meta_function_to_function (make_keycode (keycode_value (cc)
                                                          & ~u_int32_t (CCF_SHIFT_BIT | CCF_CTRL_BIT)));
  return cc == KEY_MOUSEMOVE;
}

// For tests: put a character (a key or a text character) into the key queue.
// mark: 0 = keyboard, 1 = mouse (LCHAR_MOUSE), 2 = menu (LCHAR_MENU).
lisp
Fsi_kbd_queue_put (lisp lc, lisp lmark)
{
  check_char (lc);
  lChar c = xchar_value (lc);
  int mark = lmark && lmark != Qnil ? fixnum_value (lmark) : 0;
  if (mark == 1)
    c |= LCHAR_MOUSE;
  else if (mark == 2)
    c |= LCHAR_MENU;
  else if (mark)
    FErange_error (lmark);
  return boole (app.kbdq.put_key (c));
}

// Is the object a kbd macro (a vector of characters, or an old string)?
lisp
Fsi_kbd_macro_p (lisp x)
{
  return boole (kbd_macro_p (x));
}

// For tests: UTF-16 code units to Chars, by the rules of the given side
// (nil = the side that this build uses).  Returns a list of the values.
//   (editor::*utf16-to-chars units [unicode [half-width]])
lisp
Fsi_utf16_to_chars (lisp lunits, lisp lunicode, lisp lhalf)
{
  bool unicode = lunicode && lunicode != Qnil
    ? fixnum_value (lunicode) != 0
    : XYZZY_CHAR_UNICODE != 0;
  bool half = lhalf && lhalf != Qnil;
  std::vector <Char> v;
  kbd_utf16_decoder dec;
  Char out[kbd_utf16_decoder::OUT_MAX];
  for (lisp p = lunits; consp (p); p = xcdr (p))
    {
      QUIT;
      long u = fixnum_value (xcar (p));
      if (u < 0 || u > 0xffff)
        FErange_error (xcar (p));
      int n = dec.feed (ucs2_t (u), out, unicode, 0, half);
      v.insert (v.end (), out, out + n);
    }
  int n = dec.flush (out, unicode);
  v.insert (v.end (), out, out + n);

  lisp r = Qnil;
  for (size_t i = v.size (); i > 0; i--)
    r = xcons (make_fixnum (v[i - 1]), r);
  return r;
}

// For tests: put a UTF-16 code unit into the key queue as the keyboard does.
lisp
Fsi_kbd_queue_put_utf16 (lisp lunit)
{
  long u = fixnum_value (lunit);
  if (u < 0 || u > 0xffff)
    FErange_error (lunit);
  return boole (app.kbdq.put_utf16 (ucs2_t (u)));
}
