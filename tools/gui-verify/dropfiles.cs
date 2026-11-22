using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

// dropfiles.exe <hwnd-decimal> <client-x> <client-y> <file>...
// Delivers WM_DROPFILES to a window of OUR OWN xyzzy (same private desktop) with an HDROP that is valid
// in the target process: a DROPFILES (wide) block is placed in memory that GlobalAlloc'ed inside the target
// (a small CreateRemoteThread stub calls GlobalAlloc there). xyzzy then reads it with DragQueryFileW / DragFinish
// exactly as for a drop from Explorer. No real input, no foreground API.
class DropFiles {
  [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a, bool i, uint pid);
  [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr s, uint t, uint pr);
  [DllImport("kernel32.dll")] static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out IntPtr w);
  [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr p, IntPtr a, byte[] b, int n, out IntPtr r);
  [DllImport("kernel32.dll")] static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr sa, UIntPtr ss, IntPtr start, IntPtr param, uint fl, out uint tid);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] static extern IntPtr GetModuleHandle(string n);
  [DllImport("kernel32.dll")] static extern IntPtr GetProcAddress(IntPtr m, string n);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll", SetLastError = true)] static extern IntPtr SendMessageTimeout(IntPtr h, uint m, IntPtr w, IntPtr l, uint fl, uint to, out UIntPtr res);
  [DllImport("user32.dll", SetLastError = true)] static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);

  static int Main(string[] a) {
    IntPtr hwnd = new IntPtr(long.Parse(a[0]));
    int px = int.Parse(a[1]), py = int.Parse(a[2]);
    uint pid; GetWindowThreadProcessId(hwnd, out pid);
    // DROPFILES: DWORD pFiles; POINT pt; BOOL fNC; BOOL fWide;
    var ms = new System.IO.MemoryStream();
    var bw = new System.IO.BinaryWriter(ms);
    bw.Write(20); bw.Write(px); bw.Write(py); bw.Write(0); bw.Write(1);
    for (int i = 3; i < a.Length; i++) { bw.Write(Encoding.Unicode.GetBytes(a[i])); bw.Write((short)0); }
    bw.Write((short)0);
    byte[] blob = ms.ToArray();
    IntPtr hp = OpenProcess(0x1F0FFF, false, pid);
    if (hp == IntPtr.Zero) { Console.WriteLine("OpenProcess failed"); return 1; }
    IntPtr mem = VirtualAllocEx(hp, IntPtr.Zero, (UIntPtr)4096, 0x3000, 0x40);
    IntPtr ga = GetProcAddress(GetModuleHandle("kernel32.dll"), "GlobalAlloc");
    IntPtr slot = new IntPtr(mem.ToInt64() + 512);
    var st = new List<byte>();
    st.AddRange(new byte[] { 0x48, 0x83, 0xEC, 0x28 });                    // sub rsp,28h
    st.AddRange(new byte[] { 0x31, 0xC9 });                                // xor ecx,ecx   (GMEM_FIXED)
    st.AddRange(new byte[] { 0x48, 0xBA }); st.AddRange(BitConverter.GetBytes((long)blob.Length));   // mov rdx,size
    st.AddRange(new byte[] { 0x48, 0xB8 }); st.AddRange(BitConverter.GetBytes(ga.ToInt64()));        // mov rax,GlobalAlloc
    st.AddRange(new byte[] { 0xFF, 0xD0 });                                // call rax
    st.AddRange(new byte[] { 0x48, 0xB9 }); st.AddRange(BitConverter.GetBytes(slot.ToInt64()));      // mov rcx,slot
    st.AddRange(new byte[] { 0x48, 0x89, 0x01 });                          // mov [rcx],rax
    st.AddRange(new byte[] { 0x48, 0x83, 0xC4, 0x28 });                    // add rsp,28h
    st.AddRange(new byte[] { 0x31, 0xC0, 0xC3 });                          // xor eax,eax ; ret
    IntPtr w;
    WriteProcessMemory(hp, mem, st.ToArray(), st.Count, out w);
    uint tid;
    IntPtr th = CreateRemoteThread(hp, IntPtr.Zero, UIntPtr.Zero, mem, IntPtr.Zero, 0, out tid);
    if (th == IntPtr.Zero) { Console.WriteLine("CreateRemoteThread failed " + Marshal.GetLastWin32Error()); return 2; }
    WaitForSingleObject(th, 5000); CloseHandle(th);
    byte[] pb = new byte[8]; IntPtr r;
    ReadProcessMemory(hp, slot, pb, 8, out r);
    long ptr = BitConverter.ToInt64(pb, 0);
    if (ptr == 0) { Console.WriteLine("remote GlobalAlloc returned NULL"); return 3; }
    WriteProcessMemory(hp, new IntPtr(ptr), blob, blob.Length, out w);
    Console.WriteLine("HDROP in target = 0x" + ptr.ToString("X") + " (" + blob.Length + " bytes, " + (a.Length - 3) + " files)");
    UIntPtr res;
    IntPtr rr = SendMessageTimeout(hwnd, 0x233 /*WM_DROPFILES*/, new IntPtr(ptr), IntPtr.Zero, 2, 15000, out res);
    Console.WriteLine("SendMessageTimeout WM_DROPFILES -> " + rr + " lasterror=" + Marshal.GetLastWin32Error() + " IsWindow=" + IsWindow(hwnd));
    CloseHandle(hp);
    return 0;
  }
}
