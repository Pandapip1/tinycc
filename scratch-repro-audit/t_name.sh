#!/bin/bash
set -u
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
WTCC="/tmp/claude/repos/tcc-scan-repro/x86_64-win32-tcc -B/tmp/claude/pe64root"
D=/tmp/repro_name; rm -rf $D; mkdir -p $D; cd $D
cp /tmp/claude/repro/scratch/src.c aa.c
cp /tmp/claude/repro/scratch/src.c bbbbbbbbbbbbbbbb.c
for n in aa bbbbbbbbbbbbbbbb; do
  $TCC   -c $n.c -o $n.o;   $TCC   $n.c -o $n.elf
  $TCC   -g -c $n.c -o $n.g.o
  $WTCC  -c $n.c -o $n.pe.o; $WTCC $n.c -o $n.exe
done
echo "NAME -c ELF:  $(cmp -s aa.o bbbbbbbbbbbbbbbb.o && echo SAME || echo DIFF)"
echo "NAME link ELF:$(cmp -s aa.elf bbbbbbbbbbbbbbbb.elf && echo SAME || echo DIFF)"
echo "NAME -g -c:   $(cmp -s aa.g.o bbbbbbbbbbbbbbbb.g.o && echo SAME || echo DIFF)"
echo "NAME -c PE:   $(cmp -s aa.pe.o bbbbbbbbbbbbbbbb.pe.o && echo SAME || echo DIFF)"
echo "NAME link PE: $(cmp -s aa.exe bbbbbbbbbbbbbbbb.exe && echo SAME || echo DIFF)"
# absolute vs relative path, no -g
$TCC -c $D/aa.c -o abs.o; $TCC -c aa.c -o rel.o
echo "ABSPATH -c ELF nog: $(cmp -s abs.o rel.o && echo SAME || echo DIFF)"
$WTCC -c $D/aa.c -o abs.pe.o; $WTCC -c aa.c -o rel.pe.o
echo "ABSPATH -c PE  nog: $(cmp -s abs.pe.o rel.pe.o && echo SAME || echo DIFF)"
echo "--- leaked strings in non-g ELF .o ---"
command grep -a -o 'aa\.c\|/tmp/repro_name' aa.o | sort -u
echo "--- leaked strings in non-g PE .o ---"
command grep -a -o 'aa\.c\|/tmp/repro_name' aa.pe.o | sort -u
