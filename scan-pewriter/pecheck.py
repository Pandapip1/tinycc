#!/usr/bin/env python3
"""Independent PE/PE32+ header verifier for tinycc's tccpe.c output.

Parses the emitted image from raw bytes and recomputes, from the file's
actual content, every optional-header field that is supposed to describe
the file.  Reports header-vs-reality disagreements.
"""
import struct, sys

def u8(b,o):  return b[o]
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def u64(b,o): return struct.unpack_from('<Q',b,o)[0]

def pe_checksum(data, cksum_off):
    """PE/COFF image checksum: 16-bit ones-complement folded sum of the whole
    file with the CheckSum field taken as zero, plus the file size."""
    b = bytearray(data)
    b[cksum_off:cksum_off+4] = b'\0\0\0\0'
    if len(b) & 1:
        b += b'\0'
    s = 0
    for i in range(0, len(b), 2):
        s += b[i] | (b[i+1] << 8)
        s = (s & 0xffff) + (s >> 16)
    s = (s & 0xffff) + (s >> 16)
    return (s + len(data)) & 0xffffffff

def main(path):
    d = open(path,'rb').read()
    out = []
    P = out.append
    assert d[:2] == b'MZ'
    nt = u32(d, 0x3c)
    assert d[nt:nt+4] == b'PE\0\0', 'no PE sig'
    fh = nt+4
    mach   = u16(d, fh+0)
    nsec   = u16(d, fh+2)
    ptr_symtab = u32(d, fh+8)
    nsyms  = u32(d, fh+12)
    optsz  = u16(d, fh+16)
    chars  = u16(d, fh+18)
    oh = fh + 20
    magic = u16(d, oh)
    pe32p = (magic == 0x20b)
    P(f"file            : {path}  ({len(d)} bytes)")
    P(f"Machine         : 0x{mach:04x}   Magic 0x{magic:03x} ({'PE32+' if pe32p else 'PE32'})")
    P(f"NumberOfSections: {nsec}   SizeOfOptionalHeader {optsz}   Characteristics 0x{chars:04x}")

    f = {}
    f['SizeOfCode']              = u32(d, oh+4)
    f['SizeOfInitializedData']   = u32(d, oh+8)
    f['SizeOfUninitializedData'] = u32(d, oh+12)
    f['AddressOfEntryPoint']     = u32(d, oh+16)
    f['BaseOfCode']              = u32(d, oh+20)
    if pe32p:
        f['ImageBase'] = u64(d, oh+24); o = oh+32
    else:
        f['BaseOfData'] = u32(d, oh+24)
        f['ImageBase']  = u32(d, oh+28); o = oh+32
    f['SectionAlignment'] = u32(d, o+0)
    f['FileAlignment']    = u32(d, o+4)
    f['MajorOSVersion']   = u16(d, o+8)
    f['MinorOSVersion']   = u16(d, o+10)
    f['MajorImageVersion']= u16(d, o+12)
    f['MinorImageVersion']= u16(d, o+14)
    f['MajorSubsysVer']   = u16(d, o+16)
    f['MinorSubsysVer']   = u16(d, o+18)
    f['Win32VersionValue']= u32(d, o+20)
    f['SizeOfImage']      = u32(d, o+24)
    f['SizeOfHeaders']    = u32(d, o+28)
    cksum_off = o+32
    f['CheckSum']         = u32(d, o+32)
    f['Subsystem']        = u16(d, o+36)
    f['DllCharacteristics']=u16(d, o+38)
    q = o+40
    W = 8 if pe32p else 4
    rd = u64 if pe32p else u32
    f['SizeOfStackReserve'] = rd(d, q+0*W)
    f['SizeOfStackCommit']  = rd(d, q+1*W)
    f['SizeOfHeapReserve']  = rd(d, q+2*W)
    f['SizeOfHeapCommit']   = rd(d, q+3*W)
    q += 4*W
    f['LoaderFlags']        = u32(d, q)
    f['NumberOfRvaAndSizes']= u32(d, q+4)
    ddir_off = q+8
    nrva = f['NumberOfRvaAndSizes']
    dd = [(u32(d, ddir_off+8*i), u32(d, ddir_off+8*i+4)) for i in range(nrva)]

    sh_off = ddir_off + 8*nrva
    P(f"opt hdr bytes   : {sh_off - oh} (SizeOfOptionalHeader says {optsz})")
    secs = []
    for i in range(nsec):
        s = sh_off + 40*i
        secs.append(dict(
            name=d[s:s+8].rstrip(b'\0').decode('latin1'),
            vsize=u32(d,s+8), vaddr=u32(d,s+12),
            rawsz=u32(d,s+16), rawptr=u32(d,s+20),
            chars=u32(d,s+36)))
    P("")
    P("sections (from file):")
    for s in secs:
        P("  %-12s vaddr %08x vsize %08x rawptr %08x rawsz %08x flags %08x"
          % (s['name'], s['vaddr'], s['vsize'], s['rawptr'], s['rawsz'], s['chars']))

    # ---- independently computed truth ----
    CODE  = 0x20; INIT = 0x40; UNINIT = 0x80
    salign = f['SectionAlignment']; falign = f['FileAlignment']
    def valign(n): return (n + salign - 1) & ~(salign - 1)

    truth = {}
    truth['SizeOfCode'] = sum(s['rawsz'] for s in secs if s['chars'] & CODE)
    truth['SizeOfInitializedData'] = sum(s['rawsz'] for s in secs if s['chars'] & INIT)
    # bss: explicit UNINIT sections, plus the tail of any section whose
    # virtual size exceeds its raw size (that tail is zero-filled by the loader)
    truth['SizeOfUninitializedData_explicit'] = sum(
        max(s['vsize'], s['rawsz']) for s in secs if s['chars'] & UNINIT)
    truth['bss_tail_bytes'] = sum(
        max(0, s['vsize'] - s['rawsz']) for s in secs)
    truth['SizeOfImage'] = max(valign(s['vaddr'] + max(s['vsize'], s['rawsz'])) for s in secs)
    truth['SizeOfHeaders_min'] = sh_off + 40*nsec
    truth['BaseOfCode'] = min([s['vaddr'] for s in secs if s['chars'] & CODE] or [0])
    truth['CheckSum'] = pe_checksum(d, cksum_off)
    truth['first_rawptr'] = min([s['rawptr'] for s in secs if s['rawsz']] or [0])

    P("")
    P("optional header fields:")
    for k, v in f.items():
        P("  %-24s %s" % (k, hex(v)))
    P("")
    P("data directories:")
    names = ["EXPORT","IMPORT","RESOURCE","EXCEPTION","SECURITY","BASERELOC","DEBUG","ARCH",
             "GLOBALPTR","TLS","LOAD_CONFIG","BOUND_IMPORT","IAT","DELAY_IMPORT","COM",""]
    for i,(a,sz) in enumerate(dd):
        if a or sz:
            owner = [s['name'] for s in secs if s['vaddr'] <= a < s['vaddr']+max(s['vsize'],s['rawsz'])]
            P("  [%2d] %-13s rva %08x size %08x  in %s" % (i, names[i], a, sz, owner or "*** NO SECTION ***"))

    P("")
    P("=== header vs. independently computed truth ===")
    def cmp(label, got, want, note=""):
        tag = "OK  " if got == want else "MISMATCH"
        P("  [%s] %-26s header=%-12s computed=%-12s %s" % (tag, label, hex(got), hex(want), note))
    cmp("SizeOfCode", f['SizeOfCode'], truth['SizeOfCode'], "sum of rawsz of IMAGE_SCN_CNT_CODE sections")
    cmp("SizeOfInitializedData", f['SizeOfInitializedData'], truth['SizeOfInitializedData'],
        "sum of rawsz of IMAGE_SCN_CNT_INITIALIZED_DATA sections")
    cmp("SizeOfUninitializedData", f['SizeOfUninitializedData'], truth['SizeOfUninitializedData_explicit'],
        "sum of CNT_UNINITIALIZED_DATA sections; zero-fill tails total 0x%x" % truth['bss_tail_bytes'])
    cmp("SizeOfImage", f['SizeOfImage'], truth['SizeOfImage'], "max(align(vaddr+vsize))")
    cmp("BaseOfCode", f['BaseOfCode'], truth['BaseOfCode'], "lowest CODE section rva")
    cmp("CheckSum", f['CheckSum'], truth['CheckSum'], "recomputed ones-complement image checksum")
    P("  [%s] %-26s header=%-12s headers actually end at %s, first raw data at %s"
      % ("OK  " if f['SizeOfHeaders'] >= truth['SizeOfHeaders_min'] and f['SizeOfHeaders'] % falign == 0 else "CHECK",
         "SizeOfHeaders", hex(f['SizeOfHeaders']), hex(truth['SizeOfHeaders_min']), hex(truth['first_rawptr'])))

    # alignment invariants (PE/COFF spec, Optional Header Windows-Specific Fields)
    P("")
    P("=== alignment / layout invariants ===")
    for s in secs:
        if s['vaddr'] % salign:
            P("  [MISMATCH] %s vaddr %08x not a multiple of SectionAlignment %x" % (s['name'], s['vaddr'], salign))
        if s['rawsz'] and s['rawptr'] % falign:
            P("  [MISMATCH] %s rawptr %08x not a multiple of FileAlignment %x" % (s['name'], s['rawptr'], falign))
        if s['rawptr'] + s['rawsz'] > len(d):
            P("  [MISMATCH] %s raw data runs past EOF" % s['name'])
    if f['AddressOfEntryPoint']:
        owner = [s for s in secs if s['vaddr'] <= f['AddressOfEntryPoint'] < s['vaddr']+max(s['vsize'],s['rawsz'])]
        P("  entry rva %08x lands in %s%s" % (f['AddressOfEntryPoint'],
            owner[0]['name'] if owner else "*** NO SECTION ***",
            "" if owner and (owner[0]['chars'] & CODE) else "  (not a CODE section!)"))
    if ptr_symtab:
        P("  PointerToSymbolTable %08x NumberOfSymbols %d -> symtab ends %08x, file %08x"
          % (ptr_symtab, nsyms, ptr_symtab + 18*nsyms, len(d)))

    # ---- base relocations: parse and validate against section content ----
    if len(dd) > 5 and dd[5][0]:
        P("")
        P("=== base relocations (.reloc) ===")
        rva, size = dd[5]
        sec = [s for s in secs if s['vaddr'] <= rva < s['vaddr']+max(s['vsize'],s['rawsz'])][0]
        off = sec['rawptr'] + (rva - sec['vaddr'])
        end = off + size
        types = {}
        nblk = 0
        bad = 0
        while off < end - 7:
            page = u32(d, off); blksz = u32(d, off+4)
            if blksz == 0: break
            nblk += 1
            for j in range(off+8, off+blksz, 2):
                e = u16(d, j)
                t, o2 = e >> 12, e & 0xfff
                types[t] = types.get(t, 0) + 1
                if t == 0: continue
                target = page + o2
                ts = [s for s in secs if s['vaddr'] <= target < s['vaddr']+max(s['vsize'],s['rawsz'])]
                width = 8 if t == 10 else 4
                if not ts:
                    P("    [MISMATCH] reloc rva %08x has no containing section" % target)
                    bad += 1
                elif target - ts[0]['vaddr'] + width > ts[0]['rawsz']:
                    P("    [MISMATCH] reloc rva %08x (%d bytes) not inside raw data of %s"
                      % (target, width, ts[0]['name']))
                    bad += 1
                else:
                    fo = ts[0]['rawptr'] + (target - ts[0]['vaddr'])
                    val = u64(d, fo) if width == 8 else u32(d, fo)
                    # for a DIR64/HIGHLOW fixup the stored value must be a VA
                    # inside the image at the preferred base
                    if not (f['ImageBase'] <= val < f['ImageBase'] + f['SizeOfImage']):
                        P("    [MISMATCH] reloc rva %08x holds %0*x, outside "
                          "[ImageBase, ImageBase+SizeOfImage)" % (target, width*2, val))
                        bad += 1
            off += blksz
        P("    %d blocks, entry types %s, %d suspect entries" %
          (nblk, {("ABSOLUTE" if k==0 else "HIGHLOW" if k==3 else "DIR64" if k==10 else str(k)): v
                  for k,v in sorted(types.items())}, bad))
        expect = 10 if pe32p else 3
        P("    expected fixup type for this magic: %s" % ("DIR64" if pe32p else "HIGHLOW"))
        for k in types:
            if k not in (0, expect):
                P("    [MISMATCH] unexpected fixup type %d present" % k)

    print("\n".join(out))

if __name__ == '__main__':
    for p in sys.argv[1:]:
        main(p)
        print()
