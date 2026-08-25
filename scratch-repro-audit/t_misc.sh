#!/bin/bash
set -u
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
WTCC="/tmp/claude/repos/tcc-scan-repro/x86_64-win32-tcc -B/tmp/claude/pe64root"
D=/tmp/repro_misc; rm -rf $D; mkdir -p $D; cd $D
cp /tmp/claude/repro/scratch/src.c s.c; cp /tmp/claude/repro/scratch/b.c b.c; cp /tmp/claude/repro/scratch/c.c c.c
echo "=== ASLR ==="
setarch -R $TCC s.c -o aslroff.elf 2>/dev/null || echo "setarch unavailable"
$TCC s.c -o aslron.elf
cmp -s aslroff.elf aslron.elf && echo "ASLR ELF: SAME" || echo "ASLR ELF: DIFF"
setarch -R $TCC -g -c s.c -o aslroff.g.o 2>/dev/null; $TCC -g -c s.c -o aslron.g.o
cmp -s aslroff.g.o aslron.g.o && echo "ASLR -g .o: SAME" || echo "ASLR -g .o: DIFF"
setarch -R $WTCC s.c -o aslroff.exe 2>/dev/null; $WTCC s.c -o aslron.exe
cmp -s aslroff.exe aslron.exe && echo "ASLR PE: SAME" || echo "ASLR PE: DIFF"
echo "=== umask (mode only) ==="
(umask 022; $TCC s.c -o um22.elf); (umask 077; $TCC s.c -o um77.elf)
stat -c '%a %n' um22.elf um77.elf
cmp -s um22.elf um77.elf && echo "umask CONTENT: SAME" || echo "umask CONTENT: DIFF"
echo "=== link input order ==="
$TCC -c s.c -o s.o; $TCC -c b.c -o b.o; $TCC -c c.c -o c.o
$TCC s.o b.o c.o -o ord1.elf; $TCC s.o b.o c.o -o ord1b.elf
$TCC s.o c.o b.o -o ord2.elf
cmp -s ord1.elf ord1b.elf && echo "same-order repeat: SAME" || echo "same-order repeat: DIFF"
cmp -s ord1.elf ord2.elf && echo "swapped-order: SAME" || echo "swapped-order: DIFF"
echo "=== -c batch vs individual ==="
mkdir -p m1 m2; cp s.c b.c c.c m1/; cp s.c b.c c.c m2/
(cd m1 && $TCC -c s.c b.c c.c)
(cd m2 && for f in s b c; do $TCC -c $f.c; done)
for f in s b c; do cmp -s m1/$f.o m2/$f.o && echo "batch-vs-individual $f.o: SAME" || echo "batch-vs-individual $f.o: DIFF"; done
echo "=== tcc -ar determinism ==="
rm -f a1.a a2.a
$TCC -ar rcs a1.a s.o b.o c.o
S=$(date +%s); while [ "$(date +%s)" = "$S" ]; do :; done
$TCC -ar rcs a2.a s.o b.o c.o
cmp -s a1.a a2.a && echo "tcc -ar across second: SAME" || { echo "tcc -ar across second: DIFF"; cmp -l a1.a a2.a | head; }
echo "-- ar header of a1.a --"; head -c 120 a1.a | od -c | head -6
