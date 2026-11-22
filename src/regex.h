#ifndef _regex_h_
# define _regex_h_

#define MAX_REGS 10

// A compiled pattern is an array of Char words, and the offsets and the repeat
// counts in it are limited to this many words.  It has nothing to do with the
// number of the characters (CHAR_LIMIT).
#define RE_WORD_LIMIT 0x10000

// The 256 slot tables of the search (the fastmap of the regexp search, the skip
// table of the Boyer-Moore search, the fastmap of the regexp keywords) are
// indexed by fold256 (c).  An ASCII character has a slot of its own, and every
// other character, whatever its value is, is mixed into 0x80..0xFF.  Never
// index such a table with (c >> 8): the high byte of a character is also an
// ASCII slot (BUG-0056).  A table has to be a superset: a slot stands for all
// the characters that fold to it.  The function does not depend on the value
// of a character, so it is the same for the old internal code and for Unicode.
inline int
fold256 (Char c)
{
  if (c < 0x80)
    return int (c);
  return 0x80 | int ((c ^ (c >> 7) ^ (c >> 14) ^ (c >> 21) ^ (c >> 28)) & 0x7f);
}

// A set of characters: the ASCII characters as 128 bits and the other
// characters as sorted ranges [lo, hi].  The range does not depend on the
// value of a character.  It is made for a regexp class [...] and for
// skip-chars, and compiled to an array of Char words:
//   size, ascii[4], nranges, lo0, hi0, lo1, hi1, ...
// where size is the number of all the words, the size word included.
class charclass
{
  enum {ASCII_WORDS = 4};
  Char cc_ascii[ASCII_WORDS];
  Char *cc_ranges;              // pairs of (lo, hi)
  int cc_nranges;
  int cc_allocated;
  static int compare_range (const void *, const void *);
  void normalize ();
public:
  charclass ();
  ~charclass ();
  charclass (const charclass &) = delete;
  charclass &operator = (const charclass &) = delete;
  void set_ascii (Char c)
    {cc_ascii[c >> 5] |= Char (1) << (c & 31);}
  // The characters lo..hi.  An ASCII character is not translated.
  void set_range (Char lo, Char hi);
  void set (Char c) {set_range (c, c);}
  // The number of the words of the compiled form (merges the ranges).
  int size ();
  // Write the compiled form (size () words) and return the end of it.
  Char *copy (Char *);
  // Does the compiled form P (starting at the size word) hold C?
  static int match (const Char *p, Char c)
    {
      if (c < 0x80)
        return (p[1 + (c >> 5)] >> (c & 31)) & 1;
      return match_range (p, c);
    }
  static int match_range (const Char *, Char);
  // Set the fastmap slots of the characters of the compiled form P.
  static void fastmap (const Char *p, char *fastmap);
  // Set the fastmap slots of the characters that are not in P (a slot of a
  // non-ASCII character is always set; it stands for many characters).
  static void not_fastmap (const Char *p, char *fastmap);
};

struct re_point;
class re_search;

static inline int
re_syntax_word_p (u_char c)
{
  return c == SCword;
}

static inline int
re_syntax_symbol_p (u_char c)
{
  return c == SCsymbol || c == SCword;
}

class Regexp
{
public:
  struct sregs
    {
      int nregs;
      point_t start[MAX_REGS];
      point_t end[MAX_REGS];
    };
  sregs re_regs;
protected:
  Char *re_pattern;
  int re_size;
  const u_char *re_translate;
  const syntax_table *re_syntax_table;
  int re_match_void_p;
  int re_match_bol_p;
  char re_fastmap[256];
  lisp re_object;
  int re_has_backref;
  Region re_range;
  point_t re_last_match;
  lChar re_last_match_char;

  class record_failure
    {
      enum {TABSIZE = 97};
      struct ent
        {
          ent *cdr;
          point_t point, max;
          const Char *pat;
        };
      ent *m_tab[TABSIZE];
      ent m_entbuf[512];
      ent *m_ep;
      static u_int hashval (const Char *, point_t, point_t);
    public:
      record_failure ();
      void init ();
      int find (const Char *, point_t, point_t) const;
      void add (const Char *, const re_point &);
    };
  record_failure re_failure;

  int backref (re_point &, int) const;
  int match_char_class (const Char *, Char) const;
  int match (re_point &, const Char *, const Char *);
  int match (const re_point &);
  int search (const re_search &, re_point &);
  int branch (re_point &, const Char *, const Char *);
  int closure (re_point &, const Char *, const Char *, int);
  int closure_backtrack (re_point &, const Char *, const Char *, int);
  int simple_closure (re_point &, const Char *, const Char *);
  int shortest_simple_closure (re_point &, const Char *, const Char *);
  static int compare_regs (const sregs &, const sregs &);
  void start_save_regs (int, point_t);
  void end_save_regs (int, point_t);
  void init_match (const re_point &, point_t, lChar);
  void init_match (const Buffer *, point_t, lChar);
  int bobp (const re_point &) const;
  int eobp (const re_point &) const;
  static int repeat_max (Char);
  // The slot of the fastmap for the character in the text (translated as the
  // pattern is).
  int fastmap_slot (Char c) const
    {return fold256 (ascii_char_p (c) ? re_translate[c] : c);}
  u_char char_syntax (Char cc) const
    {
      return syntax_of (re_syntax_table, cc);
    }
  int syntax_word_p (Char cc) const
    {return re_syntax_word_p (char_syntax (cc));}
  int syntax_symbol_p (Char cc) const
    {return re_syntax_symbol_p (char_syntax (cc));}

public:
  void compile (const Char *, int, int);
  void compile (lisp, int);
  int search (const Char *, int, int);
  int search (const Buffer *, const Point &, point_t, point_t, point_t, lChar);
  int search_backward (const Buffer *, const Point &, point_t, point_t, point_t, lChar);
  int match (const Char *, int, int);
  int match (const Buffer *, const Point &, point_t, point_t);
  Regexp (const u_char *translate, const syntax_table *syntax_tab)
       : re_pattern (0), re_translate (translate),
         re_syntax_table (syntax_tab), re_object (0) {}
  lisp make_regexp (lisp) const;
  ~Regexp ()
    {
      if (re_pattern && !re_object)
        free (re_pattern);
    }
  static int smart_case_fold_p (const Char *, int);
  const Region &range () const {return re_range;}
  point_t last_match () const {return re_last_match;}
  lChar last_match_char () const {return re_last_match_char;}
  static int merge_fastmap (lisp, char *, const syntax_table *);
};

#endif
