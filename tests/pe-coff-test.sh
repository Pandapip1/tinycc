#!/bin/sh
# Regression test for the PE-COFF relocatable-object reader (tccpe.c,
# pe_load_obj_file()).  It feeds tcc's win32 linker genuine GNU as output.
#
# It needs an i386 PE assembler and the i386-win32 cross compiler.  When the
# assembler is missing the test skips cleanly, so machines without MinGW
# binutils still get a green suite.  Running the linked program additionally
# needs wine (or a win32 host); when neither is available the link and the
# error-path checks still run, only the value check is skipped.
#
# usage: pe-coff-test.sh <topsrc> <top> <make>

set -e

TOPSRC=${1:-..}
TOP=${2:-..}
MAKE=${3:-make}
AS=${PE_COFF_AS:-i686-w64-mingw32-as}
SRC=$TOPSRC/tests
OUT=pe-coff-tmp

if ! command -v "$AS" >/dev/null 2>&1; then
    echo " . skipped: $AS not found"
    exit 0
fi

rm -rf "$OUT"
mkdir -p "$OUT"

"$MAKE" -C "$TOP" cross-i386-win32 >"$OUT/build.log" 2>&1 || {
    echo " . skipped: could not build the i386-win32 cross compiler"
    sed 's/^/   | /' "$OUT/build.log"
    exit 0
}
TCC="$TOP/i386-win32-tcc"
[ -x "$TCC" ] || TCC="$TOP/i386-win32-tcc.exe"
TCCFLAGS="-B$TOPSRC/win32 -I$TOPSRC/include -L$TOP"

"$AS" -o "$OUT/pe-coff-obj.o" "$SRC/pe-coff-obj.s"
"$AS" -o "$OUT/pe-coff-bad.o" "$SRC/pe-coff-bad.s"

# 1. the object must load, and the resulting program must link
"$TCC" $TCCFLAGS -o "$OUT/pe-coff.exe" "$SRC/pe-coff-main.c" "$OUT/pe-coff-obj.o"
echo " . linked a GNU as pe-i386 object"

# 2. the same object inside an archive, both a la carte and --whole-archive
if command -v i686-w64-mingw32-ar >/dev/null 2>&1; then
    rm -f "$OUT/libpecoff.a"
    i686-w64-mingw32-ar rcs "$OUT/libpecoff.a" "$OUT/pe-coff-obj.o"
    "$TCC" $TCCFLAGS -o "$OUT/pe-coff-ar.exe" "$SRC/pe-coff-main.c" "$OUT/libpecoff.a"
    "$TCC" $TCCFLAGS -o "$OUT/pe-coff-arw.exe" "$SRC/pe-coff-main.c" \
        -Wl,--whole-archive "$OUT/libpecoff.a"
    echo " . linked the same object from a .a archive"
else
    echo " . skipped the archive check: i686-w64-mingw32-ar not found"
fi

# 3. an unsupported relocation type must fail loudly and name the type.
#    Silently accepting it, or passing it through unchanged, is the exact
#    defect this reader is built to prevent.
if "$TCC" $TCCFLAGS -o "$OUT/pe-coff-bad.exe" "$SRC/pe-coff-main.c" \
        "$OUT/pe-coff-bad.o" >"$OUT/bad.log" 2>&1; then
    echo "error: an object with relocation type 7 (DIR32NB) linked successfully"
    exit 1
fi
if ! grep 'unsupported PE-COFF relocation type 7' "$OUT/bad.log" >/dev/null; then
    echo "error: relocation type 7 was rejected, but not with the expected message:"
    sed 's/^/   | /' "$OUT/bad.log"
    exit 1
fi
echo " . rejected an unsupported relocation type, loudly"

# 4. finally, check the relocations actually computed the right values
RUN=
CANRUN=yes
if [ "$OS" = "Windows_NT" ]; then
    :
elif command -v wine >/dev/null 2>&1; then
    RUN=wine
else
    CANRUN=no
    echo " . skipped the value checks: no wine, and this is not a win32 host"
fi

# run_and_check <exe> <expect>
run_and_check() {
    [ "$CANRUN" = yes ] || return 0
    if ! $RUN "$1" >"$OUT/raw.txt" 2>"$OUT/err.txt"; then
        echo " . skipped a value check: could not run $1"
        sed 's/^/   | /' "$OUT/err.txt"
        CANRUN=no
        return 0
    fi
    tr -d '\r' <"$OUT/raw.txt" >"$OUT/out.txt"   # the program writes CRLF
    if diff -u "$2" "$OUT/out.txt"; then
        return 0
    fi
    echo "error: wrong values after relocation, from $1"
    exit 1
}

run_and_check "$OUT/pe-coff.exe" "$SRC/pe-coff-test.expect" &&
    [ "$CANRUN" = no ] || echo " . relocations resolved to the expected values"

# 5. the same, for x86-64.  There is no x86_64-w64-mingw32-as on most Linux
#    boxes, so this leans on clang's integrated assembler when it can target
#    windows-gnu, and is skipped when neither is available.
X64AS=
if [ -n "$PE_COFF_AS64" ]; then
    X64AS=$PE_COFF_AS64
elif command -v x86_64-w64-mingw32-as >/dev/null 2>&1; then
    X64AS=x86_64-w64-mingw32-as
elif command -v clang >/dev/null 2>&1 &&
     clang --target=x86_64-windows-gnu -c "$SRC/pe-coff-obj64.s" \
        -o "$OUT/probe64.o" >/dev/null 2>&1; then
    X64AS="clang --target=x86_64-windows-gnu -c"
fi
if [ -z "$X64AS" ]; then
    echo " . skipped the x86-64 checks: no assembler that emits pe-x86-64"
    exit 0
fi
$X64AS -o "$OUT/pe-coff-obj64.o" "$SRC/pe-coff-obj64.s"
"$MAKE" -C "$TOP" cross-x86_64-win32 >"$OUT/build64.log" 2>&1 || {
    echo " . skipped the x86-64 checks: could not build the cross compiler"
    sed 's/^/   | /' "$OUT/build64.log"
    exit 0
}
TCC64="$TOP/x86_64-win32-tcc"
[ -x "$TCC64" ] || TCC64="$TOP/x86_64-win32-tcc.exe"
"$TCC64" $TCCFLAGS -o "$OUT/pe-coff64.exe" "$SRC/pe-coff-main64.c" \
    "$OUT/pe-coff-obj64.o"
echo " . linked a pe-x86-64 object"
run_and_check "$OUT/pe-coff64.exe" "$SRC/pe-coff-test64.expect" &&
    [ "$CANRUN" = no ] || echo " . x86-64 relocations resolved to the expected values"
