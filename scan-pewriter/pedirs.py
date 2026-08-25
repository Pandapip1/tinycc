#!/usr/bin/env python3
"""Validate PE data-directory SIZES against the structures actually emitted.

For each directory tcc fills in (IMPORT, IAT, EXPORT, BASERELOC, EXCEPTION,
TLS, DELAY_IMPORT) this walks the real structure in the file, works out how
many bytes it genuinely occupies, and compares that with the Size the
optional header advertises.
"""
import struct, sys

def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def u64(b,o): return struct.unpack_from('<Q',b,o)[0]

class Img:
    def __init__(self, path):
        self.d = d = open(path,'rb').read()
        nt = u32(d,0x3c); self.nt=nt
        self.nsec = u16(d,nt+6)
        optsz = u16(d,nt+20)
        oh = nt+24; self.oh = oh
        self.magic = u16(d,oh)
        self.p64 = self.magic == 0x20b
        self.imagebase = u64(d,oh+24) if self.p64 else u32(d,oh+28)
        q = oh+(112 if self.p64 else 96)
        self.nrva = u32(d,q-4)
        self.dd = [(u32(d,q+8*i),u32(d,q+8*i+4)) for i in range(self.nrva)]
        sh = q+8*self.nrva
        self.secs=[]
        for i in range(self.nsec):
            s=sh+40*i
            self.secs.append(dict(name=d[s:s+8], vsize=u32(d,s+8), vaddr=u32(d,s+12),
                                  rawsz=u32(d,s+16), rawptr=u32(d,s+20), chars=u32(d,s+36)))
    def off(self, rva):
        for s in self.secs:
            if s['vaddr'] <= rva < s['vaddr']+max(s['vsize'],s['rawsz']):
                o = s['rawptr'] + (rva - s['vaddr'])
                return o if s['rawsz'] and o < s['rawptr']+s['rawsz'] else None
        return None
    def cstr(self, rva):
        o=self.off(rva)
        if o is None: return None
        e=self.d.index(b'\0',o)
        return self.d[o:e].decode('latin1')

def check(path):
    im = Img(path); d = im.d
    print("== %s (%s)" % (path, "PE32+" if im.p64 else "PE32"))
    def cmp(name, hdr, real, extra=""):
        print("  [%s] %-14s hdr Size=%#-8x real=%#-8x %s" %
              ("OK  " if hdr==real else "MISMATCH", name, hdr, real, extra))

    # IMPORT (dir 1): array of 20-byte descriptors, null-terminated
    rva,sz = im.dd[1]
    if rva:
        o = im.off(rva); n=0; iat_spans=[]
        while True:
            desc = d[o+20*n:o+20*n+20]
            if desc == b'\0'*20: n+=1; break
            oft,tds,fc,nm,ft = struct.unpack('<IIIII', desc)
            dll = im.cstr(nm)
            # count thunks in FirstThunk array
            to = im.off(ft); k=0
            W = 8 if im.p64 else 4
            rd = u64 if im.p64 else u32
            while rd(d,to+W*k) != 0: k+=1
            iat_spans.append((ft, (k+1)*W, dll, k))
            n+=1
        cmp("IMPORT", sz, 20*n, "%d descriptors incl. null terminator" % n)
        lo = min(a for a,_,_,_ in iat_spans); hi = max(a+l for a,l,_,_ in iat_spans)
        cmp("IAT", im.dd[12][1], hi-lo,
            "IAT rva hdr=%#x real-lowest-FirstThunk=%#x; dlls=%s"
            % (im.dd[12][0], lo, [(x[2],x[3]) for x in iat_spans]))

    # EXPORT (dir 0)
    rva,sz = im.dd[0]
    if rva:
        o=im.off(rva)
        nfun=u32(d,o+20); nnam=u32(d,o+24)
        af=u32(d,o+28); an=u32(d,o+32); ao=u32(d,o+36); namerva=u32(d,o+12)
        end = max(rva+40, af+4*nfun, an+4*nnam, ao+2*nnam)
        # plus the name strings
        strend = end
        for i in range(nnam):
            nr=u32(d, im.off(an)+4*i)
            strend = max(strend, nr+len(im.cstr(nr))+1)
        strend = max(strend, namerva+len(im.cstr(namerva))+1)
        cmp("EXPORT", sz, strend-rva, "%d funcs %d names, tables end %#x, strings end %#x"
            % (nfun,nnam,end,strend))

    # BASERELOC (dir 5)
    rva,sz = im.dd[5]
    if rva:
        o=im.off(rva); tot=0
        while tot < sz:
            b=u32(d,o+tot+4)
            if b==0: break
            tot+=b
        cmp("BASERELOC", sz, tot, "sum of SizeOfBlock over the chain")

    # TLS (dir 9)
    rva,sz = im.dd[9]
    if rva:
        W = 8 if im.p64 else 4
        real = 4*W + 8
        o=im.off(rva)
        rd = u64 if im.p64 else u32
        vals=[rd(d,o+i*W) for i in range(4)]
        cmp("TLS", sz, real, "IMAGE_TLS_DIRECTORY%d; Start=%#x End=%#x Index=%#x CB=%#x"
            % (64 if im.p64 else 32, *vals))
        for lbl,v in zip(("StartAddressOfRawData","EndAddressOfRawData","AddressOfIndex","AddressOfCallBacks"),vals):
            inside = im.imagebase <= v < im.imagebase + u32(d, im.oh+56)
            print("      %-22s %#x  %s" % (lbl, v, "VA in image" if inside else "*** not a VA in this image ***"))

    # EXCEPTION (dir 3): array of 12-byte RUNTIME_FUNCTION (x64)
    rva,sz = im.dd[3]
    if rva and im.magic==0x20b:
        o=im.off(rva); n=0
        while o+12*n+12 <= im.off(rva)+sz:
            beg,end,unw = struct.unpack_from('<III', d, o+12*n)
            if (beg,end,unw)==(0,0,0): break
            n+=1
        print("  [info] EXCEPTION  %d RUNTIME_FUNCTION entries in Size=%#x (%s)"
              % (n, sz, "exact" if 12*n==sz else "%d bytes slack" % (sz-12*n)))
        # each entry must be sorted ascending and unwind info must be in the image
        prev=0; bad=0
        for i in range(n):
            beg,end,unw = struct.unpack_from('<III', d, o+12*i)
            if beg < prev: print("    [MISMATCH] RUNTIME_FUNCTION[%d] BeginAddress %#x < previous %#x (table must be sorted, PE/COFF 6.3)" % (i,beg,prev)); bad+=1
            prev=beg
            if im.off(unw) is None: print("    [MISMATCH] entry[%d] UnwindInfoAddress %#x not in any raw section" % (i,unw)); bad+=1
            if im.off(beg) is None: print("    [MISMATCH] entry[%d] BeginAddress %#x not in any raw section" % (i,beg)); bad+=1
            if end <= beg: print("    [MISMATCH] entry[%d] EndAddress %#x <= BeginAddress %#x" % (i,end,beg)); bad+=1
        if not bad: print("    all %d entries sorted, in-range" % n)

    # DELAY_IMPORT (dir 13)
    rva,sz = im.dd[13]
    if rva:
        o=im.off(rva); n=0
        while d[o+32*n:o+32*n+32] != b'\0'*32: n+=1
        cmp("DELAY_IMPORT", sz, 32*(n+1), "%d descriptors + null terminator" % n)

for p in sys.argv[1:]:
    check(p); print()
