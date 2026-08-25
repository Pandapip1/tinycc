#!/bin/bash
set -u
TCC="/tmp/claude/repos/tcc-scan-repro/tcc -B/tmp/claude/repos/tcc-scan-repro"
D=/tmp/repro_date; rm -rf $D; mkdir -p $D; cd $D
printf 'const char *d = __DATE__;\nconst char *t = __TIME__;\n' > d.c
env -i PATH=/usr/bin:/bin TZ=UTC SOURCE_DATE_EPOCH=1000000000 $TCC -E d.c 2>&1 | tail -3
echo "--- TZ=Pacific/Kiritimati (UTC+14) ---"
env -i PATH=/usr/bin:/bin TZ=Pacific/Kiritimati SOURCE_DATE_EPOCH=1000000000 $TCC -E d.c 2>&1 | tail -3
echo "--- TZ=Pacific/Midway (UTC-11) ---"
env -i PATH=/usr/bin:/bin TZ=Pacific/Midway SOURCE_DATE_EPOCH=1000000000 $TCC -E d.c 2>&1 | tail -3
echo "--- object across a clock second ---"
env -i PATH=/usr/bin:/bin TZ=UTC $TCC -c d.c -o d1.o
S=$(date +%s); while [ "$(date +%s)" = "$S" ]; do :; done
env -i PATH=/usr/bin:/bin TZ=UTC $TCC -c d.c -o d2.o
cmp -s d1.o d2.o && echo "DATEMACRO across second: SAME" || echo "DATEMACRO across second: DIFF"
cmp -l d1.o d2.o | head
echo "--- __TIMESTAMP__ support ---"
printf 'const char*x=__TIMESTAMP__;\n' > ts.c
env -i PATH=/usr/bin:/bin $TCC -c ts.c -o ts.o 2>&1 | head -3; echo "rc=$?"
