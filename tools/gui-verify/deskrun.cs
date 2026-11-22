using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

// deskrun.exe <timeout-seconds> <command line...>
// Creates a private desktop (not the user's Default desktop), runs the command line on it
// (CreateProcess with STARTUPINFO.lpDesktop), waits, then reports whether the user's
// foreground window changed and whether any window of the child tree leaked onto Default.
// Uses no real input API and no foreground-stealing API.
class DeskRun {
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr CreateDesktop(string name, IntPtr dev, IntPtr dm, int flags, uint access, IntPtr sa);
  [DllImport("user32.dll", SetLastError = true)] static extern bool CloseDesktop(IntPtr h);
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr OpenInputDesktop(int flags, bool inherit, uint access);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr desk, EnumProc p, IntPtr l);
  [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr OpenDesktop(string name, int flags, bool inherit, uint access);
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
  static extern bool CreateProcess(string app, StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr h, uint code);
  [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  struct STARTUPINFO { public int cb; public string lpReserved, lpDesktop, lpTitle; public int x, y, xs, ys, xc, yc, fill, flags; public short show, res2; public IntPtr res2p, hin, hout, herr; }
  [StructLayout(LayoutKind.Sequential)]
  struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int pid, tid; }
  const uint GENERIC_ALL = 0x10000000; // DESKTOP_* all access is requested with GENERIC_ALL below

  static string Fg() {
    IntPtr h = GetForegroundWindow();
    var sb = new StringBuilder(256); GetWindowText(h, sb, 256);
    uint pid; GetWindowThreadProcessId(h, out pid);
    return h.ToInt64() + " pid=" + pid + " [" + sb + "]";
  }

  static int Main(string[] a) {
    int timeout = int.Parse(a[0]);
    string cmd = string.Join(" ", a, 1, a.Length - 1);
    string name = "xyzzy-verify-" + Process.GetCurrentProcess().Id;
    string fgBefore = Fg();
    IntPtr desk = CreateDesktop(name, IntPtr.Zero, IntPtr.Zero, 0, 0x01FF, IntPtr.Zero); // DESKTOP_ALL access (0x1FF)
    if (desk == IntPtr.Zero) { Console.WriteLine("CreateDesktop failed " + Marshal.GetLastWin32Error()); return 90; }
    Console.WriteLine("desktop created: " + name);
    var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si); si.lpDesktop = name;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(null, new StringBuilder(cmd), IntPtr.Zero, IntPtr.Zero, false, 0x08000000 /*CREATE_NO_WINDOW*/, IntPtr.Zero, null, ref si, out pi)) {
      Console.WriteLine("CreateProcess failed " + Marshal.GetLastWin32Error()); CloseDesktop(desk); return 91;
    }
    Console.WriteLine("child pid=" + pi.pid);
    // watch the Default (input) desktop for the whole run: any window from a process that is not
    // in the user's own session set at start would be a leak; we record windows owned by child tree pids
    IntPtr def = OpenDesktop("Default", 0, false, 0x0100 /*DESKTOP_ENUMERATE*/);
    var leaks = new HashSet<string>();
    var fgStolen = new HashSet<string>();
    var fgSeen = new List<string>();
    var sw = Stopwatch.StartNew();
    bool done = false;
    while (sw.ElapsedMilliseconds < timeout * 1000L) {
      if (WaitForSingleObject(pi.hProcess, 250) == 0) { done = true; break; }
      var kids = Descendants(pi.pid);
      { uint fpid; IntPtr fh = GetForegroundWindow(); GetWindowThreadProcessId(fh, out fpid);
        if (kids.Contains((int)fpid)) fgStolen.Add(fpid + ":" + fh.ToInt64());
        string fs = Fg(); if (fgSeen.Count == 0 || fgSeen[fgSeen.Count - 1] != fs) fgSeen.Add(fs); }
      EnumDesktopWindows(def, (h, l) => {
        uint pid; GetWindowThreadProcessId(h, out pid);
        if (kids.Contains((int)pid) && IsWindowVisible(h)) { var sb = new StringBuilder(256); GetWindowText(h, sb, 256); leaks.Add(pid + ":" + h.ToInt64() + ":" + sb); }
        return true;
      }, IntPtr.Zero);
    }
    uint code = 0;
    if (!done) { Console.WriteLine("TIMEOUT: terminating child"); TerminateProcess(pi.hProcess, 99); }
    GetExitCodeProcess(pi.hProcess, out code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    // kill leftovers of the child tree that are still on our private desktop (only processes started by it)
    string fgAfter = Fg();
    CloseDesktop(desk);
    Console.WriteLine("exit code: " + code);
    Console.WriteLine("foreground before: " + fgBefore);
    Console.WriteLine("foreground after : " + fgAfter);
    Console.WriteLine("foreground unchanged: " + (fgBefore == fgAfter) + " (it can change by the user's own doing; the check that matters is the next line)");
    Console.WriteLine("foreground window ever owned by the child tree: " + (fgStolen.Count > 0));
    foreach (var s in fgSeen) Console.WriteLine("  fg seen: " + s);
    Console.WriteLine("windows of the child tree seen on Default desktop: " + leaks.Count);
    foreach (var l in leaks) Console.WriteLine("  LEAK " + l);
    return (int)code;
  }

  static HashSet<int> Descendants(int root) {
    var set = new HashSet<int>(); set.Add(root);
    var searcher = new System.Management.ManagementObjectSearcher("SELECT ProcessId, ParentProcessId FROM Win32_Process");
    var parent = new Dictionary<int, int>();
    foreach (System.Management.ManagementObject o in searcher.Get()) parent[Convert.ToInt32(o["ProcessId"])] = Convert.ToInt32(o["ParentProcessId"]);
    bool grew = true;
    while (grew) { grew = false; foreach (var kv in parent) if (!set.Contains(kv.Key) && set.Contains(kv.Value)) { set.Add(kv.Key); grew = true; } }
    return set;
  }
}
