	.text
	.globl	_start32
_start32:
	call	_extfunc
	movl	$_gdata, %eax
	ret
	.data
	.globl	_gdata
_gdata:
	.long	42
	.long	_start32
	.bss
	.globl	_bssvar
	.comm	_commvar, 16
