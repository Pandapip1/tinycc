	.text
	.pushsection .foo,"a"
	.byte 0x11
	.pushsection .foo,"a"
	.byte 0x22
	.popsection
	.byte 0x33
	.popsection
	.byte 0x44
