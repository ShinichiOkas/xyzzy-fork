#ifndef _vfs_h_
#define _vfs_h_

#include "wfile.h"

// wfile（W 版 API の層）の上に、ネットワークの共有で拒否されたときに資格情報を
// 聞いて再試行する処理を足したもの。引数のパスは wfile::path で、char（narrow）
// からも Char からも暗黙に作られる。
class WINFS
{
public:
  typedef wfile::path path;
  typedef wfile::find_data find_data;

  static wchar_t wfs_share_cache[MAX_PATH * 2];

  static void clear_share_cache () {*wfs_share_cache = 0;}

  static BOOL CreateDirectory (const path &);
  static HANDLE CreateFile (const path &, DWORD dwDesiredAccess, DWORD dwShareMode,
                            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                            DWORD dwCreationDisposition,
                            DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
  static BOOL CopyFile (const path &from, const path &to, BOOL fail_if_exists);
  static BOOL DeleteFile (const path &);
  static HANDLE FindFirstFile (const path &, find_data *);
  static BOOL FindNextFile (HANDLE, find_data *);
  // root が 0 のときはカレントドライブ
  static BOOL GetDiskFreeSpace (const char *root, LPDWORD lpSectorsPerCluster,
                                LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                LPDWORD lpTotalNumberOfClusters);
  static DWORD GetFileAttributes (const path &);
  // buf は narrow。*file_part は buf の中の最後の要素（無ければ 0）
  static DWORD GetFullPathName (const char *, DWORD size, char *buf, char **file_part);
  static UINT GetTempFileName (const char *dir, const char *prefix, UINT unique,
                               char *out);
  static BOOL GetVolumeInformation (const char *root, char *volname, DWORD volsize,
                                    DWORD *serial, DWORD *maxl, DWORD *flags,
                                    char *fsname, DWORD fssize);
  static HMODULE LoadLibrary (const path &);
  static BOOL MoveFile (const path &from, const path &to);
  static BOOL RemoveDirectory (const path &);
  static BOOL SetCurrentDirectory (const char *);
  static BOOL SetFileAttributes (const path &, DWORD dwFileAttributes);
  // ネットワークの列挙は narrow の NETRESOURCE のまま（ファイルの API ではない）
  static DWORD WNetOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage,
                             LPNETRESOURCE lpNetResource, LPHANDLE lphEnum);

  static int get_file_data (const path &, find_data &);
};

#endif /* _vfs_h_ */
