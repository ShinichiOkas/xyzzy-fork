// -*-C++-*-
// The reader and the writer of ini files (xyzzy.ini).
//
// They do what GetPrivateProfileString and WritePrivateProfileString did,
// but the file is opened by the W layer (wfile.h) and read and written by
// the converters of the codec layer (codec.h). See section 4.3 of
// docs/refactoring/s4-char-design.md.
//
// The file:
//   - Reading: a file that begins with a UTF-8 byte order mark is UTF-8, one
//     that begins with a UTF-16 byte order mark is UTF-16. Any other file is
//     cp932 (the files that the old xyzzy wrote).
//   - Writing: XYZZY_CHAR_UNICODE 0 writes cp932 (no mark). 1 writes UTF-8
//     with a byte order mark (the old xyzzy cannot read it).
//
// The rules of the file are those of GetPrivateProfileString:
//   - section [name] and key=value; names are compared without case and
//     without the white space around them; the first section of the name
//     and the first key of the name in it are used; a line that begins with
//     ';' is a comment; a line without '=' is ignored.
//   - The white space around a value is dropped, and so are the pair of
//     double quotes (or single quotes) around it.
//   - A write replaces the value of the key in place (the line, its key and
//     its end of line are kept), adds a new key after the last key of the
//     section, and adds a new section at the end of the file.
//
// All char * of this interface are the narrow strings of the program
// (cp932 now). The conversion is in ini_narrow_spec of ini.cc.
#ifndef _ini_h_
#define _ini_h_

// Returns the length of the value that is written in buf (at most size - 1
// bytes, with the terminating NUL), 0 and "" when the key is not found.
// *found tells whether the key was found (a value can be empty).
int ini_read (const char *file, const char *section, const char *key,
              char *buf, int size, int *found = 0);

// Writes a value of a key. With value == 0 the key is deleted; with key == 0
// the section is deleted. It returns 1 when the file is written (or when
// there is nothing to change), 0 when it could not be written (a file that
// exists but cannot be read is not written: it would lose the lines).
// mode: the encoding of the file that is written. The default is by
// XYZZY_CHAR_UNICODE; the tests ask for the 1 side on the 0 build.
#define INI_WRITE_DEFAULT (-1)
#define INI_WRITE_CP932 0
#define INI_WRITE_UTF8_BOM 1
int ini_write (const char *file, const char *section, const char *key,
               const char *value, int mode = INI_WRITE_DEFAULT);

#endif /* _ini_h_ */
