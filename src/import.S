	.section ".text"
	.global logobmp
	.global logobmpsize
logobmp:
	.incbin "../src/images/UEoSlogo-128x64.bmp"
logobmpsize:
	.word .-logobmp

	.global graphbmp
	.global graphbmpsize
graphbmp:
	.incbin "../src/images/graph-128x64.bmp"
graphbmpsize:
	.word .-graphbmp

	.global stopbmp
	.global stopbmpsize
stopbmp:
	.incbin "../src/images/stop-18x64.bmp"
stopbmpsize:
	.word .-stopbmp

	.global selectbmp
	.global selectbmpsize
selectbmp:
	.incbin "../src/images/selectprofile-18x64.bmp"
selectbmpsize:
	.word .-selectbmp

	.global editbmp
	.global editbmpsize
editbmp:
	.incbin "../src/images/editprofile-18x64.bmp"
editbmpsize:
	.word .-editbmp

	.global f3editbmp
	.global f3editbmpsize
f3editbmp:
	.incbin "../src/images/f3edit-18x16.bmp"
f3editbmpsize:
	.word .-f3editbmp
