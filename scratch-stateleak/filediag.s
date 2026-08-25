	.file	"orig.c"
	.text
foo:
	movl $1, %eax
	bogusinsn
	ret
