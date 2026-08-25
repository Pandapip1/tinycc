/* Assembler directives and expressions that GNU as accepts and that
   'gcc -S' output relies on.  Each of these used to be rejected. */

int printf(const char*, ...);

#if defined test_zero

/* '.zero' is an alias of '.skip': N zero bytes, or N copies of an
   optional fill value. */
__asm__(".data\n"
        "z0: .byte 0x11\n"
        "    .zero 3\n"
        "    .zero 2,0x41\n"
        "    .byte 0x22\n"
        ".text\n");
extern unsigned char z0[];
int main(void)
{
    int i;
    for (i = 0; i < 7; i++)
        printf("%02x", z0[i]);
    printf("\n");
    return 0;
}

#elif defined test_section_entsize

/* A mergeable-strings section carries a fourth argument, the entry
   size, after the section type. */
__asm__(".section .rodata.str1.1,\"aMS\",@progbits,1\n"
        "smsg: .string \"entsize-ok\"\n"
        ".section .rodata.cst8,\"aM\",@progbits,8\n"
        "scst: .quad 0x1234\n"
        ".section .note.GNU-stack,\"\",@progbits\n"
        ".text\n");
extern const char smsg[];
extern const long long scst;
int main(void) { printf("%s %llx\n", smsg, (unsigned long long)scst); return 0; }

#elif defined test_p2align_maxskip

/* '.p2align n,,max': if the padding would exceed max, do not align. */
__asm__(".data\n"
        "  .p2align 4\n"
        "a0: .byte 1\n"
        "  .p2align 4,,4\n"   /* needs 15, max 4 -> no padding */
        "a1: .byte 2\n"
        "  .p2align 4,,15\n"  /* needs 14, max 15 -> pad */
        "a2: .byte 3\n"
        ".section .rodata\n"
        ".globl gaps\n"
        "gaps: .long a1 - a0\n"
        "      .long a2 - a1\n"
        ".text\n");
extern const int gaps[];
int main(void) { printf("%d %d\n", gaps[0], gaps[1]); return 0; }

#elif defined test_comm_local

/* '.comm' declares a common symbol; after '.local' it is allocated in
   .bss instead. */
__asm__(".comm gcomm,4,4\n"
        ".local lcomm1\n"
        ".comm lcomm1,8,8\n"
        ".lcomm lcomm2,4\n");
extern int gcomm;
extern long long lcomm1;
extern int lcomm2;
int main(void)
{
    gcomm = 7;
    lcomm1 = 9;
    lcomm2 = 11;
    printf("%d %lld %d\n", gcomm, lcomm1, lcomm2);
    return 0;
}

#elif defined test_leb128

__asm__(".data\n"
        "lebs: .uleb128 0, 127, 128, 624485\n"
        "      .sleb128 0, -1, 63, 64, -64, -65, -123456\n"
        "      .byte 0xee\n"
        ".text\n");
extern unsigned char lebs[];
int main(void)
{
    int i;
    for (i = 0; i < 19; i++)
        printf("%02x", lebs[i]);
    printf("\n");
    return 0;
}

#elif defined test_local_label_diff

/* Differences of numeric local labels, including forward references,
   which is what gcc emits for .note.gnu.property. */
__asm__(".section .rodata\n"
        ".globl diffs\n"
        "diffs:\n"
        "  .long 1f - 0f\n"   /* both forward */
        "  .long 4f - 1f\n"   /* both forward */
        "  .long 4f - 0f\n"   /* both forward */
        "0: .byte 1,2,3,4\n"
        "1: .byte 5,6\n"
        "4: .byte 7\n"
        "  .p2align 2\n"
        ".globl diffs2\n"
        "diffs2:\n"
        "  .long 1b - 0b\n"   /* both backward */
        "  .long 4b - 1b\n"
        "  .long 0b - 4b\n"
        ".text\n");
extern const int diffs[];
extern const int diffs2[];
int main(void)
{
    printf("%d %d %d %d %d %d\n", diffs[0], diffs[1], diffs[2],
           diffs2[0], diffs2[1], diffs2[2]);
    return 0;
}

#elif defined test_plt_suffix

/* 'sym@PLT' on a call/jump target. */
int helper(int x) { return x * 3; }
#if defined __x86_64__ || defined __i386__
__asm__(".text\n"
        ".globl viaplt\n"
        "viaplt: jmp helper@PLT\n");
extern int viaplt(int);
#else
#define viaplt helper
#endif
int main(void) { printf("%d\n", viaplt(7)); return 0; }

#elif defined test_shift_and_cltq

/* 'sall'/'salq' (aliases of shl) and 'cltq' (AT&T name of cdqe). */
#if defined __x86_64__
__asm__(".text\n"
        ".globl sh3\n"
        "sh3: movl %edi,%eax\n"
        "     sall $3,%eax\n"
        "     ret\n"
        ".globl sxhi\n"
        "sxhi: movl %edi,%eax\n"
        "      cltq\n"
        "      shrq $32,%rax\n"
        "      ret\n");
extern int sh3(int);
extern int sxhi(int);
#elif defined __i386__
__asm__(".text\n"
        ".globl sh3\n"
        "sh3: movl 4(%esp),%eax\n"
        "     sall $3,%eax\n"
        "     ret\n");
extern int sh3(int);
/* cltq is x86-64 only */
static int sxhi(int x) { return x < 0 ? -1 : 0; }
#else
static int sh3(int x) { return x << 3; }
static int sxhi(int x) { return x < 0 ? -1 : 0; }
#endif
int main(void) { printf("%d %d %d\n", sh3(5), sxhi(-1), sxhi(1)); return 0; }

#elif defined test_rip_immediate

/* 'movl $imm, sym(%rip)': the displacement is emitted before the
   immediate, so the relocation has to account for the immediate.
   NOTE: TCC's own linker adds the stored displacement to the addend, so
   it compensated for the old, wrong addend and this test passes with
   'tcc -run' either way.  The bug was only visible once the object was
   linked by a conforming ELF linker, which uses the addend alone.  This
   block guards the encoding path from regressing. */
int riptgt[8];
#if defined __x86_64__
__asm__(".text\n"
        ".globl setslot\n"
        "setslot: movl $55, riptgt+20(%rip)\n"
        "         ret\n");
extern void setslot(void);
#else
static void setslot(void) { riptgt[5] = 55; }
#endif
int main(void)
{
    setslot();
    printf("%d %d %d\n", riptgt[4], riptgt[5], riptgt[6]);
    return 0;
}

#endif
