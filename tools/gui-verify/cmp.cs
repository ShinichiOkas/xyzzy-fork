using System; using System.IO; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices; using System.Text;
class Cmp {
  static byte[] Load(string path, out int w, out int h, out int stride) {
    using (var bmp = new Bitmap(path)) {
      w = bmp.Width; h = bmp.Height;
      var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
      stride = d.Stride; byte[] b = new byte[stride * h]; Marshal.Copy(d.Scan0, b, 0, b.Length); bmp.UnlockBits(d); return b;
    }
  }
  static int[][] Masks(params string[] pngs) { var l = new System.Collections.Generic.List<int[]>(); foreach (var p in pngs) { string m = Path.ChangeExtension(p, ".mask"); if (File.Exists(m)) { var v = File.ReadAllText(m).Trim().Split(new char[]{' '}); l.Add(new int[]{int.Parse(v[0]) - 3, int.Parse(v[1]) - 3, int.Parse(v[2]) + 3, int.Parse(v[3]) + 3}); } } return l.ToArray(); }
  static bool Masked(int[][] ms, int xx, int yy) { foreach (var m in ms) if (xx >= m[0] && xx < m[2] && yy >= m[1] && yy < m[3]) return true; return false; }
  static string Diff(string a, string b, int[] bands, int[][] extra = null) {
    int[][] ms = extra != null ? extra : Masks(a, b);
    int w1, h1, s1, w2, h2, s2;
    byte[] x = Load(a, out w1, out h1, out s1), y = Load(b, out w2, out h2, out s2);
    if (w1 != w2 || h1 != h2) return "SIZE DIFFERS " + w1 + "x" + h1 + " vs " + w2 + "x" + h2;
    var sb = new StringBuilder(); long total = 0;
    for (int k = 0; k + 1 < bands.Length; k++) {
      int y0 = bands[k], y1 = Math.Min(bands[k + 1], h1); long n = 0; int minx = int.MaxValue, maxx = -1, miny = int.MaxValue, maxy = -1;
      for (int yy = y0; yy < y1; yy++) for (int xx = 0; xx < w1; xx++) {
        int o = yy * s1 + xx * 4;
        if ((x[o] != y[o] || x[o + 1] != y[o + 1] || x[o + 2] != y[o + 2]) && !Masked(ms, xx, yy)) { n++; if (xx < minx) minx = xx; if (xx > maxx) maxx = xx; if (yy < miny) miny = yy; if (yy > maxy) maxy = yy; }
      }
      total += n; sb.Append("[" + y0 + "-" + y1 + ": " + n + (n > 0 ? " bbox=(" + minx + "," + miny + ")-(" + maxx + "," + maxy + ")" : "") + "] ");
    }
    return "total=" + total + " " + sb;
  }
  // cmp.exe <dir> <tagA> <tagB> [bands...]   or  cmp.exe -f fileA fileB
  static int Main(string[] a) {
    int[] bands = new int[] { 0, 89, 1433, 1464, 1490, 1537 };
    if (a[0] == "-n") { int w, h, s; byte[] x = Load(a[1], out w, out h, out s); long n = 0; for (int yy = 91; yy < 1430 && yy < h; yy++) for (int xx = 62; xx < 1960 && xx < w; xx++) { int o = yy * s + xx * 4; if (x[o] != 255 || x[o+1] != 255 || x[o+2] != 255) n++; } Console.WriteLine(n); return 0; }
    if (a[0] == "-fm") { Console.WriteLine(Diff(a[5], a[6], new int[] { 0, 100000 }, new int[][] { new int[] { int.Parse(a[1]) - 3, int.Parse(a[2]) - 3, int.Parse(a[3]) + 3, int.Parse(a[4]) + 3 } })); return 0; }
    if (a[0] == "-f") { Console.WriteLine(Diff(a[1], a[2], new int[] { 0, 100000 })); return 0; }
    string dir = a[0], A = a[1], B = a[2];
    foreach (var f in Directory.GetFiles(dir, A + "-*.png")) {
      string name = Path.GetFileName(f).Substring(A.Length + 1);
      string g = Path.Combine(dir, B + "-" + name);
      if (!File.Exists(g)) { Console.WriteLine(name + " : missing in " + B); continue; }
      Console.WriteLine(name.Replace(".png", "") + " : " + Diff(f, g, bands));
    }
    return 0;
  }
}