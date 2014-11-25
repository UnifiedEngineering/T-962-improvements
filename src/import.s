	.section ".text"
	.global logobmp
	.global logobmpsize
logobmp:
	.incbin "../src/UEoSlogo-128x64.bmp"
logobmpsize:
	.word .-logobmp

	.global graphbmp
	.global graphbmpsize
graphbmp:
	.incbin "../src/graph-128x64.bmp"
graphbmpsize:
	.word .-graphbmp
