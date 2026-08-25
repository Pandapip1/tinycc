/* C side of the x86-64 PE-COFF object reader test. */
#include <stdio.h>

extern int px_add(int a, int b);
extern int px_call_c(void);
extern int px_bias;
extern int px_addr32;
extern int *px_selfptr;
extern int (*px_textptr)(int, int);
extern void px_set(void), px_setb(void);

int px_from_c(void) { return 42; }

int main(void)
{
    printf("add=%d\n", px_add(10, 20));
    printf("addr32=%d\n", px_addr32 == (int)(long long)&px_bias);
    printf("selfptr=%d\n", px_selfptr == (int *)((char *)&px_bias + 4));
    printf("textptr=%d\n", px_textptr == px_add);
    printf("rel32=%d\n", px_call_c());
    px_set();
    printf("set=%d\n", px_bias);
    px_setb();
    printf("setb=%d\n", px_bias);
    return 0;
}
