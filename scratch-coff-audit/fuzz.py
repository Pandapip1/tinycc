#!/usr/bin/env python3
"""Structure-aware mutational fuzzer for tcc's PE-COFF object reader.

Mutates the *metadata* regions of a valid COFF object (file header, section
table, symbol table, relocation records, string-table size word) rather than
random file bytes, since that is where the untrusted-integer surface lives.
Reports any run whose exit status is a signal (rc >= 128) or an ASAN report.
"""
import os, random, struct, subprocess, sys, shutil, time

import coff
AR_WRAP = bool(int(os.environ.get('AR_WRAP', '0')))

SEED, TCC, ARGS, N, OUT = sys.argv[1], sys.argv[2], sys.argv[3].split(), int(sys.argv[4]), sys.argv[5]
os.makedirs(OUT, exist_ok=True)
base = bytearray(open(SEED, 'rb').read())
o = coff.Obj(SEED)

# --- interesting metadata byte ranges (offset, width) ---------------------
FIELDS = []
def add(off, w): FIELDS.append((off, w))
for w, off in ((2,2),(4,8),(4,12),(2,16)):        # nscns, symptr, nsyms, opthdr
    add(off, w)
for i in range(1, o.nsec+1):
    sh = o.sh(i)
    for k in range(0, 8): add(sh+k, 1)             # name bytes
    for off, w in ((8,4),(12,4),(16,4),(20,4),(24,4),(32,2),(36,4)):
        add(sh+off, w)
for k in range(o.nsyms):
    sy = o.sym(k)
    for off, w in ((0,4),(4,4),(8,4),(12,2),(16,1),(17,1)):
        add(sy+off, w)
for i in range(1, o.nsec+1):
    for j in range(o.nreloc(i)):
        r = o.rel(i, j)
        for off, w in ((0,4),(4,4),(8,2)):
            add(r+off, w)
# string table size word
if o.symptr:
    add(o.symptr + o.nsyms*coff.SYMESZ, 4)
FIELDS = [(a, w) for (a, w) in FIELDS if a + w <= len(base)]

VALS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 14, 0x7f, 0x80, 0xff, 0x100, 0x1000,
        0x7fff, 0x8000, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe,
        0xffffffff, 0x1000000, 0x00f00000, 0x01000000]

def mutate(rnd):
    b = bytearray(base)
    for _ in range(rnd.randint(1, 4)):
        if rnd.random() < 0.85 and FIELDS:
            a, w = rnd.choice(FIELDS)
            v = rnd.choice(VALS)
            for k in range(w):
                b[a+k] = (v >> (8*k)) & 0xff
        else:
            a = rnd.randrange(len(b))
            b[a] ^= 1 << rnd.randrange(8)
    if rnd.random() < 0.10:                 # truncate: exercises load_data()
        b = b[:rnd.randrange(20, len(b))]
    if AR_WRAP:                             # wrap as a 1-member ar archive
        d = bytes(b)
        hdr = ('m.o/'.ljust(16) + '0'.ljust(12) + '0'.ljust(6) + '0'.ljust(6)
               + '644'.ljust(8) + str(len(d)).ljust(10) + '`\n').encode()
        b = bytearray(b'!<arch>\n' + hdr + d + (b'\n' if len(d) % 2 else b''))
    return b

# max_allocation_size_mb + allocator_may_return_null keep the huge-allocation
# paths (already characterised separately) from OOM-killing the fuzz cgroup;
# tcc turns the NULL back into its own 'memory full' error.
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=0:exitcode=86:'
                                    'handle_segv=1:log_path=stderr:'
                                    'allocator_may_return_null=1:max_allocation_size_mb=400')
rnd = random.Random(int(os.environ.get('FUZZ_SEED', '1')))
tmp = os.path.join(OUT, 'cur.a' if AR_WRAP else 'cur.o')
found = 0
t0 = time.time()
for n in range(N):
    b = mutate(rnd)
    with open(tmp, 'wb') as f:
        f.write(bytes(b))
    try:
        p = subprocess.run([TCC] + ARGS + [tmp], capture_output=True, timeout=10, env=env)
        rc, err = p.returncode, p.stderr
    except subprocess.TimeoutExpired:
        rc, err = -999, b'TIMEOUT'
    bad = rc >= 128 or rc < 0 or rc == 86 or b'ERROR: AddressSanitizer' in err
    if bad:
        found += 1
        name = os.path.join(OUT, 'crash_%05d_rc%s.o' % (n, rc))
        shutil.copy(tmp, name)
        with open(name + '.txt', 'wb') as f:
            f.write(b'rc=%d\n' % rc + err[:4000])
        print('[%d] rc=%s -> %s' % (n, rc, name), flush=True)
        print(err[:600].decode('utf8', 'replace'), flush=True)
print('done %d runs, %d findings, %.0fs' % (N, found, time.time()-t0), flush=True)
