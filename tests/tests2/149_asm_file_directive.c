/* '.file' names the source file for debug info.  tcc has no '.loc', so it
   cannot map .s line numbers back to that file; it used to rename the
   current file for diagnostics and for the DWARF line table anyway, and
   then reported the *new* name beside the *old* file's line numbers.  It
   is now accepted and ignored, so diagnostics name the real file, as gas
   does.  Ignoring it also means the operands are never lexed, which is
   what used to corrupt PARSE_FLAG_TOK_STR for the rest of the file. */

int printf(const char*, ...);

#if defined test_file_then_section

__asm__(".file \"asmfile.c\"\n"
        ".section .rodata.t1,\"a\"\n"
        "msg1: .string \"section-ok\"\n"
        ".text\n");
extern const char msg1[];
int main(void) { printf("%s\n", msg1); return 0; }

#elif defined test_file_then_ascii

__asm__(".file \"asmfile.c\"\n"
        ".data\n"
        "msg2: .ascii \"ascii-ok\"\n"
        ".byte 0\n"
        ".text\n");
extern const char msg2[];
int main(void) { printf("%s\n", msg2); return 0; }

#elif defined test_file_then_type

__asm__(".file \"asmfile.c\"\n"
        ".data\n"
        "msg3: .string \"type-ok\"\n"
        ".type msg3,\"object\"\n"
        ".text\n");
extern const char msg3[];
int main(void) { printf("%s\n", msg3); return 0; }

#elif defined test_file_then_type_function

/* this exact shape used to SEGFAULT: with PARSE_FLAG_TOK_STR still cleared
   by '.file', "function" arrived as a string token whose CValue was never
   filled in, and get_tok_str(tok, NULL) dereferenced it */
__asm__(".file \"asmfile.c\"\n"
        ".text\n"
        ".globl zsym\n"
        "zsym: ret\n"
        ".type zsym,\"function\"\n");
int main(void) { printf("type-function-ok\n"); return 0; }

#elif defined test_file_twice

__asm__(".file \"asmfile.c\"\n"
        ".file \"asmfile2.c\"\n"
        ".section .rodata.t4,\"a\"\n"
        "msg4: .string \"twice-ok\"\n"
        ".text\n");
extern const char msg4[];
int main(void) { printf("%s\n", msg4); return 0; }

#elif defined test_file_dwarf5_form

/* 'gcc -S -g' emits '.file 0 "dir" "name"'.  tcc used to take the
   *directory* for the file name and then choke on the second string with
   "end of line expected". */
__asm__(".file 0 \"somedir\" \"asmfile.c\"\n"
        ".file 1 \"asmfile.c\"\n"
        ".section .rodata.t6,\"a\"\n"
        "msg6: .string \"dwarf5-ok\"\n"
        ".text\n");
extern const char msg6[];
int main(void) { printf("%s\n", msg6); return 0; }

#elif defined test_file_does_not_remap_diagnostics

/* '.file' must NOT rename the file for later diagnostics: the line number
   still counts the real file, so naming 'asmfile.c' here would report one
   file's name beside another file's line numbers. */
__asm__(".file \"asmfile.c\"\n"
        ".section .rodata.t5,\"\n");
int main(void) { return 0; }

#endif
