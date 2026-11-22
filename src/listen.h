#ifndef _listen_h_
#define _listen_h_

// v3: data is UTF-8 (v2 was the ACP). A xyzzycli.exe and a xyzzy.exe of
// different versions do not find each other.
static const char xyzzysrv_name[] = "xyzzy-server v3";

// wm_private_xyzzysrv
//  wparam: process ID
//  lparam: handle

struct xyzzysrv_param
{
  int size;
  DWORD pid;
  HANDLE hevent;
  HWND hwnd;
  int kill_ok;
  char data[1];                 // UTF-8, ends with NUL
};

#endif
