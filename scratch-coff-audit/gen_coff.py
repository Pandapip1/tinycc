#!/usr/bin/env python3
"""Build minimal PE-COFF objects for auditing tcc's pe_load_obj_file()."""
import struct, sys

def build(machine=0x8664, sections=(), symbols=(), strtab=b'', opthdr=b''):
    """sections: list of dicts {name, flags, vsize, vaddr, data, relocs, rawsize(optional
    override), rawptr(override), relptr(override), nreloc(override)}
    relocs: list of (vaddr, symndx, type)
    symbols: list of (name8bytes, value, scnum, sclass, numaux, auxbytes)"""
    n = len(sections)
    # layout
    off = 20 + len(opthdr) + n*40
    for s in sections:
        s['_rawptr'] = off if s['data'] else 0
        off += len(s['data'])
    for s in sections:
        s['_relptr'] = off if s['relocs'] else 0
        off += len(s['relocs'])*10
    symptr = off if symbols else 0
    nsyms = sum(1 + sy[4] for sy in symbols)

    fh = struct.pack('<HHIIIHH', machine, n, 0, symptr, nsyms, len(opthdr), 0)
    sh = b''
    for s in sections:
        nm = s['name'][:8].ljust(8, b'\0')
        sh += struct.pack('<8sIIIIIIHHI', nm, s.get('vsize', 0), s.get('vaddr', 0),
                          s.get('rawsize', len(s['data'])),
                          s.get('rawptr', s['_rawptr']),
                          s.get('relptr', s['_relptr']), 0,
                          s.get('nreloc', len(s['relocs'])), 0, s['flags'])
    body = b''.join(s['data'] for s in sections)
    rel = b''
    for s in sections:
        for (v, ix, t) in s['relocs']:
            rel += struct.pack('<IIH', v & 0xffffffff, ix & 0xffffffff, t & 0xffff)
    sy = b''
    for (name, value, scnum, sclass, numaux, aux) in symbols:
        # name[8] value[4] scnum[2] type[2] sclass[1] numaux[1] = 18 bytes
        sy += struct.pack('<8sIhHBB', name[:8].ljust(8, b'\0'), value & 0xffffffff,
                          scnum, 0, sclass, numaux) + aux
    st = struct.pack('<I', len(strtab) + 4) + strtab if symbols else b''
    return fh + opthdr + sh + body + rel + sy + st

TEXT = 0x60500020   # CODE|MEM_EXECUTE|MEM_READ|ALIGN_16
DATA = 0xC0400040   # INITIALIZED_DATA|READ|WRITE|ALIGN_8
BSS  = 0xC0300080   # UNINITIALIZED_DATA|READ|WRITE|ALIGN_4

def sec(name, flags, data=b'', relocs=(), **kw):
    d = dict(name=name, flags=flags, data=data, relocs=list(relocs))
    d.update(kw)
    return d

def sym(name, value=0, scnum=1, sclass=2, numaux=0, aux=b''):
    return (name, value, scnum, sclass, numaux, aux.ljust(18*numaux, b'\0'))
