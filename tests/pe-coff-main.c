/* C side of the PE-COFF object reader test; compiled by the i386-win32 tcc
   and linked against pe-coff-obj.o, a genuine GNU as pe-i386 object. */
#include <stdio.h>

extern int pc_add(int a, int b);
extern int pc_call_c(void);
extern int pc_bias;
extern int *pc_selfptr;
extern int (*pc_textptr)(int, int);
extern int pc_common[4];
extern char pc_bssvar[8];
extern int *pc_getcom(void);
extern char *pc_getbss(void);
extern int pc_getabs(void);

int pc_from_c(void) { return 42; }

int main(void)
{
    pc_common[0] = 5;
    pc_bssvar[0] = 9;
    printf("add=%d\n", pc_add(10, 20));
    printf("bias=%d\n", pc_bias);
    printf("selfptr=%d\n", pc_selfptr == (int *)((char *)&pc_bias + 4));
    printf("textptr=%d\n", pc_textptr == pc_add);
    printf("disp32=%d\n", pc_call_c());
    printf("common=%d %d\n", pc_getcom() == pc_common, pc_common[0]);
    printf("bss=%d %d\n", pc_getbss() == pc_bssvar, pc_bssvar[0]);
    printf("abs=0x%x\n", pc_getabs());
    return 0;
}
