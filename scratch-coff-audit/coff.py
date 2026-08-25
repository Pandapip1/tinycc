"""Minimal PE-COFF object accessors for the tcc pe_load_obj_file audit."""
import struct
FILHSZ, SCNHSZ, SYMESZ, RELSZ = 20, 40, 18, 10
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def s16(b,o): return struct.unpack_from('<h',b,o)[0]
def p16(b,o,v): struct.pack_into('<H',b,o,v & 0xffff)
def p32(b,o,v): struct.pack_into('<I',b,o,v & 0xffffffff)
class Obj:
    def __init__(self, path):
        self.b = bytearray(open(path,'rb').read())
    @property
    def nsec(self): return u16(self.b, 2)
    @property
    def symptr(self): return u32(self.b, 8)
    @property
    def nsyms(self): return u32(self.b, 12)
    @property
    def opthdr(self): return u16(self.b, 16)
    def sh(self, i):  # 1-based
        return FILHSZ + self.opthdr + (i-1)*SCNHSZ
    def sname(self, i):
        return bytes(self.b[self.sh(i):self.sh(i)+8]).rstrip(b'\0')
    def rawsize(self, i): return u32(self.b, self.sh(i)+16)
    def rawptr(self, i):  return u32(self.b, self.sh(i)+20)
    def relptr(self, i):  return u32(self.b, self.sh(i)+24)
    def nreloc(self, i):  return u16(self.b, self.sh(i)+32)
    def flags(self, i):   return u32(self.b, self.sh(i)+36)
    def rel(self, i, j):  return self.relptr(i) + j*RELSZ
    def sym(self, k):     return self.symptr + k*SYMESZ
    def save(self, path): open(path,'wb').write(bytes(self.b))
    def dump(self):
        print("machine=%04x nsec=%d nsyms=%d symptr=%d opthdr=%d size=%d"
              % (u16(self.b,0), self.nsec, self.nsyms, self.symptr, self.opthdr, len(self.b)))
        for i in range(1, self.nsec+1):
            print("  sec %d %-10s rawsize=%#x rawptr=%#x relptr=%#x nrel=%d flags=%08x"
                  % (i, self.sname(i).decode('latin1'), self.rawsize(i), self.rawptr(i),
                     self.relptr(i), self.nreloc(i), self.flags(i)))
            for j in range(self.nreloc(i)):
                r = self.rel(i,j)
                print("     rel %d vaddr=%#x sym=%d type=%d" % (j, u32(self.b,r), u32(self.b,r+4), u16(self.b,r+8)))
