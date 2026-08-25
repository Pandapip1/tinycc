#!/bin/sh
# Regression test for reproducible output.
#
#  - SOURCE_DATE_EPOCH must replace the current time in __DATE__/__TIME__,
#    must be interpreted in UTC and must be rejected when malformed
#    (https://reproducible-builds.org/specs/source-date-epoch/)
#  - -f...-prefix-map= must actually remap the pathnames tcc records
#  - the linker options that only ask for what tcc already does must not
#    be an error, the ones asking for more must stay one
#
# usage: reprotest.sh <tcc> <topsrc>

set -e

TCC=${1:-../tcc -B..}
TOPSRC=${2:-..}
OUT=repro-tmp
FAIL=0

rm -rf "$OUT"
mkdir -p "$OUT"

fail() {
    echo " ! FAILED: $*"
    FAIL=1
}

cat > "$OUT/dt.c" <<'EOF'
const char *tcc_date = __DATE__;
const char *tcc_time = __TIME__;
const char *tcc_file = __FILE__;
int main(void) { return 0; }
EOF

# ---------------------------------------------------------------- __DATE__

# 1000000000 is 2001-09-09 01:46:40 UTC
SDE=1000000000
export SOURCE_DATE_EPOCH=$SDE

TZ=UTC $TCC -c "$OUT/dt.c" -o "$OUT/a1.o"
# cross a real clock second, so that a __TIME__ taken from time() differs
s=`date +%S`
while [ "`date +%S`" = "$s" ]; do :; done
# ... and use a different timezone, so that a __DATE__ taken from localtime()
# differs too
TZ=Pacific/Midway $TCC -c "$OUT/dt.c" -o "$OUT/a2.o"

cmp "$OUT/a1.o" "$OUT/a2.o" \
    || fail "SOURCE_DATE_EPOCH: output differs across time and TZ"

strings -a "$OUT/a1.o" > "$OUT/a1.str" 2>/dev/null || cp "$OUT/a1.o" "$OUT/a1.str"
grep -a -q 'Sep  9 2001' "$OUT/a1.str" \
    || fail "SOURCE_DATE_EPOCH: __DATE__ is not the UTC date of $SDE"
grep -a -q '01:46:40' "$OUT/a1.str" \
    || fail "SOURCE_DATE_EPOCH: __TIME__ is not the UTC time of $SDE"

# a malformed value "SHOULD exit with a non-zero error code"
for bad in xyz -1 ' 12' 12.5 99999999999999999999; do
    if SOURCE_DATE_EPOCH="$bad" $TCC -c "$OUT/dt.c" -o "$OUT/bad.o" 2>/dev/null; then
        fail "SOURCE_DATE_EPOCH='$bad' was accepted"
    fi
done
unset SOURCE_DATE_EPOCH

# an unset variable keeps the old behaviour: a date is still produced
$TCC -c "$OUT/dt.c" -o "$OUT/a3.o"

if [ $FAIL = 0 ]; then echo " . SOURCE_DATE_EPOCH OK"; fi

# -------------------------------------------------------------- prefix maps

here=`pwd`
D="$here/$OUT"

$TCC -g -c "$D/dt.c" -o "$D/g1.o"
grep -a -q "$here" "$D/g1.o" || fail "test bug: no build path in -g output"

$TCC -g -ffile-prefix-map="$here"=/RE -c "$D/dt.c" -o "$D/g2.o"
! grep -a -q "$here" "$D/g2.o" \
    || fail "-ffile-prefix-map: build path still in the debug info"
grep -a -q "/RE" "$D/g2.o" || fail "-ffile-prefix-map: nothing was remapped"

for dw in -gdwarf-4 -gdwarf-5 -gstabs; do
    $TCC $dw -ffile-prefix-map="$here"=/RE -c "$D/dt.c" -o "$D/g3.o"
    ! grep -a -q "$here" "$D/g3.o" \
        || fail "-ffile-prefix-map: build path still in $dw output"
done

# __FILE__ is -fmacro-prefix-map, not -fdebug-prefix-map
$TCC -fmacro-prefix-map="$here"=/RE -c "$D/dt.c" -o "$D/m1.o"
grep -a -q "/RE/$OUT/dt.c" "$D/m1.o" || fail "-fmacro-prefix-map: __FILE__ not remapped"
$TCC -fdebug-prefix-map="$here"=/RE -c "$D/dt.c" -o "$D/m2.o"
grep -a -q "$here/$OUT/dt.c" "$D/m2.o" || fail "-fdebug-prefix-map: __FILE__ was remapped"

# -ftest-coverage records the build directory even without -g
if $TCC -ftest-coverage "$D/dt.c" -o "$D/cov1" 2>/dev/null; then
    grep -a -q "$here" "$D/cov1" || fail "test bug: no build path in -ftest-coverage output"
    $TCC -ftest-coverage -ffile-prefix-map="$here"=/RE "$D/dt.c" -o "$D/cov2"
    ! grep -a -q "$here" "$D/cov2" \
        || fail "-ffile-prefix-map: build path still in the -ftest-coverage data"
else
    echo " . skipped: cannot link -ftest-coverage here"
fi

# a mapping without '=' is an error, not a silent no-op
if $TCC -ffile-prefix-map=nosuchthing -c "$D/dt.c" -o "$D/e.o" 2>/dev/null; then
    fail "-ffile-prefix-map without OLD=NEW was accepted"
fi
# and an unimplemented -f option is still not accepted silently
$TCC -Wunsupported -c "$D/dt.c" -o "$D/e.o" -fno-such-option 2>"$D/w.txt" || true
grep -q "unsupported option" "$D/w.txt" || fail "-Wunsupported no longer warns"

if [ $FAIL = 0 ]; then echo " . prefix maps OK"; fi

# ----------------------------------------------------------- linker options

if $TCC "$D/dt.c" -o "$D/l0" 2>/dev/null; then
    for opt in --build-id=none --no-insert-timestamp --hash-style=sysv; do
        $TCC -Wl,$opt "$D/dt.c" -o "$D/l1" \
            || fail "-Wl,$opt is still an error"
        cmp "$D/l0" "$D/l1" || fail "-Wl,$opt changed the output"
    done
    for opt in --build-id --build-id=sha1 --insert-timestamp \
               --hash-style=gnu --hash-style=both --sort-common; do
        if $TCC -Wl,$opt "$D/dt.c" -o "$D/l2" 2>/dev/null; then
            fail "-Wl,$opt was accepted although tcc does not do it"
        fi
    done
    if [ $FAIL = 0 ]; then echo " . linker options OK"; fi
else
    echo " . skipped: cannot link here"
fi

[ $FAIL = 0 ] || exit 1
echo "Reproducibility Test OK"
