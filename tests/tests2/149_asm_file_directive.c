/* The '.file' directive temporarily clears PARSE_FLAG_TOK_STR so that it
   can see the raw, still quoted string.  It must restore the flag again,
   otherwise every later directive taking a string constant breaks for the
   rest of the translation unit. */

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

#elif defined test_file_twice

__asm__(".file \"asmfile.c\"\n"
        ".file \"asmfile2.c\"\n"
        ".section .rodata.t4,\"a\"\n"
        "msg4: .string \"twice-ok\"\n"
        ".text\n");
extern const char msg4[];
int main(void) { printf("%s\n", msg4); return 0; }

#elif defined test_file_remaps_diagnostics

/* '.file' must still rename the file for subsequent diagnostics */
__asm__(".file \"asmfile.c\"\n"
        ".section .rodata.t5,\"\n");
int main(void) { return 0; }

#endif
