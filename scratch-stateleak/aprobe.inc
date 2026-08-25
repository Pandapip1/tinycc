	.text
	.globl aprobe
	.type	aprobe, @function
aprobe:
	movl	$1, %eax
	ret
	.size	aprobe, .-aprobe
	.section	.rodata
.LSTR:
	.string	"hello"
	.ascii	"world"
	.byte	1,2,3
	.align 4
	.long	7
	.data
	.globl adata
adata:
	.quad	.LSTR
	.section	.note.GNU-stack,"",@progbits
