// -*-C++-*-
// W 版のファイル API の薄い層。
//
// xyzzy のソースがファイルに触る Win32 API は、すべてこの層（と、その上で
// ネットワークの資格情報を聞き直す vfs.cc の WINFS）を通る。内部の文字列と
// UTF-16 の変換は、この層の 4 つの関数（narrow_to_wide / chars_to_wide /
// wide_to_narrow / wide_to_chars）だけが行う。Char を Unicode のコードポイント
// に広げるときは、この 4 つを替えればよい。
//
// 長いパス: 絶対パスに直した結果が長いとき（MAX_PATH に近いとき）、層の中で
// \\?\ （UNC は \\?\UNC\）を付ける。呼び出し側は付けない。
#ifndef _wfile_h_
# define _wfile_h_

# include "cdecl.h"
# include <share.h>

namespace wfile
{
  // 1 つのファイル名（cp932 の narrow）の最大のバイト数。255 UTF-16 単位は最大 510 バイト。
  const int FIND_NAME_MAX = MAX_PATH * 2 + 2;

  // 変換後の UTF-16 の最大の長さ（wchar_t の数）。内部のパスは PATH_MAX を超えない。
  const int WPATH_BUF = PATH_MAX * 2 + 16;

  // \\?\ を付ける閾値。CreateDirectory は MAX_PATH - 12 (248) 文字までなので、
  // それより少し手前から付ける。
  const int LONG_PATH_THRESHOLD = 240;

  // ---- 変換（内部の文字列 <-> UTF-16）。ここだけが符号の変換を知っている ----
  // narrow: プログラムの narrow 文字列（今は cp932）。Char: 内部の Char の列。
  // 戻り値は終端の NUL を除く長さ。入りきらないとき -1。len < 0 は NUL 終端。
  int narrow_to_wide (wchar_t *w, int wsize, const char *s, int len = -1);
  int chars_to_wide (wchar_t *w, int wsize, const Char *s, int len = -1);
  int wide_to_narrow (char *s, int size, const wchar_t *w, int wlen = -1);
  int wide_to_chars (Char *s, int size, const wchar_t *w, int wlen = -1);

  // ---- パス ----
  // 呼び出し側の文字列（char / Char）から作る。変換し、必要なら \\?\ を付ける。
  class path
  {
  public:
    enum prefix_t {PREFIX, NOPREFIX};
  private:
    wchar_t m_buf[WPATH_BUF];
    int m_len;
    int m_ok;
    void build (const wchar_t *, int, prefix_t);
  public:
    path (const char *, prefix_t = PREFIX);
    path (const Char *, prefix_t = PREFIX);
    // len 文字だけ（NUL 終端でない Char の列）
    path (const Char *, int len, prefix_t);
    const wchar_t *c_str () const {return m_buf;}
    operator const wchar_t * () const {return m_buf;}
    int length () const {return m_len;}
    // 変換と組み立てに成功したか。失敗なら API を呼ばず ERROR_FILENAME_EXCED_RANGE
    int ok () const {return m_ok;}
    // \\server\share （\\?\UNC\server\share も）の部分を out に取り出す。
    // noshare_ok のとき \\server だけでもよい
    int share_root (wchar_t *out, int size, int noshare_ok) const;
  };

  // ---- ディレクトリ列挙・属性 ----
  // WIN32_FIND_DATAA の代わり。名前は narrow（cp932）。8.3 の名前は持たない。
  // サイズは 64bit。
  struct find_data
  {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    // 255 UTF-16 単位は cp932 で最大 510 バイト
    char cFileName[FIND_NAME_MAX];
    int64_t size () const
      {return int64_t (uint64_t (nFileSizeHigh) << 32 | nFileSizeLow);}
  };

  HANDLE find_first_file (const path &, find_data &);
  BOOL find_next_file (HANDLE, find_data &);

  // ---- 素の W API の薄い包み（ネットワークの資格情報は聞かない） ----
  HANDLE create_file (const path &, DWORD access, DWORD share,
                      LPSECURITY_ATTRIBUTES, DWORD disposition,
                      DWORD flags, HANDLE tmpl);
  BOOL delete_file (const path &);
  BOOL move_file (const path &, const path &);
  BOOL copy_file (const path &, const path &, BOOL fail_if_exists);
  BOOL create_directory (const path &);
  BOOL remove_directory (const path &);
  DWORD get_file_attributes (const path &);
  BOOL set_file_attributes (const path &, DWORD);
  // GetFileSizeEx
  BOOL get_file_size (HANDLE, int64_t *);
  // 成功したら FILE*。モードは ASCII。shflag は _SH_DENYNO など
  FILE *fopen (const path &, const char *mode, int shflag = _SH_DENYNO);
  HMODULE load_library (const path &);

  // カレントディレクトリ（\\?\ は付けない）
  BOOL set_current_directory (const char *);
  DWORD get_current_directory (char *, DWORD size);
  DWORD get_current_directory (Char *, DWORD size);

  // 戻りは narrow / Char。file_part は結果の中の最後の要素へのオフセット
  // （無ければ -1）。長い結果も扱える。
  DWORD get_full_path_name (const char *, char *, DWORD size, int *file_part);
  DWORD get_full_path_name (const Char *, Char *, DWORD size, int *file_part);

  DWORD get_temp_path (char *, DWORD size);
  DWORD get_module_file_name (HMODULE, char *, DWORD size);
  UINT get_windows_directory (char *, UINT size);
  UINT get_system_directory (char *, UINT size);
  UINT get_temp_file_name (const char *dir, const char *prefix, UINT unique, char *out);
  UINT get_drive_type (const char *root);
  BOOL get_volume_information (const char *root, char *volname, DWORD volsize,
                               DWORD *serial, DWORD *maxl, DWORD *flags,
                               char *fsname, DWORD fssize);
  // root が 0 のときはカレントドライブ
  BOOL get_disk_free_space_ex (const char *root, uint64_t *avail,
                               uint64_t *total, uint64_t *free_total);
  BOOL get_disk_free_space (const char *root, DWORD *sectors_per_cluster,
                            DWORD *bytes_per_sector, DWORD *free_clusters,
                            DWORD *total_clusters);
  DWORD search_path (const char *dir, const char *file, const char *ext,
                     char *buf, DWORD size, int *file_part);
}

#endif /* _wfile_h_ */
