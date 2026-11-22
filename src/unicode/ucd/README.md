# Unicode Character Database (UCD) files

These files are inputs of `gen-uprops` (src/gen-uprops.cc). The build turns
them into the Unicode property tables of `uprops.cc` (src/gen/uprops-data.h).
The build does not use the network; to change the version, replace the files
here and set `UCD_VERSION` in src/gen-uprops.cc.

- Version: Unicode 18.0.0 (the latest release when these files were added, 2026-11-22)
- Source: https://www.unicode.org/Public/18.0.0/ucd/
- License: Unicode License v3 (https://www.unicode.org/license.txt, copied
  here as `UNICODE-LICENSE.txt`). The data files carry their own copyright
  header, which is kept as is.

| File | Used for |
|---|---|
| UnicodeData.txt | general category, simple upper / lower / title case mappings |
| EastAsianWidth.txt | East Asian Width (display width, ambiguous characters) |
| Scripts.txt | script (font group, `si:uprops-script`) |
| Blocks.txt | block (font group only; not kept in the tables) |
| CaseFolding.txt | simple case folding (status C and S) |

The files are tracked as they are published (LF line ends). The generator
accepts CR LF as well.
