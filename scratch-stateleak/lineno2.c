int a;
int b;
int c;
void f(void){ __asm__ volatile("nop\n\tbogus_insn\n"); }
