/* x86-64 fixture for the PE-COFF object reader.  Assembled by clang's
   integrated assembler (--target=x86_64-windows-gnu) into a genuine
   pe-x86-64 relocatable object; x86_64-w64-mingw32-as would do as well.
   Between them these definitions exercise:
     - IMAGE_REL_AMD64_ADDR64 (1) with and without an addend
     - IMAGE_REL_AMD64_ADDR32 (2), a 32-bit virtual address
     - IMAGE_REL_AMD64_REL32  (4) against a defined symbol, with a zero
       addend and with the non-zero addends the assembler stores when an
       immediate follows the displacement
     - IMAGE_REL_AMD64_REL32  (4) against an undefined external
   tcc is a RELA target, so every one of these addends has to be lifted out
   of the section contents into r_addend, with the field zeroed.  */

	.data
	.globl	px_bias
px_bias:
	.long	7
	.globl	px_addr32
px_addr32:
	.long	px_bias		/* ADDR32 */
	.p2align 3
	.globl	px_selfptr
px_selfptr:
	.quad	px_bias+4	/* ADDR64, addend 4 */
	.globl	px_textptr
px_textptr:
	.quad	px_add		/* ADDR64, addend 0 */

	.text
	.globl	px_add
px_add:
	movl	%ecx, %eax
	addl	%edx, %eax
	addl	px_bias(%rip), %eax	/* REL32, addend 0 */
	retq

	.globl	px_call_c
px_call_c:
	jmp	px_from_c		/* REL32 to an undefined external */

	.globl	px_set
px_set:
	movl	$55, px_bias(%rip)	/* REL32, addend -4 (4-byte immediate) */
	retq

	.globl	px_setb
px_setb:
	movb	$9, px_bias(%rip)	/* REL32, addend -1 (1-byte immediate) */
	retq
