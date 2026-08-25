#!/bin/bash
# Axis: build directory path (different lengths). Holds source content identical.
set -u
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
WTCC=/tmp/claude/repos/tcc-scan-repro/x86_64-win32-tcc
PEB=-B/tmp/claude/pe64root
SRC=/tmp/claude/repro/scratch/src.c
A=/tmp/repro_a; B=/tmp/repro_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
rm -rf $A $B; mkdir -p $A $B
cp $SRC $A/src.c; cp $SRC $B/src.c
for d in $A $B; do
  ( cd $d
    $TCC src.c -o out.elf
    $TCC -c src.c -o out.o
    $TCC -g src.c -o outg.elf
    $TCC -g -c src.c -o outg.o
    $WTCC $PEB src.c -o out.exe
    $WTCC $PEB -c src.c -o outpe.o
    $WTCC $PEB -g src.c -o outg.exe
  )
done
for f in out.elf out.o outg.elf outg.o out.exe outpe.o outg.exe; do
  if cmp -s $A/$f $B/$f; then echo "PATH  SAME  $f"; else echo "PATH  DIFF  $f"; fi
done
