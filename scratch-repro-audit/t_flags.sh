#!/bin/bash
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
WTCC="/tmp/claude/repos/tcc-scan-repro/x86_64-win32-tcc -B/tmp/claude/pe64root"
D=/tmp/repro_flags; rm -rf $D; mkdir -p $D; cd $D
cp /tmp/claude/repro/scratch/src.c s.c
$TCC s.c -o ref.elf; $WTCC s.c -o ref.exe
for f in "-Wl,--build-id=none" "-Wl,--build-id" "-frandom-seed=1234" "-ffile-prefix-map=/tmp=/x" "-fdebug-prefix-map=/tmp=/x" "-fmacro-prefix-map=/tmp=/x" "-Wl,--sort-common" "-Wl,--hash-style=gnu" "-Wl,--no-insert-timestamp" "-Wno-date-time" "-Wdate-time"; do
  out=$($TCC $f s.c -o f.elf 2>&1); rc=$?
  if [ $rc -ne 0 ]; then st="FATAL rc=$rc: $out"
  elif [ -n "$out" ]; then st="WARN: $out"
  elif cmp -s ref.elf f.elf; then st="ACCEPTED-noeffect(identical output)"
  else st="ACCEPTED-changes-output"; fi
  printf 'ELF  %-30s %s\n' "$f" "$st"
  out=$($WTCC $f s.c -o f.exe 2>&1); rc=$?
  if [ $rc -ne 0 ]; then st="FATAL rc=$rc: $out"
  elif [ -n "$out" ]; then st="WARN: $out"
  elif cmp -s ref.exe f.exe; then st="ACCEPTED-noeffect(identical output)"
  else st="ACCEPTED-changes-output"; fi
  printf 'PE   %-30s %s\n' "$f" "$st"
done
