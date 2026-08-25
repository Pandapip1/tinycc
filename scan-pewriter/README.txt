Reconnaissance scripts for the tccpe.c PE-writer audit (branch tcc-coord/scan-pewriter).
No compiler source is modified by anything in this directory.

  pecheck.py <image>...  parse an emitted PE/PE32+ image from raw bytes and
                         recompute SizeOfCode / SizeOfInitializedData /
                         SizeOfUninitializedData / SizeOfImage / BaseOfCode /
                         CheckSum / alignment invariants from the file's own
                         content, then diff against the optional header.
                         Also walks the base-relocation chain and checks each
                         fixup target lands in raw section data and holds a VA
                         inside [ImageBase, ImageBase+SizeOfImage).

  pedirs.py  <image>...  walk the IMPORT / IAT / EXPORT / BASERELOC / TLS /
                         EXCEPTION / DELAY_IMPORT structures and compare the
                         bytes they really occupy with the Size advertised in
                         the corresponding IMAGE_DATA_DIRECTORY entry.

Usage (cross compilers built in this tree):
  ./x86_64-win32-tcc -B/tmp/claude/pe64root foo.c -o foo.exe
  ./i386-win32-tcc   -B/tmp/claude/pe32root foo.c -o foo.exe
  python3 scan-pewriter/pecheck.py foo.exe
