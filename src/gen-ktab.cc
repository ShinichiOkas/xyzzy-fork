#include "gen-stdafx.h"

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;

#if 0
static const u_char h_20_7e[] = (" !\"#$%&'()*+,-./"
                                 "0123456789:;<=>?"
                                 "@ABCDEFGHIJKLMNO"
                                 "PQRSTUVWXYZ[\\]^_"
                                 "`abcdefghijklmno"
                                 "pqrstuvwxyz{|}~");

//                                0123456789abcdef
static const u_char h_a1_df[] = ( "｡｢｣､･ｦｧｨｩｪｫｬｭｮｯ"   // A
                                 "ｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿ"   // B
                                 "ﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏ"   // C
                                 "ﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟ"); // D
#endif

static const u_char f_20_7e[] = ("　！”＃＄％＆’（）＊＋，－．／"
                                 "０１２３４５６７８９：；＜＝＞？"
                                 "＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯ"
                                 "ＰＱＲＳＴＵＶＷＸＹＺ［￥］＾＿"
                                 "‘ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏ"
                                 "ｐｑｒｓｔｕｖｗｘｙｚ｛｜｝￣");

static const u_char fk_a1_df[] = ("。「」、・ヲァィゥェォャュョッ"
                                  "ーアイウエオカキクケコサシスセソ"
                                  "タチツテトナニヌネノハヒフヘホマ"
                                  "ミムメモヤユヨラリルレロワン゛゜");

static const u_char fh_a1_df[] = ("。「」、・をぁぃぅぇぉゃゅょっ"
                                  "ーあいうえおかきくけこさしすせそ"
                                  "たちつてとなにぬねのはひふへほま"
                                  "みむめもやゆよらりるれろわん゛゜");

static const u_char fh_voiced_b6_ce[] = ("がぎぐげござじずぜぞだぢづでど"
					 "          ばびぶべぼ");

static const u_char fk_voiced_b3_ce[] = ("ヴ    ガギグゲゴザジズゼゾ"
					 "ダヂヅデド          バビブベボ");

static const u_char fh_semi_voiced_ca_ce[] = "ぱぴぷぺぽ";

static const u_char fk_semi_voiced_ca_ce[] = "パピプペポ";

#define F(T, N) (u_short (((T) [(N) * 2] << 8) + (T) [(N) * 2 + 1]))

static void gen_unicode_tables ();

static void
soutput (u_char *buf, int n)
{
  int min, max;
  int i, j;

  for (min = 0; min < 256 && !buf[min]; min++)
    ;
  for (max = 255; max > min && !buf[max]; max--)
    ;
  if (min == max)
    return;
  printf ("#define VOICED_SOUND%x_MIN 0x%04x\n", n, (n << 8) + min);
  printf ("#define VOICED_SOUND%x_MAX 0x%04x\n", n, (n << 8) + max);
  printf ("static const u_char voiced_sound_%x[] = {\n", n);
  for (i = min, j = 0; i <= max; i++, j++)
    {
      printf ("0x%02x,", buf[i]);
      if ((j & 7) == 7)
        printf ("\n");
    }
  printf ("};\n\n");
}

static void
make_ss_table (void)
{
  u_char buf[256];
  int i;

  memset (buf, 0, sizeof buf);
  for (i = 0xb6; i <= 0xce; i++)
    {
      if (F (fh_voiced_b6_ce, i - 0xb6) >= 0x8200
          && F (fh_voiced_b6_ce, i - 0xb6) < 0x8300)
        buf[F (fh_voiced_b6_ce, i - 0xb6) - 0x8200] = i;
    }
  for (i = 0xca; i <= 0xce; i++)
    buf[F (fh_semi_voiced_ca_ce, i - 0xca) - 0x8200] = i & 0x7f;
  soutput (buf, 0x82);

  memset (buf, 0, sizeof buf);
  for (i = 0xb3; i <= 0xce; i++)
    {
      if (F (fk_voiced_b3_ce, i - 0xb3) >= 0x8300
          && F (fk_voiced_b3_ce, i - 0xb3) < 0x8400)
        buf[F (fk_voiced_b3_ce, i - 0xb3) - 0x8300] = i;
    }
  for (i = 0xca; i <= 0xce; i++)
    buf[F (fk_semi_voiced_ca_ce, i - 0xca) - 0x8300] = i & 0x7f;
  soutput (buf, 0x83);
}

static void
fill (u_short *buf, u_short from)
{
  u_short to = from + 0x100;
  int i;

  memset (buf, 0, sizeof *buf * 256);
  for (i = 0x20; i <= 0x7e; i++)
    {
      if (F (f_20_7e, i - 0x20) >= from
          && F (f_20_7e, i - 0x20) < to)
        buf[F (f_20_7e, i - 0x20) - from] = i;
    }
  for (i = 0xa1; i <= 0xdf; i++)
    {
      if (F (fk_a1_df, i - 0xa1) >= from
          && F (fk_a1_df, i - 0xa1) < to)
        buf[F (fk_a1_df, i - 0xa1) - from] = i;
    }
  for (i = 0xa1; i <= 0xdf; i++)
    {
      if (F (fh_a1_df, i - 0xa1) >= from
          && F (fh_a1_df, i - 0xa1) < to)
        buf[F (fh_a1_df, i - 0xa1) - from] = i;
    }
}

static void
output (u_short *buf, int n)
{
  int min, max;
  int i, j;

  for (min = 0; min < 256 && !buf[min]; min++)
    ;
  for (max = 255; max > min && !buf[max]; max--)
    ;
  if (min == max)
    return;
  printf ("#define TO_HALF_WIDTH%x_MIN 0x%04x\n", n, (n << 8) + min);
  printf ("#define TO_HALF_WIDTH%x_MAX 0x%04x\n", n, (n << 8) + max);
  printf ("static const u_char to_half_width_%x[] = {\n", n);
  for (i = min, j = 0; i <= max; i++, j++)
    {
      printf ("0x%02x,", buf[i]);
      if ((j & 7) == 7)
        printf ("\n");
    }
  printf ("};\n\n");
}

void
gen_ktab (int argc, char **argv)
{
  int i;
  u_short buf[256];

  printf ("static const Char to_full_20_7e[] = {\n");
  for (i = 0x20; i <= 0x7e; i++)
    {
      printf ("0x%04x,", F (f_20_7e, i - 0x20));
      if ((i & 7) == 7)
	printf ("\n");
    }
  printf ("};\n\n");

  printf ("static const Char to_fullkata_a1_df[] = {\n");
  for (i = 0xa1; i <= 0xdf; i++)
    {
      printf ("0x%04x,", F (fk_a1_df, i - 0xa1));
      if ((i & 7) == 7)
	printf ("\n");
    }
  printf ("};\n\n");

  printf ("static const Char to_fullhira_a1_df[] = {\n");
  for (i = 0xa1; i <= 0xdf; i++)
    {
      printf ("0x%04x,", F (fh_a1_df, i - 0xa1));
      if ((i & 7) == 7)
	printf ("\n");
    }
  printf ("};\n\n");

  printf ("static const Char to_fullhira_voiced_b6_ce[] = {\n");
  for (i = 0xb6; i <= 0xce; i++)
    {
      printf ("0x%04x,",
              (fh_voiced_b6_ce[(i - 0xb6) * 2] == ' '
               ? 0 : F (fh_voiced_b6_ce, i - 0xb6)));
      if ((i & 7) == 7)
	printf ("\n");
    }
  printf ("};\n\n");

  printf ("static const Char to_fullkata_voiced_b3_ce[] = {\n");
  for (i = 0xb3; i <= 0xce; i++)
    {
      printf ("0x%04x,",
              (fk_voiced_b3_ce[(i - 0xb3) * 2] == ' '
               ? 0 : F (fk_voiced_b3_ce, i - 0xb3)));
      if ((i & 7) == 7)
	printf ("\n");
    }
  printf ("};\n\n");

  printf ("static const Char to_fullhira_semi_voiced_ca_ce[] = {\n");
  for (i = 0xca; i <= 0xce; i++)
    printf ("0x%04x,", F (fh_semi_voiced_ca_ce, i - 0xca));
  printf ("};\n\n");

  printf ("static const Char to_fullkata_semi_voiced_ca_ce[] = {\n");
  for (i = 0xca; i <= 0xce; i++)
    printf ("0x%04x,", F (fk_semi_voiced_ca_ce, i - 0xca));
  printf ("};\n\n");

  printf ("#define VOICED_SOUND_MARK 0x%04x\n", F ("゛", 0));
  printf ("#define SEMI_VOICED_SOUND_MARK 0x%04x\n", F ("゜", 0));
  printf ("\n");

  for (i = 0; i < 3; i++)
    {
      fill (buf, u_short (0x8100 + i * 0x100));
      output (buf, 0x81 + i);
    }

  make_ss_table ();

  printf ("#define FULL_WIDTH_HIRAGANA_MIN 0x829f\n");
  printf ("#define FULL_WIDTH_HIRAGANA_MAX 0x82f1\n");
  printf ("#define FULL_WIDTH_KATAKANA_MIN 0x8340\n");
  printf ("#define FULL_WIDTH_KATAKANA_MAX 0x8396\n");

  gen_unicode_tables ();

  exit (0);
}


// ---------------------------------------------------------------------------
// The tables for XYZZY_CHAR_UNICODE = 1 (S4 R9).  They hold the same data as
// the tables above but are written in Unicode (the half width forms are
// U+FF61..U+FF9F), so that they do not depend on the code of cp932.
//
// kind is a set of the flags of map-to-*-width: 1 ASCII, 2 hiragana,
// 4 katakana.  The punctuation of kana (the full stop, the brackets, the
// middle dot, the prolonged sound mark and the voiced sound marks) is in both
// of the tables of kana, so its kind is 6.

#define K_ASC 1
#define K_HIRA 2
#define K_KATA 4

static const wchar_t w_f_20_7e[] = L"　！”＃＄％＆’（）＊＋，－．／"
                                   L"０１２３４５６７８９：；＜＝＞？"
                                   L"＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯ"
                                   L"ＰＱＲＳＴＵＶＷＸＹＺ［￥］＾＿"
                                   L"‘ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏ"
                                   L"ｐｑｒｓｔｕｖｗｘｙｚ｛｜｝￣";

static const wchar_t w_fk_a1_df[] = L"。「」、・ヲァィゥェォャュョッ"
                                    L"ーアイウエオカキクケコサシスセソ"
                                    L"タチツテトナニヌネノハヒフヘホマ"
                                    L"ミムメモヤユヨラリルレロワン゛゜";

static const wchar_t w_fh_a1_df[] = L"。「」、・をぁぃぅぇぉゃゅょっ"
                                    L"ーあいうえおかきくけこさしすせそ"
                                    L"たちつてとなにぬねのはひふへほま"
                                    L"みむめもやゆよらりるれろわん゛゜";

static const wchar_t w_fh_voiced_b6_ce[] = L"がぎぐげござじずぜぞだぢづでど"
                                           L"     ばびぶべぼ";

static const wchar_t w_fk_voiced_b3_ce[] = L"ヴ  ガギグゲゴザジズゼゾ"
                                           L"ダヂヅデド     バビブベボ";

static const wchar_t w_fh_semi_voiced_ca_ce[] = L"ぱぴぷぺぽ";

static const wchar_t w_fk_semi_voiced_ca_ce[] = L"パピプペポ";

struct u_entry
{
  unsigned short a, b, c;
  unsigned char kind;
};

static u_entry u_half[512];
static int u_nhalf;
static u_entry u_voiced[128];
static int u_nvoiced;
static u_entry u_combine[128];
static int u_ncombine;

static int
u_cmp (const void *x, const void *y)
{
  const u_entry *p = (const u_entry *)x;
  const u_entry *q = (const u_entry *)y;
  if (p->a != q->a)
    return p->a < q->a ? -1 : 1;
  if (p->b != q->b)
    return p->b < q->b ? -1 : 1;
  return 0;
}

static void
u_print_chars (const char *name, const wchar_t *t, int n)
{
  printf ("static const Char %s[] = {\n", name);
  for (int i = 0; i < n; i++)
    {
      printf ("0x%04x,", t[i]);
      if ((i & 7) == 7)
        printf ("\n");
    }
  printf ("};\n\n");
}

// A full width character -> the half width character, for a kind.
static void
u_add_half (unsigned short full, unsigned short half, int kind)
{
  for (int i = 0; i < u_nhalf; i++)
    if (u_half[i].a == full)
      {
        if (u_half[i].b != half)
          {
            fprintf (stderr, "gen-ktab: U+%04x has two half width forms\n", full);
            exit (2);
          }
        u_half[i].kind |= kind;
        return;
      }
  u_half[u_nhalf].a = full;
  u_half[u_nhalf].b = half;
  u_half[u_nhalf].kind = kind;
  u_nhalf++;
}

// The voiced (or semi voiced) full width characters of T[0..N), the half
// width kana of the code FIRST + i: the character -> the base and the mark
// (all half width), and the base (the full width character of the base in
// BASE_TABLE) + the full width mark -> the character.
static void
u_add_voiced (const wchar_t *t, int n, int first, int kind, int semi,
              const wchar_t *base_table)
{
  for (int i = 0; i < n; i++)
    {
      if (t[i] == L' ')
        continue;
      int code = first + i;             // the code in 0xa1..0xdf
      u_voiced[u_nvoiced].a = t[i];
      u_voiced[u_nvoiced].b = 0xff61 + (code - 0xa1);
      u_voiced[u_nvoiced].c = semi ? 0xff9f : 0xff9e;
      u_voiced[u_nvoiced].kind = kind;
      u_nvoiced++;

      u_combine[u_ncombine].a = base_table[code - 0xa1];
      u_combine[u_ncombine].b = semi ? 0x309c : 0x309b;
      u_combine[u_ncombine].c = t[i];
      u_combine[u_ncombine].kind = kind;
      u_ncombine++;
    }
}

static void
gen_unicode_tables ()
{
  printf ("\n// Unicode tables (XYZZY_CHAR_UNICODE = 1)\n");
  u_print_chars ("u_to_full_20_7e", w_f_20_7e, 95);
  u_print_chars ("u_to_fullkata_ff61_ff9f", w_fk_a1_df, 63);
  u_print_chars ("u_to_fullhira_ff61_ff9f", w_fh_a1_df, 63);
  printf ("#define U_VOICED_SOUND_MARK 0x309b\n");
  printf ("#define U_SEMI_VOICED_SOUND_MARK 0x309c\n");
  printf ("#define U_HALF_VOICED_SOUND_MARK 0xff9e\n");
  printf ("#define U_HALF_SEMI_VOICED_SOUND_MARK 0xff9f\n\n");

  for (int i = 0; i < 95; i++)
    u_add_half (w_f_20_7e[i], 0x20 + i, K_ASC);
  for (int i = 0; i < 63; i++)
    {
      u_add_half (w_fk_a1_df[i], 0xff61 + i, K_KATA);
      u_add_half (w_fh_a1_df[i], 0xff61 + i, K_HIRA);
    }
  qsort (u_half, u_nhalf, sizeof u_half[0], u_cmp);

  u_add_voiced (w_fh_voiced_b6_ce, 25, 0xb6, K_HIRA, 0, w_fh_a1_df);
  u_add_voiced (w_fh_semi_voiced_ca_ce, 5, 0xca, K_HIRA, 1, w_fh_a1_df);
  u_add_voiced (w_fk_voiced_b3_ce, 28, 0xb3, K_KATA, 0, w_fk_a1_df);
  u_add_voiced (w_fk_semi_voiced_ca_ce, 5, 0xca, K_KATA, 1, w_fk_a1_df);
  qsort (u_voiced, u_nvoiced, sizeof u_voiced[0], u_cmp);
  qsort (u_combine, u_ncombine, sizeof u_combine[0], u_cmp);

  printf ("struct u_width_entry\n{\n  u_int16_t a, b, c;\n  u_char kind;\n};\n\n");

  // a = full width, b = half width
  printf ("#define U_TO_HALF_COUNT %d\n", u_nhalf);
  printf ("static const u_width_entry u_to_half[] = {\n");
  for (int i = 0; i < u_nhalf; i++)
    printf ("{0x%04x,0x%04x,0,%d},\n", u_half[i].a, u_half[i].b, u_half[i].kind);
  printf ("};\n\n");

  // a = full width voiced (or semi voiced), b = the base, c = the mark
  // (both half width)
  printf ("#define U_VOICED_COUNT %d\n", u_nvoiced);
  printf ("static const u_width_entry u_voiced[] = {\n");
  for (int i = 0; i < u_nvoiced; i++)
    printf ("{0x%04x,0x%04x,0x%04x,%d},\n",
            u_voiced[i].a, u_voiced[i].b, u_voiced[i].c, u_voiced[i].kind);
  printf ("};\n\n");

  // a = base, b = the full width mark, c = the full width voiced
  printf ("#define U_COMBINE_COUNT %d\n", u_ncombine);
  printf ("static const u_width_entry u_combine[] = {\n");
  for (int i = 0; i < u_ncombine; i++)
    printf ("{0x%04x,0x%04x,0x%04x,%d},\n",
            u_combine[i].a, u_combine[i].b, u_combine[i].c, u_combine[i].kind);
  printf ("};\n\n");
}
