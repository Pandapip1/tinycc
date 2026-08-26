#!/bin/sh
# Regression test for archive member names longer than 15 characters.
#
# The ar_name field of an archive header is 16 bytes and the name stored
# there is terminated by '/', so only 15 characters fit.  tcc used to
# truncate silently, which made two members whose names share a 15-char
# prefix (clock_nanosleep.o and clock_nanosleep_ns.o here) indistinguishable
# and one of them unreachable by name.  Long names now go into the SysV/GNU
# extended name table (the "//" member) and are referenced as "/<offset>".
#
# Checked here: tcc -ar writes them, tcc -ar t/x reads them back, tcc's
# linker still resolves the archive through its armap, short names are still
# stored inline, and - when binutils is around - GNU ar reads the result too.
#
# usage: ar-longname-test.sh "<tcc>" <exesuf>

set -e

TCC=${1:-../tcc}
EXESUF=$2
OUT=ar-longname-tmp

# 'tcc -ar' must come first on the command line and needs no -B/-I, so the
# bare binary is used for it
set -- $TCC
TCCBIN=$1
case $TCCBIN in /*) ;; *) TCCBIN=`pwd`/$TCCBIN;; esac

rm -rf "$OUT"
mkdir -p "$OUT/x"

fail() { echo "error: $*"; exit 1; }

cat >"$OUT/clock_nanosleep.c" <<'EOF'
int clock_nanosleep_fn(void) { return 1; }
EOF
cat >"$OUT/clock_nanosleep_ns.c" <<'EOF'
int clock_nanosleep_ns_fn(void) { return 2; }
EOF
cat >"$OUT/short.c" <<'EOF'
int short_fn(void) { return 4; }
EOF
cat >"$OUT/main.c" <<'EOF'
int clock_nanosleep_fn(void);
int clock_nanosleep_ns_fn(void);
int short_fn(void);
int main(void)
{
    return clock_nanosleep_fn() + clock_nanosleep_ns_fn() + short_fn() == 7
           ? 0 : 1;
}
EOF

# give the members different sizes, so that a name resolving to the wrong
# member shows up in the extracted files
i=0
while [ $i -lt 40 ]; do
    echo "int clock_nanosleep_ns_pad$i(void) { return $i; }" \
        >>"$OUT/clock_nanosleep_ns.c"
    i=`expr $i + 1`
done

$TCC -c "$OUT/clock_nanosleep.c" -o "$OUT/clock_nanosleep.o"
$TCC -c "$OUT/clock_nanosleep_ns.c" -o "$OUT/clock_nanosleep_ns.o"
$TCC -c "$OUT/short.c" -o "$OUT/short.o"
"$TCCBIN" -ar rcs "$OUT/libtest.a" "$OUT/clock_nanosleep.o" \
    "$OUT/clock_nanosleep_ns.o" "$OUT/short.o"

# 1. tcc -ar t must list the full names, all of them different
"$TCCBIN" -ar t "$OUT/libtest.a" >"$OUT/t.out"
cat >"$OUT/t.expect" <<'EOF'
clock_nanosleep.o
clock_nanosleep_ns.o
short.o
EOF
cmp -s "$OUT/t.out" "$OUT/t.expect" || {
    echo "--- got ---"; cat "$OUT/t.out"
    echo "--- expected ---"; cat "$OUT/t.expect"
    fail "tcc -ar t does not list the full member names"
}
echo " . tcc -ar t lists the full member names"

# 2. tcc -ar x must extract every member, with its own contents
cp "$OUT/libtest.a" "$OUT/x/"
(cd "$OUT/x" && "$TCCBIN" -ar x libtest.a)
for f in clock_nanosleep.o clock_nanosleep_ns.o short.o; do
    cmp "$OUT/$f" "$OUT/x/$f" || fail "extracted $f differs"
done
echo " . tcc -ar x extracts every member under its own name"

# the armap size lives in the ar_size field of the first header, at file
# offset 8 + 48; the next header starts after it, at an even offset
armapend() {
    size=`dd if="$1" bs=1 skip=56 count=10 2>/dev/null | tr -d ' '`
    end=`expr 68 + $size`
    case $end in *[13579]) end=`expr $end + 1`;; esac
    echo $end
}

# 3. short names stay inline: an archive that has none of them long must
#    hold the name itself where the second header starts
"$TCCBIN" -ar rcs "$OUT/libshort.a" "$OUT/short.o"
name=`dd if="$OUT/libshort.a" bs=1 skip=\`armapend "$OUT/libshort.a"\` \
      count=8 2>/dev/null`
[ "$name" = "short.o/" ] \
    || fail "short names are not stored inline (got '$name')"
echo " . short names are still stored inline"

# 4. and with a long name around, that is where the "//" member sits
name=`dd if="$OUT/libtest.a" bs=1 skip=\`armapend "$OUT/libtest.a"\` \
      count=2 2>/dev/null`
[ "$name" = "//" ] || fail "no extended name table in the archive"
echo " . the archive carries an extended name table"

# 5. tcc's linker still resolves the archive through its armap
$TCC -o "$OUT/prog$EXESUF" "$OUT/main.c" "$OUT/libtest.a"
"$OUT/prog$EXESUF" || fail "linking through the armap gave the wrong members"
echo " . tcc links the archive through its armap"

# 6. GNU ar reads what tcc wrote, if it is installed
if command -v ar >/dev/null 2>&1 &&
   ar t "$OUT/libtest.a" >"$OUT/gnu.out" 2>/dev/null; then
    cmp -s "$OUT/gnu.out" "$OUT/t.expect" || {
        echo "--- got ---"; cat "$OUT/gnu.out"
        echo "--- expected ---"; cat "$OUT/t.expect"
        fail "GNU ar t does not list the full member names"
    }
    echo " . GNU ar t lists the full member names"
else
    echo " . skipped: no GNU ar"
fi
