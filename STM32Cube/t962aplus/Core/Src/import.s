	.section ".text"
	.global logobmp
	.global logobmpsize
logobmp:
	.incbin "../Core/Src/images/UEoSlogo-128x64.bmp"
logobmpsize:
	.word .-logobmp

	.global graphbmp
	.global graphbmpsize
graphbmp:
	.incbin "../Core/Src/images/graph-128x64.bmp"
graphbmpsize:
	.word .-graphbmp

	.global stopbmp
	.global stopbmpsize
stopbmp:
	.incbin "../Core/Src/images/stop-18x64.bmp"
stopbmpsize:
	.word .-stopbmp

	.global selectbmp
	.global selectbmpsize
selectbmp:
	.incbin "../Core/Src/images/selectprofile-18x64.bmp"
selectbmpsize:
	.word .-selectbmp

	.global editbmp
	.global editbmpsize
editbmp:
	.incbin "../Core/Src/images/editprofile-18x64.bmp"
editbmpsize:
	.word .-editbmp

	.global f3editbmp
	.global f3editbmpsize
f3editbmp:
	.incbin "../Core/Src/images/f3edit-18x16.bmp"
f3editbmpsize:
	.word .-f3editbmp
