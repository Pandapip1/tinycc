/* '.pushsection' nesting used to be recorded in the Section object itself
   (sec->prev), so pushing the *same* section at two nesting levels
   overwrote the saved link and lost a level: the outer '.popsection' then
   failed with ".popsection without .pushsection".  gas assembles the
   equivalent .s file fine. */

int printf(const char*, ...);

__asm__(".pushsection .data,\"aw\"\n"
        "pv1: .byte 0x11\n"
        ".pushsection .data,\"aw\"\n"     /* same section, second level */
        "pv2: .byte 0x22\n"
        ".popsection\n"                   /* back to level 1 (.data) */
        "pv3: .byte 0x33\n"
        ".popsection\n"                   /* back to .text - used to fail */
        ".text\n");

extern unsigned char pv1, pv2, pv3;

int main(void)
{
    printf("%02x %02x %02x\n", pv1, pv2, pv3);
    return 0;
}
