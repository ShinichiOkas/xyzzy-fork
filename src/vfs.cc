#include "stdafx.h"
#include "ed.h"
#include "dyn-handle.h"

class NetPassDlg
{
  HWND hwnd;
public:
  char username[256];
  char passwd[256];
  const char *remote;

private:
  static INT_PTR CALLBACK netpass_dlgproc (HWND, UINT, WPARAM, LPARAM);
  BOOL dlgproc (UINT, WPARAM, LPARAM);
  void do_command (int, int);
  void init_dialog ();

public:
  NetPassDlg (const char *);
  int do_modal ();
};

NetPassDlg::NetPassDlg (const char *r)
     : remote (r)
{
  *username = 0;
  *passwd = 0;
}

void
NetPassDlg::do_command (int id, int code)
{
  switch (id)
    {
    case IDOK:
      GetDlgItemText (hwnd, IDC_USERNAME, username, sizeof username);
      GetDlgItemText (hwnd, IDC_PASSWD, passwd, sizeof passwd);
      /* fall thru... */
    case IDCANCEL:
      EndDialog (hwnd, id);
      break;
    }
}

void
NetPassDlg::init_dialog ()
{
  center_window (hwnd);
  set_window_icon (hwnd);
  SetDlgItemText (hwnd, IDC_SHARE_NAME, remote);
}

BOOL
NetPassDlg::dlgproc (UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
    {
    case WM_INITDIALOG:
      init_dialog ();
      return 1;

    case WM_COMMAND:
      do_command (LOWORD (wparam), HIWORD (wparam));
      return 1;

    default:
      return 0;
    }
}

INT_PTR CALLBACK
NetPassDlg::netpass_dlgproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  NetPassDlg *p;
  if (msg == WM_INITDIALOG)
    {
      p = (NetPassDlg *)lparam;
      SetWindowLongPtr (hwnd, DWLP_USER, lparam);
      p->hwnd = hwnd;
    }
  else
    {
      p = (NetPassDlg *)GetWindowLongPtr (hwnd, DWLP_USER);
      if (!p)
        return 0;
    }
  return p->dlgproc (msg, wparam, lparam);
}

int
NetPassDlg::do_modal ()
{
  return DialogBoxParam (app.hinst, MAKEINTRESOURCE (IDD_NETPASSWD),
                         get_active_window (), netpass_dlgproc, LPARAM (this)) == IDOK;
}

// FN は式。拒否されたら（共有の資格情報を聞けたら）もう一度呼ぶ。
// 引数のパスは呼び出しの前に変換済みなので、失敗の後の GetLastError は
// FN の結果のまま。
#define WINFS_CALL(TYPE, FAILED, ASKPASS, FN) \
  { TYPE r = FN; \
    if (r == (FAILED) && ASKPASS) \
      r = FN; \
    return r; }
#define WINFS_CALL1(TYPE, FAILED, PATH, FN) \
  WINFS_CALL (TYPE, FAILED, askpass (PATH), FN)
#define WINFS_CALL2(TYPE, FAILED, PATH1, PATH2, FN) \
  WINFS_CALL (TYPE, FAILED, askpass (PATH1, PATH2), FN)

static int
try_connect (wchar_t *remote, int e)
{
  NETRESOURCEW nr;
  nr.dwType = RESOURCETYPE_DISK;
  nr.lpLocalName = 0;
  nr.lpRemoteName = remote;
  nr.lpProvider = 0;

  if (e == ERROR_ACCESS_DENIED
      && WNetAddConnection2W (&nr, 0, 0, 0) == NO_ERROR)
    return 1;

  char nremote[MAX_PATH * 4];
  if (wfile::wide_to_narrow (nremote, sizeof nremote, remote) < 0)
    *nremote = 0;

  while (1)
    {
      NetPassDlg d (nremote);
      if (!d.do_modal ())
        return 0;

      wchar_t user[256], passwd[256];
      if (wfile::narrow_to_wide (user, 256, d.username) < 0)
        *user = 0;
      if (wfile::narrow_to_wide (passwd, 256, d.passwd) < 0)
        *passwd = 0;
      switch (WNetAddConnection2W (&nr, passwd, user, 0))
        {
        case NO_ERROR:
          return 1;

        case ERROR_INVALID_PASSWORD:
        case ERROR_LOGON_FAILURE:
        case ERROR_ACCESS_DENIED:
          break;

        default:
          return 0;
        }
    }
}

static int
askpass1 (const wfile::path &path, int noshare_ok)
{
  if (!path.ok ())
    return 0;

  int e = GetLastError ();
  switch (e)
    {
    default:
      return 0;

    case ERROR_ACCESS_DENIED:
    case ERROR_INVALID_PASSWORD:
    case ERROR_LOGON_FAILURE:
      break;
    }

  wchar_t remote[MAX_PATH * 2];
  if (!path.share_root (remote, MAX_PATH * 2, noshare_ok))
    return 0;
  if (!_wcsicmp (WINFS::wfs_share_cache, remote))
    return 0;
  if (try_connect (remote, e))
    {
      *WINFS::wfs_share_cache = 0;
      return 1;
    }
  wcscpy_s (WINFS::wfs_share_cache, MAX_PATH * 2, remote);
  SetLastError (e);
  return 0;
}

static inline int
askpass (const wfile::path &path)
{
  return askpass1 (path, 0);
}

static inline int
askpass_noshare (const wfile::path &path)
{
  return askpass1 (path, 1);
}

static inline int
askpass (const wfile::path &path1, const wfile::path &path2)
{
  return askpass1 (path1, 0) || askpass1 (path2, 0);
}

wchar_t WINFS::wfs_share_cache[MAX_PATH * 2];

BOOL
WINFS::CreateDirectory (const path &p)
{
  WINFS_CALL1 (BOOL, FALSE, p, wfile::create_directory (p));
}

HANDLE
WINFS::CreateFile (const path &p, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
  HANDLE r = wfile::create_file (p, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                 dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  if (r != INVALID_HANDLE_VALUE)
    return r;
  if (!(dwFlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS))
    {
      int e = GetLastError ();
      if (e == ERROR_ACCESS_DENIED)
        {
          DWORD a = wfile::get_file_attributes (p);
          SetLastError (e);
          if (a != -1 && a & FILE_ATTRIBUTE_DIRECTORY)
            return r;
        }
    }
  if (askpass (p))
    r = wfile::create_file (p, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  return r;
}

BOOL
WINFS::CopyFile (const path &from, const path &to, BOOL fail_if_exists)
{
  WINFS_CALL2 (BOOL, FALSE, from, to, wfile::copy_file (from, to, fail_if_exists));
}

BOOL
WINFS::DeleteFile (const path &p)
{
  WINFS_CALL1 (BOOL, FALSE, p, wfile::delete_file (p));
}

HANDLE
WINFS::FindFirstFile (const path &p, find_data *fd)
{
  WINFS_CALL1 (HANDLE, INVALID_HANDLE_VALUE, p, wfile::find_first_file (p, *fd));
}

BOOL
WINFS::FindNextFile (HANDLE h, find_data *fd)
{
  *fd->cFileName = 0;
  return wfile::find_next_file (h, *fd);
}

BOOL
WINFS::GetDiskFreeSpace (const char *root, LPDWORD lpSectorsPerCluster,
                         LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                         LPDWORD lpTotalNumberOfClusters)
{
  BOOL r = wfile::get_disk_free_space (root, lpSectorsPerCluster, lpBytesPerSector,
                                       lpNumberOfFreeClusters, lpTotalNumberOfClusters);
  if (!r)
    {
      if (GetLastError () == ERROR_NOT_SUPPORTED)
        {
          *lpSectorsPerCluster = 1;
          *lpBytesPerSector = 4096;
        }
      else
        {
          if (!root || !askpass (wfile::path (root, wfile::path::NOPREFIX)))
            return 0;
          r = wfile::get_disk_free_space (root, lpSectorsPerCluster, lpBytesPerSector,
                                          lpNumberOfFreeClusters, lpTotalNumberOfClusters);
          if (!r)
            return 0;
        }
    }

  uint64_t FreeBytesAvailableToCaller;
  uint64_t TotalNumberOfBytes;
  uint64_t TotalNumberOfFreeBytes;
  if (wfile::get_disk_free_space_ex (root, &FreeBytesAvailableToCaller,
                                     &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
    {
      DWORD blk = *lpSectorsPerCluster * *lpBytesPerSector;
      if (!blk)
        blk = 512;
      *lpTotalNumberOfClusters = DWORD (TotalNumberOfBytes / blk);
      *lpNumberOfFreeClusters = DWORD (TotalNumberOfFreeBytes / blk);
      r = 1;
    }

  return r;
}

static DWORD
get_file_attributes_ask (const wfile::path &p)
{
  WINFS_CALL1 (DWORD, -1, p, wfile::get_file_attributes (p));
}

DWORD
WINFS::GetFileAttributes (const path &p)
{
  DWORD attr = get_file_attributes_ask (p);
  if (attr == DWORD (-1) && GetLastError () != ERROR_INVALID_NAME)
    {
      find_data fd;
      if (get_file_data (p, fd))
        attr = fd.dwFileAttributes;
    }
  return attr;
}

DWORD
WINFS::GetFullPathName (const char *path, DWORD size, char *buf, char **file_part)
{
  int off = -1;
  DWORD l = wfile::get_full_path_name (path, buf, size, &off);
  if (file_part)
    *file_part = off < 0 ? 0 : buf + off;
  return l;
}

UINT
WINFS::GetTempFileName (const char *dir, const char *prefix, UINT unique, char *out)
{
  wfile::path p (dir, wfile::path::NOPREFIX);
  WINFS_CALL1 (UINT, 0, p, wfile::get_temp_file_name (dir, prefix, unique, out));
}

BOOL
WINFS::GetVolumeInformation (const char *root, char *volname, DWORD volsize,
                             DWORD *serial, DWORD *maxl, DWORD *flags,
                             char *fsname, DWORD fssize)
{
  wfile::path p (root, wfile::path::NOPREFIX);
  WINFS_CALL1 (BOOL, FALSE, p,
               wfile::get_volume_information (root, volname, volsize, serial, maxl,
                                              flags, fsname, fssize));
}

HMODULE
WINFS::LoadLibrary (const path &p)
{
  WINFS_CALL1 (HMODULE, NULL, p, wfile::load_library (p));
}

static BOOL
move_file_ask (const wfile::path &from, const wfile::path &to)
{
  WINFS_CALL2 (BOOL, FALSE, from, to, wfile::move_file (from, to));
}

BOOL
WINFS::MoveFile (const path &from, const path &to)
{
  for (int retry = 0;; retry++)
    {
      if (move_file_ask (from, to))
        return 1;
      if (retry >= 3)
        return 0;
      Sleep (50);
    }
}

BOOL
WINFS::RemoveDirectory (const path &p)
{
  WINFS_CALL1 (BOOL, FALSE, p, wfile::remove_directory (p));
}

BOOL
WINFS::SetFileAttributes (const path &p, DWORD dwFileAttributes)
{
  WINFS_CALL1 (BOOL, FALSE, p, wfile::set_file_attributes (p, dwFileAttributes));
}

BOOL
WINFS::SetCurrentDirectory (const char *s)
{
  wfile::path p (s, wfile::path::NOPREFIX);
  WINFS_CALL1 (BOOL, FALSE, p, wfile::set_current_directory (s));
}

DWORD
WINFS::WNetOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage,
                     LPNETRESOURCE lpNetResource, LPHANDLE lphEnum)
{
  if (!lpNetResource)
    return ::WNetOpenEnum (dwScope, dwType, dwUsage, lpNetResource, lphEnum);

  DWORD r = ::WNetOpenEnum (dwScope, dwType, dwUsage, lpNetResource, lphEnum);
  if (r != NO_ERROR
      && lpNetResource->lpRemoteName
      && askpass_noshare (wfile::path (lpNetResource->lpRemoteName,
                                       wfile::path::NOPREFIX)))
    r = ::WNetOpenEnum (dwScope, dwType, dwUsage, lpNetResource, lphEnum);
  return r;
}

int
WINFS::get_file_data (const path &p, find_data &fd)
{
  HANDLE h = FindFirstFile (p, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  FindClose (h);
  return 1;
}
