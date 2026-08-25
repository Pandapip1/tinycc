#!/usr/bin/env python3
"""Generate malformed PE-COFF objects for tests/pe-coff-test.sh.

These exercise the reader's *rejection* paths, which cannot be produced with
an assembler: the header fields involved are ones no assembler ever writes.
Each object is a valid pe-i386 relocatable object except for the one field
named below, so a failure to reject it is unambiguously the reader's fault.

usage: pe-coff-gen.py <outdir>
"""
import os, struct, sys

MACHINE = 0x014c                # IMAGE_FILE_MACHINE_I386
TEXT    = 0x60500020            # CODE | EXECUTE | READ | ALIGN_16
DATA    = 0xC0400040            # INITIALIZED_DATA | READ | WRITE | ALIGN_8
NRELOC_OVFL = 0x01000000        # IMAGE_SCN_LNK_NRELOC_OVFL

def obj(sections, symbols, nsyms=None):
    """sections: [(name, flags, data, [(vaddr, symndx, type)], nreloc_field)]
       symbols:  [(name, value, scnum, sclass)]"""
    off = 20 + 40 * len(sections)
    rawptr, relptr = [], []
    for (_, _, data, _, _) in sections:
        rawptr.append(off if data else 0)
        off += len(data)
    for (_, _, _, relocs, _) in sections:
        relptr.append(off if relocs else 0)
        off += 10 * len(relocs)
    symptr = off if symbols else 0

    out = struct.pack('<HHIIIHH', MACHINE, len(sections), 0, symptr,
                      len(symbols) if nsyms is None else nsyms, 0, 0)
    for i, (name, flags, data, relocs, nreloc) in enumerate(sections):
        out += struct.pack('<8sIIIIIIHHI', name.encode()[:8].ljust(8, b'\0'),
                           0, 0, len(data), rawptr[i], relptr[i], 0,
                           len(relocs) if nreloc is None else nreloc, 0, flags)
    for (_, _, data, _, _) in sections:
        out += data
    for (_, _, _, relocs, _) in sections:
        for (v, ix, t) in relocs:
            out += struct.pack('<IIH', v & 0xffffffff, ix & 0xffffffff, t)
    for (name, value, scnum, sclass) in symbols:
        out += struct.pack('<8sIhHBB', name.encode()[:8].ljust(8, b'\0'),
                           value & 0xffffffff, scnum, 0, sclass, 0)
    if symbols:
        out += struct.pack('<I', 4)     # empty string table
    return out

def nsyms_overflow():
    """NumberOfSymbols = 119400000.  The symbol table is walked with an int
       index, and 119400000 * 18 (COFF_SYMESZ) exceeds INT_MAX, so before the
       fix the pointer wrapped ~2GB backwards and tcc took SIGSEGV.  The count
       cannot fit in a 116-byte file, so it must be rejected outright."""
    return obj([('.text', TEXT, b'\x90' * 4, [], None)],
               [('_start', 0, 1, 2)], nsyms=119400000)

def nreloc_ovfl(count):
    """IMAGE_SCN_LNK_NRELOC_OVFL: the real relocation count is the leading
       dummy record's VirtualAddress minus one.  count=2 is a well-formed
       object with exactly one relocation and must still load; count is a
       32-bit field, and 0x80000002 is negative as an int, which before the
       fix made the loop body never run - every relocation silently dropped,
       exit status 0.  It must be a loud error instead."""
    return obj([('.data', DATA | NRELOC_OVFL, b'\0' * 8,
                 [(count, 0, 0), (0, 1, 6)], 0xffff)],
               [('_src', 0, 1, 2), ('_tgt', 0, 0, 2)])

if __name__ == '__main__':
    d = sys.argv[1] if len(sys.argv) > 1 else '.'
    for name, data in (('nsyms-overflow.o', nsyms_overflow()),
                       ('nreloc-ok.o',      nreloc_ovfl(2)),
                       ('nreloc-ovfl.o',    nreloc_ovfl(0x80000002))):
        with open(os.path.join(d, name), 'wb') as f:
            f.write(data)
