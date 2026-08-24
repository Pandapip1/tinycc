#!/bin/sh
# Diagnosis harness for the reported "tcc i386 ELF -> pe-i386 objcopy" failure.
#
# Finding: objcopy does NOT translate relocation types when converting between
# ELF and PE-COFF.  bfd/coff-i386.c:367 and bfd/coff-x86_64.c:502 both define
#     SELECT_RELOC(x,howto) { x.r_type = howto->type; }
# and bfd/coffcode.h:2700-2707 uses that to write the reloc.  `howto' here is
# still the *ELF* input backend's howto, so the ELF r_type number is written
# verbatim into the COFF reloc record.
#
# On x86_64 the ELF numbers happen to hit populated slots of coff-x86_64.c's
# howto table, so nothing errors -- but the meanings are wrong
# (R_X86_64_PC32 = 2 becomes R_AMD64_DIR32 = 2, absolute instead of relative).
# On i386, COFF types 0..5 are EMPTY_HOWTO (bfd/coff-i386.c:206-211), so
# R_386_32 = 1 and R_386_PC32 = 2 are simply illegal and every reader rejects
# the result.
#
# This reproduces identically for gcc -m32 output, so it is not a tcc bug.
#
# Usage: elf2pe-objcopy-repro.sh <path-to-i386-tcc> <path-to-x86_64-tcc> <tcc-srcdir>
set -e
I386_TCC=${1:?}; X64_TCC=${2:?}; B=${3:?}
d=$(mktemp -d); trap 'rm -rf "$d"' 0
cat > "$d/t.c" <<'CEOF'
extern int __ctype_b[];
static const char msg[] = "hi";
static int helper(int c) { return c + msg[0]; }
int isalnum(int c) { return helper(c) + __ctype_b[c]; }
CEOF
"$I386_TCC" -c "$d/t.c" -o "$d/tcc32.o" -B"$B"
"$X64_TCC"  -c "$d/t.c" -o "$d/tcc64.o" -B"$B"
gcc -m32 -c "$d/t.c" -o "$d/gcc32.o"
gcc      -c "$d/t.c" -o "$d/gcc64.o"

for pair in "tcc32 elf32-i386 pe-i386" "gcc32 elf32-i386 pe-i386" \
            "tcc64 elf64-x86-64 pe-x86-64" "gcc64 elf64-x86-64 pe-x86-64"; do
  set -- $pair
  echo "===== $1: $2 -> $3"
  objcopy -I "$2" -O "$3" "$d/$1.o" "$d/$1-coff.o" && echo "objcopy: ok"
  # The real test is whether the result can be read back.
  objdump -r "$d/$1-coff.o" || echo "  ^^ output is not a readable COFF object"
done
