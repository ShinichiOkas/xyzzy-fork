#include "stdafx.h"
#include "ed.h"
#include "com.h"
#include "sysdep.h"

static void
set_desc (IShellLink *sl, lisp ldesc)
{
  char *b = (char *)alloca (xstring_length (ldesc) * 2 + 1);
  w2s (b, ldesc);
  ole_error (sl->SetDescription (b));
}

static void
set_args (IShellLink *sl, lisp largs)
{
  char *b = (char *)alloca (xstring_length (largs) * 2 + 1);
  w2s (b, largs);
  ole_error (sl->SetArguments (b));
}

static void
set_appid (IShellLink *sl, lisp lappid)
{
  char *b = (char *)alloca (xstring_length (lappid) * 2 + 1);
  w2s (b, lappid);

  int l = (strlen (b) + 1);
  wchar_t *w = (wchar_t *)alloca (l * sizeof (wchar_t));
  wfile::narrow_to_wide (w, l, b);

  safe_com <IPropertyStore> store;
  ole_error (sl->QueryInterface (IID_PPV_ARGS(&store)));

  PROPVARIANT pv;
  PropVariantClear (&pv);

  pv.vt = VT_LPWSTR;
  pv.pwszVal = w;

  ole_error (store->SetValue (PKEY_AppUserModel_ID, pv));
  ole_error (store->Commit ());
}

lisp
Fcreate_shortcut (lisp lobject, lisp llink, lisp keys)
{
  check_string (lobject);
  check_string (llink);
  lisp ldesc = find_keyword (Kdescription, keys, 0);
  if (ldesc)
    check_string (ldesc);
  lisp largs = find_keyword (Karguments, keys, 0);
  if (largs)
    check_string (largs);
  lisp lworkdir = find_keyword (Kworking_directory, keys, 0);
  if (lworkdir)
    check_string (lworkdir);
  int show;
  lisp lshow = find_keyword (Kshow, keys, 0);
  if (lshow == Kshow)
    show = SW_SHOWNORMAL;
  else if (lshow == Kmaximize)
    show = SW_SHOWMAXIMIZED;
  else if (lshow == Kminimize)
    show = SW_SHOWMINIMIZED;
  else
    show = SW_SHOWNORMAL;
  lisp lappid = find_keyword (Kappid, keys, 0);
  if (lappid)
    check_string (lappid);

  safe_com <IShellLink> sl;
  ole_error (CoCreateInstance (CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER,
                               IID_IShellLink, (void **)&sl));
  char path[PATH_MAX + 1];
  pathname2cstr (lobject, path);

  if (Ffile_directory_p (lobject))
    {
      safe_com <IMalloc> ialloc;
      ole_error (SHGetMalloc (&ialloc));

      safe_com <IShellFolder> sf;
      ole_error (SHGetDesktopFolder (&sf));

      map_sl_to_backsl (path);
      int l = (strlen (path) + 1);
      wchar_t *w = (wchar_t *)alloca (l * sizeof (wchar_t));
      wfile::narrow_to_wide (w, l, path);

      ULONG ul;
      safe_idl idl (ialloc);
      ole_error (sf->ParseDisplayName (get_active_window (), 0, w, &ul, &idl, 0));
      ole_error (sl->SetIDList (idl));
    }
  else
    ole_error (sl->SetPath (path));

  if (ldesc)
    set_desc (sl, ldesc);
  if (largs)
    set_args (sl, largs);
  if (lworkdir)
    {
      pathname2cstr (lworkdir, path);
      ole_error (sl->SetWorkingDirectory (path));
    }
  if (lshow)
    ole_error (sl->SetShowCmd (show));
  if (lappid)
    set_appid (sl, lappid);

  safe_com <IPersistFile> pf;
  ole_error (sl->QueryInterface (IID_IPersistFile, (void **)&pf));

  pathname2cstr (llink, path);
  int l = (strlen (path) + 1);
  wchar_t *w = (wchar_t *)alloca (l * sizeof (wchar_t));
  wfile::narrow_to_wide (w, l, path);
  ole_error (pf->Save (w, 1));

  return Qt;
}

lisp
Fget_special_folder_location (lisp place)
{
  int f = 0;
  if (place == Kdesktop)
    f = CSIDL_DESKTOPDIRECTORY;
  else if (place == Knetwork)
    f = CSIDL_NETHOOD;
  else if (place == Kpersonal)
    f = CSIDL_PERSONAL;
  else if (place == Kprograms)
    f = CSIDL_PROGRAMS;
  else if (place == Krecent)
    f = CSIDL_RECENT;
  else if (place == Ksend_to)
    f = CSIDL_SENDTO;
  else if (place == Kstart_menu)
    f = CSIDL_STARTMENU;
  else if (place == Kstartup)
    f = CSIDL_STARTUP;
  else if (place == Ktemplates)
    f = CSIDL_TEMPLATES;
  else
    FEprogram_error (Eunknown_folder_name, place);

  safe_com <IMalloc> ialloc;
  ole_error (SHGetMalloc (&ialloc));

  safe_idl idl (ialloc);
  ole_error (SHGetSpecialFolderLocation (get_active_window (), f, &idl));

  safe_com <IShellFolder> sf;
  ole_error (SHGetDesktopFolder (&sf));

  STRRET name;
  ole_error (sf->GetDisplayNameOf (idl, SHGDN_FORPARSING, &name), place);

  switch (name.uType)
    {
    default:
      FEprogram_error (Eunknown_STRRET_type, make_fixnum (name.uType));

    case STRRET_WSTR:
      {
        int l = int (wcslen (name.pOleStr)) * 2 + 2;
        char *b = (char *)alloca (l);
        wfile::wide_to_narrow (b, l, name.pOleStr);
        ialloc->Free (name.pOleStr);
        return make_string (b);
      }

    case STRRET_OFFSET:
      return make_string ((char *)(ITEMIDLIST *)idl + name.uOffset);

    case STRRET_CSTR:
      return make_string (name.cStr);
    }
}

lisp
Fresolve_shortcut (lisp lshortcut)
{
  char shortcut[PATH_MAX + 1];
  pathname2cstr (lshortcut, shortcut);
  map_sl_to_backsl (shortcut);
  int l = (strlen (shortcut) + 1);
  wchar_t *w = (wchar_t *)alloca (l * sizeof (wchar_t));
  wfile::narrow_to_wide (w, l, shortcut);

  safe_com <IShellLink> sl;
  ole_error (CoCreateInstance (CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER,
                               IID_IShellLink, (void **)&sl));

  safe_com <IPersistFile> pf;
  ole_error (sl->QueryInterface (IID_IPersistFile, (void **)&pf));
  HRESULT hr = pf->Load (w, STGM_READ);
  if (hr == E_FAIL)
    FEfile_error (Enot_a_shortcut, lshortcut);
  ole_error (hr);

  WIN32_FIND_DATA fd;
  char path[PATH_MAX], desc[PATH_MAX];
  ole_error (sl->GetPath (path, sizeof path, &fd, 0));
  if (!*path)
    FEfile_error (Enot_a_shortcut, lshortcut);
  ole_error (sl->GetDescription (desc, sizeof desc));

  map_backsl_to_sl (path);
  multiple_value::count () = 2;
  multiple_value::value (1) = make_string (desc);
  return make_string (path);
}
