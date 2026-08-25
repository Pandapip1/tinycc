#!/bin/sh
# Regression test for the PE image header tcc writes (tccpe.c, pe_write()).
#
# Everything here is checked against the emitted bytes by an independent
# parser (tests/pe-header-check.py) rather than against tcc's own idea of
# what it wrote.  No Windows and no wine are needed: these are structural
# properties of the file, not of the loader.
#
# It needs python3 and the win32 cross compilers; when either is missing the
# test skips cleanly, so a machine without them still gets a green suite.
#
# usage: pe-header-test.sh <topsrc> <top> <make>

set -e

TOPSRC=${1:-..}
TOP=${2:-..}
MAKE=${3:-make}
SRC=$TOPSRC/tests
OUT=pe-header-tmp
CHECK="$SRC/pe-header-check.py"

if ! command -v python3 >/dev/null 2>&1; then
    echo " . skipped: python3 not found"
    exit 0
fi

rm -rf "$OUT"
mkdir -p "$OUT"

TCCFLAGS="-B$TOPSRC/win32 -I$TOPSRC/include -L$TOP"

cat >"$OUT/bss.c" <<'EOF'
int bigbss[200000];
int main(void) { bigbss[5] = 1; return bigbss[5]; }
EOF
cat >"$OUT/hello.c" <<'EOF'
int main(void) { return 0; }
EOF

fail() { echo "error: $*"; exit 1; }

for target in i386-win32 x86_64-win32; do
    "$MAKE" -C "$TOP" cross-$target >"$OUT/build.log" 2>&1 || {
        echo " . skipped $target: could not build the cross compiler"
        sed 's/^/   | /' "$OUT/build.log"
        continue
    }
    TCC="$TOP/$target-tcc"
    [ -x "$TCC" ] || TCC="$TOP/$target-tcc.exe"

    # 1. SizeOfUninitializedData must describe the .bss that is really there.
    "$TCC" $TCCFLAGS -o "$OUT/bss.exe" "$OUT/bss.c"
    python3 "$CHECK" "$OUT/bss.exe" uninit-nonzero uninit-matches-bss \
        || fail "$target: SizeOfUninitializedData does not describe the image"
    echo " . $target: SizeOfUninitializedData describes the .bss section"

    # 2. an image without -g claims DEBUG_STRIPPED and has no symbol table;
    #    an image with -g has one, and so may not claim it.
    "$TCC" $TCCFLAGS -o "$OUT/hello.exe" "$OUT/hello.c"
    python3 "$CHECK" "$OUT/hello.exe" debug-stripped names-nul-padded \
        || fail "$target: header is wrong for a build without -g"
    "$TCC" $TCCFLAGS -g -o "$OUT/hellog.exe" "$OUT/hello.c"
    python3 "$CHECK" "$OUT/hellog.exe" debug-not-stripped names-nul-padded \
        || fail "$target: -g image still claims DEBUG_STRIPPED"
    echo " . $target: DEBUG_STRIPPED tracks the COFF symbol table"

    # 3. long section names are "/<offset>", NUL-padded to 8 bytes, with no
    #    tail of the real name left over.
    "$TCC" $TCCFLAGS -gdwarf -o "$OUT/dwarf.exe" "$OUT/hello.c"
    python3 "$CHECK" "$OUT/dwarf.exe" has-long-names names-nul-padded \
        || fail "$target: long section names are not NUL-padded"
    echo " . $target: long section names are NUL-padded"

    # 4. alignments the layout could not honour are rejected, and rejected
    #    before any output file is created.  Values tcc can honour are not.
    for bad in --file-alignment=300 --section-alignment=3000 \
               --file-alignment=2000; do
        rm -f "$OUT/bad.exe"
        if "$TCC" $TCCFLAGS -o "$OUT/bad.exe" "$OUT/hello.c" -Wl,$bad \
                >"$OUT/bad.log" 2>&1; then
            fail "$target: -Wl,$bad was accepted"
        fi
        grep -q 'alignment' "$OUT/bad.log" \
            || fail "$target: -Wl,$bad failed without naming the alignment"
        if [ -e "$OUT/bad.exe" ]; then
            fail "$target: -Wl,$bad left an output file behind"
        fi
    done
    for ok in --file-alignment=200 --section-alignment=1000 \
              --file-alignment=1000; do
        "$TCC" $TCCFLAGS -o "$OUT/ok.exe" "$OUT/hello.c" -Wl,$ok \
            || fail "$target: -Wl,$ok was rejected"
    done
    echo " . $target: invalid section/file alignments are rejected"

    # the pre-existing sub-page warning must still fire
    "$TCC" $TCCFLAGS -o "$OUT/sub.exe" "$OUT/hello.c" \
        -Wl,--section-alignment=200 -Wl,--file-alignment=200 \
        >"$OUT/sub.log" 2>&1 || fail "$target: sub-page alignment now fails"
    grep -q 'below the page size' "$OUT/sub.log" \
        || fail "$target: the sub-page section-alignment warning stopped firing"
    echo " . $target: the sub-page alignment warning still fires"
done
