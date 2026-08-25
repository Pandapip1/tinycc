# PE-COFF object reader audit (commit 54e9b8aa / merge 67c98c8c)

Reconnaissance only - no compiler source is changed by anything in this
directory.  Everything here reproduces findings against `pe_load_obj_file()`
in `tccpe.c`.

## Build

    ./configure && make -j4 && make cross-i386-win32 && make cross-x86_64-win32

For the heap-overflow finding an AddressSanitizer build is needed (the overread
is only 8 bytes and does not reliably fault):

    ./configure --extra-cflags="-O1 -g -fsanitize=address -fno-omit-frame-pointer" \
                --extra-ldflags="-fsanitize=address"
    make cross-i386-win32 cross-x86_64-win32

## Seeds

    i686-w64-mingw32-as -o seed32.o seed.s
    i686-w64-mingw32-as -o comdat32.o comdat.s          # COMDAT + "/nnn" names
    clang --target=x86_64-windows-gnu -c -O1 -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -o seed64.o seed64.c
    clang --target=x86_64-windows-gnu -c -o fix64.o ../tests/pe-coff-obj64.s
    clang --target=x86_64-windows-gnu -c -O1 -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -ffunction-sections -fdata-sections \
          -o comdat64.o comdat64.c

## Reproducers

| script | finding |
|---|---|
| `t_nsyms_overflow.py` | SIGSEGV: `i * COFF_SYMESZ` overflows `int` (tccpe.c:2605/2659/2757) |
| `t_oob_addend.py`     | ASAN heap-buffer-overflow: addend read before the bounds check (tccpe.c:2506/2513/2528) |
| `t_reloc_drop.py`     | silent drop of every relocation via `(int)nrel` (tccpe.c:2863) |
| `t_nreloc_ovfl.py`    | unbounded `load_data()` from the NRELOC_OVFL count (tccpe.c:2862) |
| `t_trunc_uninit.py`   | truncated file parsed against uninitialized heap (`load_data`, tccelf.c:3237) |
| `mkar.py`             | wraps any object as a `.a` member, to reach the same code via `tcc_load_member()` |

## Fuzzer

`fuzz.py SEED TCC "ARGS" N OUTDIR` - structure-aware mutation of the file
header, section table, symbol table, relocation records and string-table size
word, plus 15% raw byte flips and a 10% truncation.  `AR_WRAP=1` wraps each
candidate in an ar archive.  Flags any run with `rc >= 128` or an
`ERROR: AddressSanitizer` report.  `run_campaign.sh` drives 9 (seed, mode)
pairs in parallel; `N=20000 ./run_campaign.sh` is 180k inputs in ~70s.

Always run under a memory cap - some inputs legitimately ask for tens of GB:

    systemd-run --user --scope -q -p MemoryMax=6G -p MemorySwapMax=0 -- ./run_campaign.sh
