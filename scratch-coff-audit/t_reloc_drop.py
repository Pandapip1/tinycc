#!/usr/bin/env python3
"""IMAGE_SCN_LNK_NRELOC_OVFL: pe_load_obj_file computes nrel (unsigned long)
from the dummy record, then iterates `j < (int)nrel`.  A count >= 0x80000000
truncates to a negative int, so every relocation in the section is silently
dropped and tcc exits 0 with unrelocated contents.  It also mallocs
nrel*10 bytes (up to ~43 GB) beforehand."""
import sys
from gen_coff import *
count = int(sys.argv[1], 0)                       # goes in the dummy record
# 8 bytes of data; one real ADDR64 reloc at offset 0 pointing at _tgt
relocs = [(count, 0, 0), (0, 1, 1)]
o = build(0x8664,
          sections=[sec(b'.data', DATA | 0x01000000, data=b'\0'*8,
                        relocs=relocs, nreloc=0xffff)],
          symbols=[sym(b'_src', 0, 1, 2), sym(b'_tgt', 0, 0, 2)])
open(sys.argv[2], 'wb').write(o)
