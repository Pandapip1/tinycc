/* Fixture for the PE-COFF object reader: assembled by i686-w64-mingw32-as
   into a genuine pe-i386 relocatable object.  Between them these definitions
   exercise every relocation type and symbol kind the reader translates:
     - IMAGE_REL_I386_DIR32 (6) against a section symbol, with and without an
       addend, into .text and into .data
     - IMAGE_REL_I386_REL32 (20, "DISP32") against an undefined external
     - a COFF common symbol (section number 0, size in the value field)
     - an absolute symbol (section number -1)
     - a .bss (uninitialized) section
   Symbols are written without a leading underscore to match tcc's default
   win32 naming (tcc only prefixes with '_' under -fleading-underscore).  */

	.comm	pc_common,16,4

	.globl	pc_abs
	.set	pc_abs, 0x1234

	.bss
	.globl	pc_bssvar
pc_bssvar:
	.space	8

	.data
	.globl	pc_bias
pc_bias:
	.long	7
	.globl	pc_selfptr
pc_selfptr:
	.long	pc_bias+4	/* DIR32, section symbol .data, addend 4 */
	.globl	pc_textptr
pc_textptr:
	.long	pc_add		/* DIR32, section symbol .text, addend 0 */

	.text
	.globl	pc_add
pc_add:
	movl	4(%esp), %eax
	addl	8(%esp), %eax
	addl	pc_bias, %eax	/* DIR32 into .data */
	ret

	.globl	pc_call_c
pc_call_c:
	jmp	pc_from_c	/* DISP32 to an undefined external */

	.globl	pc_getcom
pc_getcom:
	movl	$pc_common, %eax
	ret

	.globl	pc_getbss
pc_getbss:
	movl	$pc_bssvar, %eax
	ret

	.globl	pc_getabs
pc_getabs:
	movl	$pc_abs, %eax
	ret
