Reproducers for the "global parser state mutated, not restored" scan
(branch tcc-coord/scan-stateleak).  Nothing here modifies compiler source.

1) tok_flags off-by-one in diagnostics from inline asm  (libtcc.c:669)
   ./tcc -c lineno.c   -> reports line 3, offending .popsection is on line 4
   ./tcc -c lineno2.c  -> reports line 3, bogus_insn is on line 4
   ./tcc -c l4.c l5.c l6.c -> all one line too low
   ./tcc -Wunsupported -c lineno3.c -> line 4, CORRECT (BOL already cleared)

2) .file makes every later asm diagnostic name the wrong file (tccasm.c:790 /
   tccpp.c:1769)
   ./tcc -c filediag.s -> "orig.c:5", gas says "filediag.s:5"

3) .pushsection nesting stack lives on the Section (tccasm.c:494 sec->prev)
   ./tcc -c push2.s -> ".popsection without .pushsection"; gas assembles fine
   ./tcc -c pushleak.c -> unbalanced .pushsection in inline asm leaves ->prev set

4) func_old leaks past the end of a K&R function definition (tccgen.c:8602)
   funcold.c vs funcold_ref.c: the implicit-declaration warning is suppressed
   at file scope after any old-style definition.

probe.inc/prefixes.txt and aprobe.inc/aprefixes.txt are the differential
batteries used to clear the C-side and asm-side candidates (both came back
clean after address normalisation).
