#!/bin/bash
# Fuzz campaign: every (seed, invocation-mode) pair, N inputs each.
set -u
A=/tmp/claude/tcc-asan
N=${N:-20000}
cd "$(dirname "$0")"
run() { FUZZ_SEED=$5 AR_WRAP=${6:-0} python3 fuzz.py "$2" "$3" "$4" "$N" "c_$1" > "clog_$1.txt" 2>&1; }
run a32c   seed32.o   $A/i386-win32-tcc   "-c -o /tmp/fz_a.o"                        11 &
run b32r   seed32.o   $A/i386-win32-tcc   "-r -o /tmp/fz_b.o"                        12 &
run c32cd  comdat32.o $A/i386-win32-tcc   "-r -o /tmp/fz_c.o"                        13 &
run d32ar  comdat32.o $A/i386-win32-tcc   "-r -o /tmp/fz_d.o -Wl,--whole-archive"    14 1 &
run e64c   fix64.o    $A/x86_64-win32-tcc "-B/tmp/claude/pe64root -c -o /tmp/fz_e.o" 15 &
run f64r   fix64.o    $A/x86_64-win32-tcc "-B/tmp/claude/pe64root -r -o /tmp/fz_f.o" 16 &
run g64cd  comdat64.o $A/x86_64-win32-tcc "-B/tmp/claude/pe64root -r -o /tmp/fz_g.o" 17 &
run h64ar  comdat64.o $A/x86_64-win32-tcc "-B/tmp/claude/pe64root -r -o /tmp/fz_h.o -Wl,--whole-archive" 18 1 &
run i64s   seed64.o   $A/x86_64-win32-tcc "-B/tmp/claude/pe64root -r -o /tmp/fz_i.o" 19 &
wait
