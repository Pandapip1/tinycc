void f1(void) { __asm__ volatile(".pushsection .foo,\"a\"\n.byte 0x11\n"); }
void f2(void) { __asm__ volatile(".popsection\n"); }
