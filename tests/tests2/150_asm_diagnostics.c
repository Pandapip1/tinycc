/* Diagnostics raised while the assembler lexer is parsing an inline
   asm string must report the line of the enclosing C file.  tcc_open_bf()
   resets the global tok_flags to TOK_FLAG_BOL for the inner ":asm:"
   buffer, so error1() used to subtract 1 from the *outer* file's line
   number whenever the asm lexer happened to sit at a beginning of line -
   which is the case for practically every "unknown opcode" error. */

int printf(const char*, ...);

#if defined test_asm_unknown_opcode

int a;
int b;
void f(void) { __asm__ volatile("bogus_insn"); }   /* line 14 */
int main(void) { return 0; }

#elif defined test_asm_unknown_opcode_2nd_line

int a;
int b;
void f(void) { __asm__ volatile("nop\n\tbogus_insn\n"); }   /* line 21 */
int main(void) { return 0; }

#elif defined test_asm_unknown_opcode_3rd_line

int a;
void f(void) { __asm__ volatile("nop\n"
                                "nop\n"
                                "bogus_insn\n"); }   /* line 29 (last line of the asm stmt) */
int main(void) { return 0; }

#elif defined test_asm_global

int a;
int b;
__asm__("bogus_insn");   /* line 36 */
int main(void) { return 0; }

#elif defined test_asm_popsection

int a;
int b;
void f(void) { __asm__ volatile(".popsection\n"); }   /* line 43 */
int main(void) { return 0; }

#elif defined test_asm_pushsection_leak

/* An unbalanced '.pushsection' inside inline asm is caught and undone by
   asm_instr()'s section guard, but the saved link used to be left behind
   on the Section object.  A later, unmatched '.popsection' then silently
   succeeded instead of being diagnosed. */
static void leak(void) { __asm__ volatile(".pushsection .foo,\"a\"\n"); }
static void stray(void)
{
    __asm__ volatile(".section .foo,\"a\"\n"
                     ".popsection\n"
                     ".text\n");
}
int main(void) { leak(); stray(); return 0; }

#elif defined test_asm_ident_control

/* control: '.ident' is diagnosed after TOK_FLAG_BOL has been cleared,
   so this line number was already correct before the fix and must
   stay correct. */
int a;
int b;
void f(void) { __asm__ volatile(".ident \"zz\"\n"); }   /* line 68 */
int main(void) { return 0; }

#elif defined test_asm_linkonce_bad_type

/* GAS only warns about an unrecognized '.linkonce' type and carries on
   (gas/read.c, s_linkonce()); the valid types are discard, one_only,
   same_size and same_contents.  The second warning is the once-per-file
   note that the mark itself is dropped. */
void g(void) { __asm__ volatile(".section .foo$x,\"a\"\n"
                                ".linkonce bogus_type\n"
                                ".text\n"); }   /* line 79 */
int main(void) { return 0; }

#endif
