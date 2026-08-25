#!/usr/bin/env python3
"""Structural checks on the PE optional/file header tcc writes (tccpe.c).

Parses the emitted image from raw bytes -- deliberately not sharing any code
with tccpe.c -- and asserts the header agrees with the section table it is
supposed to describe.

usage: pe-header-check.py <image> <check> [<check> ...]
"""
import struct, sys

def u16(b, o): return struct.unpack_from('<H', b, o)[0]
def u32(b, o): return struct.unpack_from('<I', b, o)[0]

UNINIT = 0x00000080
IMAGE_FILE_DEBUG_STRIPPED = 0x0200

class Image:
    def __init__(self, path):
        d = open(path, 'rb').read()
        if d[:2] != b'MZ':
            raise SystemExit("%s: not an MZ image" % path)
        nt = u32(d, 0x3c)
        if d[nt:nt+4] != b'PE\0\0':
            raise SystemExit("%s: no PE signature" % path)
        self.path, self.d = path, d
        fh = nt + 4
        self.nsec = u16(d, fh + 2)
        self.symtab = u32(d, fh + 8)
        self.nsyms = u32(d, fh + 12)
        self.chars = u16(d, fh + 18)
        oh = fh + 20
        self.size_uninit = u32(d, oh + 12)
        sh = oh + u16(d, fh + 16)
        self.secs = [dict(name=d[sh+40*i:sh+40*i+8],
                          vsize=u32(d, sh+40*i+8),
                          chars=u32(d, sh+40*i+36))
                     for i in range(self.nsec)]

def check_uninit_matches_bss(im):
    """SizeOfUninitializedData is the sum of the BSS sections' sizes.

    PE Format, Optional Header Standard Fields: "The size of the
    uninitialized data section (BSS), or the sum of all such sections if
    there are multiple BSS sections."
    """
    want = sum(s['vsize'] for s in im.secs if s['chars'] & UNINIT)
    if im.size_uninit != want:
        return "SizeOfUninitializedData is 0x%x, section table says 0x%x" % (
            im.size_uninit, want)

def check_uninit_nonzero(im):
    """This image really does have a BSS section, so the field must be set."""
    if not [s for s in im.secs if s['chars'] & UNINIT]:
        return "no IMAGE_SCN_CNT_UNINITIALIZED_DATA section in the image"
    if im.size_uninit == 0:
        return "SizeOfUninitializedData is 0 but the image has a BSS section"

def check_has_symtab(im):
    if not im.symtab or not im.nsyms:
        return "no COFF symbol table (PointerToSymbolTable=0x%x NumberOfSymbols=%d)" % (
            im.symtab, im.nsyms)

def check_debug_not_stripped(im):
    """An image carrying a COFF symbol table may not claim DEBUG_STRIPPED.

    PE Format, Characteristics: IMAGE_FILE_DEBUG_STRIPPED 0x0200 means
    "Debugging information is removed from the image file."
    """
    e = check_has_symtab(im)
    if e:
        return e
    if im.chars & IMAGE_FILE_DEBUG_STRIPPED:
        return ("Characteristics 0x%04x claims DEBUG_STRIPPED, but the image "
                "carries %d COFF symbols at 0x%x"
                % (im.chars, im.nsyms, im.symtab))

def check_debug_stripped(im):
    """Conversely, an image with no symbol table should still say so."""
    if im.symtab or im.nsyms:
        return "unexpected COFF symbol table in a build without -g"
    if not (im.chars & IMAGE_FILE_DEBUG_STRIPPED):
        return "Characteristics 0x%04x does not claim DEBUG_STRIPPED" % im.chars

def check_names_nul_padded(im):
    """Section names are "An 8-byte, null-padded UTF-8 encoded string."

    PE Format, Section Table (Section Headers), Name.
    """
    for s in im.secs:
        n = s['name']
        if b'\0' in n and n[n.index(b'\0'):].strip(b'\0'):
            return "section name %r has residual bytes past the NUL" % n

def check_has_long_names(im):
    if not [s for s in im.secs if s['name'].startswith(b'/')]:
        return "no long ('/<offset>') section names in this image"

CHECKS = dict((k[6:].replace('_', '-'), v) for k, v in sorted(globals().items())
              if k.startswith('check_'))

def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    im = Image(argv[1])
    bad = 0
    for name in argv[2:]:
        fn = CHECKS.get(name)
        if fn is None:
            raise SystemExit("unknown check '%s' (have: %s)"
                             % (name, ' '.join(sorted(CHECKS))))
        err = fn(im)
        if err:
            print("error: %s: %s: %s" % (im.path, name, err))
            bad += 1
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
