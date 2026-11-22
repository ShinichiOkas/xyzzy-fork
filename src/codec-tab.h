// -*-C++-*-
// Conversion tables of the codec layer. The tables themselves are generated
// by gen-codec.cc (src/gen/codec-tab.cc) from src/unicode/*.TXT.
#ifndef _codec_tab_h_
#define _codec_tab_h_

typedef unsigned short codec_u16;

// Reverse table (Unicode -> code) in the two stage form.
struct codec_rev
{
  const codec_u16 *idx;         // [256]: block number or 0xffff
  const codec_u16 *blk;         // blocks of 256 codes, 0xffff = none
};

// Returns the code or -1. Only the BMP has entries.
inline int
codec_rev_get (const codec_rev &t, unsigned int u)
{
  if (u > 0xffff)
    return -1;
  codec_u16 b = t.idx[u >> 8];
  if (b == 0xffff)
    return -1;
  codec_u16 c = t.blk[b * 256 + (u & 255)];
  return c == 0xffff ? -1 : int (c);
}

// 1 byte character sets: ISO 8859-x, Windows code pages, KOI8.
// fwd[b - 0x80] is the Unicode of the byte b (0x80..0xff) or 0xffff.
// rev gives the byte (0x80..0xff) of a Unicode.
struct codec_sb_table
{
  const codec_u16 *fwd;
  const codec_rev *rev;
};

enum
{
  CODEC_SB_ISO8859_1,
  CODEC_SB_ISO8859_2,
  CODEC_SB_ISO8859_3,
  CODEC_SB_ISO8859_4,
  CODEC_SB_ISO8859_5,
  CODEC_SB_ISO8859_7,
  CODEC_SB_ISO8859_9,
  CODEC_SB_ISO8859_10,
  CODEC_SB_ISO8859_13,
  CODEC_SB_CP1250,
  CODEC_SB_CP1251,
  CODEC_SB_CP1252,
  CODEC_SB_CP1253,
  CODEC_SB_CP1254,
  CODEC_SB_CP1257,
  CODEC_SB_KOI8R,
  CODEC_SB_KOI8U,
  CODEC_SB_MAX
};

extern const codec_sb_table codec_sb[CODEC_SB_MAX];

// cp932 (Windows-31J). 2 byte codes only. The single bytes are handled in
// the code: 0x00-0x7f and 0x80 are U+0000-U+007f and U+0080, 0xa1-0xdf are
// U+ff61-U+ff9f.
// fwd: index (lead index * CODEC_CP932_TRAILS + trail - 0x40). The lead
// index is 0..30 for 0x81..0x9f and 31..59 for 0xe0..0xfc.
// rev: the code that WideCharToMultiByte(932) chooses for a Unicode.
// ned_rev: the NEC selected IBM extension code (ED, EE) of a Unicode.
#define CODEC_CP932_LEADS 60
#define CODEC_CP932_TRAILS 189
extern const codec_u16 codec_cp932_fwd[CODEC_CP932_LEADS * CODEC_CP932_TRAILS];
extern const codec_rev codec_cp932_rev;
extern const codec_rev codec_cp932_ned_rev;

// IBM extension (cp932 FA40..FC4B) -> EUC-JP code. 0x8Fxxxx is JIS X 0212,
// xxxx is JIS X 0208. Index: (lead - 0xfa) * 188 + (trail - 0x40 [- 1 if > 0x7f]).
extern const unsigned int codec_ibm_euc[388];

// 94 x 94 character sets. fwd/rev index: (c1 - 0x21) * 94 + (c2 - 0x21).
extern const codec_u16 codec_jisx0212_fwd[94 * 94];
extern const codec_rev codec_jisx0212_rev;
extern const codec_u16 codec_gb2312_fwd[94 * 94];
extern const codec_rev codec_gb2312_rev;
extern const codec_u16 codec_ksc5601_fwd[94 * 94];
extern const codec_rev codec_ksc5601_rev;
extern const codec_u16 codec_cns1_fwd[94 * 94];
extern const codec_rev codec_cns1_rev;
extern const codec_u16 codec_cns2_fwd[94 * 94];
extern const codec_rev codec_cns2_rev;

// Big5. fwd/rev index: (lead - 0xa1) * 157 + trail index
// trail index: 0x40..0x7e -> 0..62, 0xa1..0xfe -> 63..156
#define CODEC_BIG5_LEADS 89
#define CODEC_BIG5_TRAILS 157
extern const codec_u16 codec_big5_fwd[CODEC_BIG5_LEADS * CODEC_BIG5_TRAILS];
extern const codec_rev codec_big5_rev;

#endif
