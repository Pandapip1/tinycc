#!/bin/bash
set -u
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
WTCC="/tmp/claude/repos/tcc-scan-repro/x86_64-win32-tcc -B/tmp/claude/pe64root"
D=/tmp/repro_env; rm -rf $D; mkdir -p $D/t; cd $D/t
cp /tmp/claude/repro/scratch/src.c s.c
build() { # $1 = tag ; env already set by caller
  $TCC -c s.c -o $1.o; $TCC s.c -o $1.elf; $TCC -g s.c -o $1.g.elf
  $WTCC -c s.c -o $1.pe.o; $WTCC s.c -o $1.exe
}
chk() { for e in o elf g.elf pe.o exe; do
   if cmp -s $1.$e $2.$e; then echo "$3  SAME  $e"; else echo "$3  DIFF  $e"; fi; done; }

env -i PATH=/usr/bin:/bin HOME=/root bash -c "cd $D/t; $(declare -f build); TCC=\"$TCC\"; WTCC=\"$WTCC\"; build base" >/dev/null 2>&1
S1=$(date +%s)
while [ "$(date +%s)" = "$S1" ]; do :; done
S2=$(date +%s); echo "clock advanced $S1 -> $S2"
env -i PATH=/usr/bin:/bin HOME=/root bash -c "cd $D/t; $(declare -f build); TCC=\"$TCC\"; WTCC=\"$WTCC\"; build later" >/dev/null 2>&1
chk base later TIME

for v in "TZ=Pacific/Kiritimati" "LC_ALL=tr_TR.UTF-8" "LANG=de_DE.UTF-8" "HOSTNAME=zzzzzzz" "USER=nobodyxyz" "TMPDIR=/tmp/reprotmpdir" "SOURCE_DATE_EPOCH=1000000000" "PWD=/nonsense"; do
  tag=$(echo $v | tr -cd 'A-Za-z0-9')
  mkdir -p /tmp/reprotmpdir
  env -i PATH=/usr/bin:/bin HOME=/root "$v" bash -c "cd $D/t; $(declare -f build); TCC=\"$TCC\"; WTCC=\"$WTCC\"; build $tag" >/dev/null 2>&1
  chk base $tag "ENV[${v%%=*}]"
done
