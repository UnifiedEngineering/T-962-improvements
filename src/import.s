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

	.global stopbmp
	.global stopbmpsize
stopbmp:
	.incbin "../src/stop-18x64.bmp"
stopbmpsize:
	.word .-stopbmp

	.global selectbmp
	.global selectbmpsize
selectbmp:
	.incbin "../src/selectprofile-18x64.bmp"
selectbmpsize:
	.word .-selectbmp

	.global editbmp
	.global editbmpsize
editbmp:
	.incbin "../src/editprofile-18x64.bmp"
editbmpsize:
	.word .-editbmp
