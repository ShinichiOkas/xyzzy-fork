#include "stdafx.h"
#include "cdecl.h"
#include "wfile.h"

namespace wfile
{

// ---------------------------------------------------------------------
// 変換
//
// 内部の文字列は今のところ cp932 の符号を Char に入れたもの（Char の 8bit 以下は
// 1 バイト文字、それより上は上位バイトが先行バイトの 2 バイト文字）。narrow は
// cp932 のバイト列。どちらも MultiByteToWideChar (932) で UTF-16 にする。これは
// 今まで A 版の API が（日本語の ANSI コードページで）していた変換と同じ。
// 内部の符号を Unicode のコードポイントに広げるときは、この 4 つの関数だけを
// 替える。
// ---------------------------------------------------------------------

static const UINT CODEPAGE = 932;

int
narrow_to_wide (wchar_t *w, int wsize, const char *s, int len)
{
  int zt = len < 0;
  if (zt)
    len = int (strlen (s));
  if (!len)
    {
      if (wsize > 0)
        *w = 0;
      return wsize > 0 ? 0 : -1;
    }
  int n = MultiByteToWideChar (CODEPAGE, 0, s, len, w, wsize > 0 ? wsize - 1 : 0);
  if (!n || n >= wsize)
    {
      if (wsize > 0)
        *w = 0;
      return -1;
    }
  w[n] = 0;
  return n;
}

int
chars_to_wide (wchar_t *w, int wsize, const Char *s, int len)
{
  if (len < 0)
    {
      len = 0;
      while (s[len])
        len++;
    }
  if (len > WPATH_BUF)
    {
      if (wsize > 0)
        *w = 0;
      return -1;
    }
  char *b = (char *)alloca (len * 2 + 1);
  char *p = b;
  for (int i = 0; i < len; i++)
    {
      if (s[i] >= 256)
        *p++ = char (s[i] >> 8);
      *p++ = char (s[i]);
    }
  return narrow_to_wide (w, wsize, b, int (p - b));
}

int
wide_to_narrow (char *s, int size, const wchar_t *w, int wlen)
{
  if (wlen < 0)
    wlen = int (wcslen (w));
  if (!wlen)
    {
      if (size > 0)
        *s = 0;
      return size > 0 ? 0 : -1;
    }
  int n = WideCharToMultiByte (CODEPAGE, WC_NO_BEST_FIT_CHARS, w, wlen,
                               s, size > 0 ? size - 1 : 0, 0, 0);
  if (!n || n >= size)
    {
      if (size > 0)
        *s = 0;
      return -1;
    }
  s[n] = 0;
  return n;
}

int
wide_to_chars (Char *s, int size, const wchar_t *w, int wlen)
{
  if (wlen < 0)
    wlen = int (wcslen (w));
  char *b = (char *)alloca (wlen * 2 + 1);
  int n = wide_to_narrow (b, wlen * 2 + 1, w, wlen);
  if (n < 0)
    {
      if (size > 0)
        *s = 0;
      return -1;
    }
  int l = 0;
  for (int i = 0; i < n; l++)
    {
      if (l + 1 >= size)
        {
          if (size > 0)
            *s = 0;
          return -1;
        }
      u_char c = u_char (b[i]);
      if (i + 1 < n && IsDBCSLeadByteEx (CODEPAGE, c))
        {
          s[l] = Char (c << 8 | u_char (b[i + 1]));
          i += 2;
        }
      else
        {
          s[l] = c;
          i++;
        }
    }
  s[l] = 0;
  return l;
}

// ---------------------------------------------------------------------
// パス
// ---------------------------------------------------------------------

static inline int
sep_p (wchar_t c)
{
  return c == L'\\' || c == L'/';
}

static inline int
alpha_p (wchar_t c)
{
  return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

// \\?\ か \\.\ で始まる（NT の名前）
static int
nt_name_p (const wchar_t *w)
{
  return (w[0] == L'\\' && w[1] == L'\\'
          && (w[2] == L'?' || w[2] == L'.') && w[3] == L'\\');
}

void
path::build (const wchar_t *w, int len, prefix_t prefix)
{
  m_ok = 0;
  m_len = 0;
  m_buf[0] = 0;
  if (len < 0 || len >= WPATH_BUF)
    return;

  if (prefix == NOPREFIX || len == 0 || nt_name_p (w))
    {
      memcpy (m_buf, w, (len + 1) * sizeof *w);
      m_len = len;
      m_ok = 1;
      return;
    }

  // 絶対パスに直した長さで長いか決める（相対パス、/、.、.. もここで片付く）。
  // 短いときは元の文字列をそのまま使う（今までの A 版と同じ振る舞いにする）。
  wchar_t full[WPATH_BUF];
  DWORD fl = GetFullPathNameW (w, WPATH_BUF, full, 0);
  if (!fl || fl >= WPATH_BUF || fl < LONG_PATH_THRESHOLD)
    {
      memcpy (m_buf, w, (len + 1) * sizeof *w);
      m_len = len;
      m_ok = 1;
      return;
    }

  if (nt_name_p (full))
    {
      memcpy (m_buf, full, (fl + 1) * sizeof *full);
      m_len = int (fl);
      m_ok = 1;
      return;
    }

  const wchar_t *head;
  const wchar_t *body;
  if (alpha_p (full[0]) && full[1] == L':' && full[2] == L'\\')
    {
      head = L"\\\\?\\";
      body = full;
    }
  else if (full[0] == L'\\' && full[1] == L'\\')
    {
      head = L"\\\\?\\UNC\\";
      body = full + 2;
    }
  else
    {
      memcpy (m_buf, w, (len + 1) * sizeof *w);
      m_len = len;
      m_ok = 1;
      return;
    }

  int hl = int (wcslen (head));
  int bl = int (wcslen (body));
  if (hl + bl >= WPATH_BUF)
    return;
  memcpy (m_buf, head, hl * sizeof *head);
  memcpy (m_buf + hl, body, (bl + 1) * sizeof *body);
  m_len = hl + bl;
  m_ok = 1;
}

path::path (const char *s, prefix_t prefix)
{
  wchar_t w[WPATH_BUF];
  int n = s ? narrow_to_wide (w, WPATH_BUF, s) : -1;
  if (n < 0)
    {
      m_ok = 0;
      m_len = 0;
      m_buf[0] = 0;
    }
  else
    build (w, n, prefix);
}

path::path (const Char *s, prefix_t prefix)
{
  wchar_t w[WPATH_BUF];
  int n = s ? chars_to_wide (w, WPATH_BUF, s) : -1;
  if (n < 0)
    {
      m_ok = 0;
      m_len = 0;
      m_buf[0] = 0;
    }
  else
    build (w, n, prefix);
}

path::path (const Char *s, int len, prefix_t prefix)
{
  wchar_t w[WPATH_BUF];
  int n = s ? chars_to_wide (w, WPATH_BUF, s, len) : -1;
  if (n < 0)
    {
      m_ok = 0;
      m_len = 0;
      m_buf[0] = 0;
    }
  else
    build (w, n, prefix);
}

int
path::share_root (wchar_t *out, int size, int noshare_ok) const
{
  const wchar_t *p = m_buf;
  if (!wcsncmp (p, L"\\\\?\\UNC\\", 8))
    p += 8;
  else if (sep_p (p[0]) && sep_p (p[1]))
    p += 2;
  else
    return 0;
  const wchar_t *start = p;
  // server
  while (*p && !sep_p (*p))
    p++;
  const wchar_t *e;
  if (*p)
    {
      // share
      for (e = p + 1; *e && !sep_p (*e); e++)
        ;
    }
  else if (noshare_ok)
    e = p;
  else
    return 0;
  int l = int (e - start);
  if (l + 3 > size)
    return 0;
  out[0] = out[1] = L'\\';
  memcpy (out + 2, start, l * sizeof *out);
  out[l + 2] = 0;
  for (int i = 2; i < l + 2; i++)
    if (out[i] == L'/')
      out[i] = L'\\';
  return 1;
}

// ---------------------------------------------------------------------
// 失敗時の共通処理
// ---------------------------------------------------------------------

#define CHECK_PATH(P, FAILED) \
  do { \
    if (!(P).ok ()) \
      { \
        SetLastError (ERROR_FILENAME_EXCED_RANGE); \
        return FAILED; \
      } \
  } while (0)

// ---------------------------------------------------------------------
// ディレクトリ列挙
// ---------------------------------------------------------------------

static void
copy_find_data (find_data &fd, const WIN32_FIND_DATAW &w)
{
  fd.dwFileAttributes = w.dwFileAttributes;
  fd.ftCreationTime = w.ftCreationTime;
  fd.ftLastAccessTime = w.ftLastAccessTime;
  fd.ftLastWriteTime = w.ftLastWriteTime;
  fd.nFileSizeHigh = w.nFileSizeHigh;
  fd.nFileSizeLow = w.nFileSizeLow;
  if (wide_to_narrow (fd.cFileName, sizeof fd.cFileName, w.cFileName) < 0)
    *fd.cFileName = 0;
}

HANDLE
find_first_file (const path &p, find_data &fd)
{
  CHECK_PATH (p, INVALID_HANDLE_VALUE);
  WIN32_FIND_DATAW w;
  HANDLE h = FindFirstFileExW (p, FindExInfoBasic, &w, FindExSearchNameMatch, 0, 0);
  if (h != INVALID_HANDLE_VALUE)
    copy_find_data (fd, w);
  return h;
}

BOOL
find_next_file (HANDLE h, find_data &fd)
{
  WIN32_FIND_DATAW w;
  w.cFileName[0] = 0;
  if (FindNextFileW (h, &w)
      || (GetLastError () == ERROR_MORE_DATA && w.cFileName[0]))
    {
      copy_find_data (fd, w);
      return 1;
    }
  return 0;
}

// ---------------------------------------------------------------------
// 素の W API
// ---------------------------------------------------------------------

HANDLE
create_file (const path &p, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
             DWORD disposition, DWORD flags, HANDLE tmpl)
{
  CHECK_PATH (p, INVALID_HANDLE_VALUE);
  return CreateFileW (p, access, share, sa, disposition, flags, tmpl);
}

BOOL
delete_file (const path &p)
{
  CHECK_PATH (p, FALSE);
  return DeleteFileW (p);
}

BOOL
move_file (const path &from, const path &to)
{
  CHECK_PATH (from, FALSE);
  CHECK_PATH (to, FALSE);
  return MoveFileW (from, to);
}

BOOL
copy_file (const path &from, const path &to, BOOL fail_if_exists)
{
  CHECK_PATH (from, FALSE);
  CHECK_PATH (to, FALSE);
  return CopyFileW (from, to, fail_if_exists);
}

BOOL
create_directory (const path &p)
{
  CHECK_PATH (p, FALSE);
  return CreateDirectoryW (p, 0);
}

BOOL
remove_directory (const path &p)
{
  CHECK_PATH (p, FALSE);
  return RemoveDirectoryW (p);
}

DWORD
get_file_attributes (const path &p)
{
  CHECK_PATH (p, INVALID_FILE_ATTRIBUTES);
  return GetFileAttributesW (p);
}

BOOL
set_file_attributes (const path &p, DWORD a)
{
  CHECK_PATH (p, FALSE);
  return SetFileAttributesW (p, a);
}

BOOL
get_file_size (HANDLE h, int64_t *size)
{
  LARGE_INTEGER li;
  if (!GetFileSizeEx (h, &li))
    return 0;
  *size = li.QuadPart;
  return 1;
}

FILE *
fopen (const path &p, const char *mode, int shflag)
{
  if (!p.ok ())
    {
      errno = ENOENT;
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  wchar_t wmode[16];
  int i = 0;
  for (; mode[i] && i < 15; i++)
    wmode[i] = wchar_t (u_char (mode[i]));
  wmode[i] = 0;
  return _wfsopen (p, wmode, shflag);
}

HMODULE
load_library (const path &p)
{
  CHECK_PATH (p, 0);
  return LoadLibraryW (p);
}

BOOL
set_current_directory (const char *s)
{
  path p (s, path::NOPREFIX);
  CHECK_PATH (p, FALSE);
  return SetCurrentDirectoryW (p);
}

DWORD
get_current_directory (char *buf, DWORD size)
{
  wchar_t w[WPATH_BUF];
  DWORD l = GetCurrentDirectoryW (WPATH_BUF, w);
  if (!l || l >= WPATH_BUF)
    return 0;
  int n = wide_to_narrow (buf, int (size), w, int (l));
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  return n;
}

DWORD
get_current_directory (Char *buf, DWORD size)
{
  wchar_t w[WPATH_BUF];
  DWORD l = GetCurrentDirectoryW (WPATH_BUF, w);
  if (!l || l >= WPATH_BUF)
    return 0;
  int n = wide_to_chars (buf, int (size), w, int (l));
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  return n;
}

// GetFullPathNameW の結果の file part のオフセットを narrow / Char の位置に直す
static DWORD
full_path_result (const wchar_t *full, const wchar_t *part, char *buf, DWORD size,
                  int *file_part)
{
  int n = wide_to_narrow (buf, int (size), full);
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  if (file_part)
    {
      *file_part = -1;
      if (part)
        *file_part = WideCharToMultiByte (CODEPAGE, WC_NO_BEST_FIT_CHARS,
                                          full, int (part - full), 0, 0, 0, 0);
    }
  return n;
}

DWORD
get_full_path_name (const char *s, char *buf, DWORD size, int *file_part)
{
  wchar_t w[WPATH_BUF], full[WPATH_BUF];
  if (narrow_to_wide (w, WPATH_BUF, s) < 0)
    {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  wchar_t *part = 0;
  DWORD l = GetFullPathNameW (w, WPATH_BUF, full, &part);
  if (!l)
    return 0;
  if (l >= WPATH_BUF)
    {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  return full_path_result (full, part, buf, size, file_part);
}

DWORD
get_full_path_name (const Char *s, Char *buf, DWORD size, int *file_part)
{
  wchar_t w[WPATH_BUF], full[WPATH_BUF];
  if (chars_to_wide (w, WPATH_BUF, s) < 0)
    {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  wchar_t *part = 0;
  DWORD l = GetFullPathNameW (w, WPATH_BUF, full, &part);
  if (!l)
    return 0;
  if (l >= WPATH_BUF)
    {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  int n = wide_to_chars (buf, int (size), full);
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  if (file_part)
    {
      *file_part = -1;
      if (part)
        {
          Char tmp[WPATH_BUF];
          *file_part = wide_to_chars (tmp, WPATH_BUF, full, int (part - full));
        }
    }
  return n;
}

DWORD
get_temp_path (char *buf, DWORD size)
{
  wchar_t w[WPATH_BUF];
  DWORD l = GetTempPathW (WPATH_BUF, w);
  if (!l || l >= WPATH_BUF)
    return 0;
  int n = wide_to_narrow (buf, int (size), w, int (l));
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  return n;
}

DWORD
get_module_file_name (HMODULE h, char *buf, DWORD size)
{
  wchar_t w[WPATH_BUF];
  DWORD l = GetModuleFileNameW (h, w, WPATH_BUF);
  if (!l || l >= WPATH_BUF)
    return 0;
  int n = wide_to_narrow (buf, int (size), w, int (l));
  if (n < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  return n;
}

UINT
get_windows_directory (char *buf, UINT size)
{
  wchar_t w[MAX_PATH];
  UINT l = GetWindowsDirectoryW (w, MAX_PATH);
  if (!l || l >= MAX_PATH)
    return 0;
  int n = wide_to_narrow (buf, int (size), w, int (l));
  return n < 0 ? 0 : n;
}

UINT
get_system_directory (char *buf, UINT size)
{
  wchar_t w[MAX_PATH];
  UINT l = GetSystemDirectoryW (w, MAX_PATH);
  if (!l || l >= MAX_PATH)
    return 0;
  int n = wide_to_narrow (buf, int (size), w, int (l));
  return n < 0 ? 0 : n;
}

UINT
get_temp_file_name (const char *dir, const char *prefix, UINT unique, char *out)
{
  path d (dir, path::NOPREFIX);
  CHECK_PATH (d, 0);
  wchar_t wprefix[8];
  if (narrow_to_wide (wprefix, 8, prefix) < 0)
    {
      SetLastError (ERROR_INVALID_PARAMETER);
      return 0;
    }
  wchar_t w[MAX_PATH];
  UINT r = GetTempFileNameW (d, wprefix, unique, w);
  if (!r)
    return 0;
  if (wide_to_narrow (out, MAX_PATH * 2, w) < 0)
    {
      SetLastError (ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
  return r;
}

UINT
get_drive_type (const char *root)
{
  path p (root, path::NOPREFIX);
  CHECK_PATH (p, DRIVE_UNKNOWN);
  return GetDriveTypeW (p);
}

BOOL
get_volume_information (const char *root, char *volname, DWORD volsize,
                        DWORD *serial, DWORD *maxl, DWORD *flags,
                        char *fsname, DWORD fssize)
{
  path p (root, path::NOPREFIX);
  CHECK_PATH (p, FALSE);
  wchar_t wvol[MAX_PATH + 1], wfs[MAX_PATH + 1];
  if (!GetVolumeInformationW (p, volname ? wvol : 0, volname ? MAX_PATH + 1 : 0,
                              serial, maxl, flags,
                              fsname ? wfs : 0, fsname ? MAX_PATH + 1 : 0))
    return 0;
  if (volname && wide_to_narrow (volname, int (volsize), wvol) < 0)
    *volname = 0;
  if (fsname && wide_to_narrow (fsname, int (fssize), wfs) < 0)
    *fsname = 0;
  return 1;
}

BOOL
get_disk_free_space_ex (const char *root, uint64_t *avail,
                        uint64_t *total, uint64_t *free_total)
{
  if (!root)
    return GetDiskFreeSpaceExW (0, (PULARGE_INTEGER)avail, (PULARGE_INTEGER)total,
                                (PULARGE_INTEGER)free_total);
  path p (root, path::NOPREFIX);
  CHECK_PATH (p, FALSE);
  return GetDiskFreeSpaceExW (p, (PULARGE_INTEGER)avail, (PULARGE_INTEGER)total,
                              (PULARGE_INTEGER)free_total);
}

BOOL
get_disk_free_space (const char *root, DWORD *spc, DWORD *bps, DWORD *free_clusters,
                     DWORD *total_clusters)
{
  if (!root)
    return GetDiskFreeSpaceW (0, spc, bps, free_clusters, total_clusters);
  path p (root, path::NOPREFIX);
  CHECK_PATH (p, FALSE);
  return GetDiskFreeSpaceW (p, spc, bps, free_clusters, total_clusters);
}

DWORD
search_path (const char *dir, const char *file, const char *ext,
             char *buf, DWORD size, int *file_part)
{
  wchar_t wdir[WPATH_BUF], wfile[WPATH_BUF], wext[64], w[WPATH_BUF];
  if ((dir && narrow_to_wide (wdir, WPATH_BUF, dir) < 0)
      || narrow_to_wide (wfile, WPATH_BUF, file) < 0
      || (ext && narrow_to_wide (wext, 64, ext) < 0))
    {
      SetLastError (ERROR_FILENAME_EXCED_RANGE);
      return 0;
    }
  wchar_t *part = 0;
  DWORD l = SearchPathW (dir ? wdir : 0, wfile, ext ? wext : 0, WPATH_BUF, w, &part);
  if (!l || l >= WPATH_BUF)
    return 0;
  return full_path_result (w, part, buf, size, file_part);
}

}
