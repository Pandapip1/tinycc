	.section .text$sel,"xr"
	.linkonce discard
	.globl	_cfunc
_cfunc:
	call	_extfunc
	ret
	.section .rdata$cd,"dr"
	.linkonce same_size
	.globl	_cdata
_cdata:
	.long	7
	.text
	.globl	_main2
_main2:
	call	_cfunc
	ret
