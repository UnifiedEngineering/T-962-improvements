/*
 * lcd.c - Display handling (KS0108 compatible, with two chip selects, active high)
 * for T-962 reflow controller
 *
 * Copyright (C) 2010,2012,2013,2014 Werner Johansson, wj@unifiedengineering.se
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "LPC214x.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lcd.h"
#include "smallfont.h"
#include "bigfont.h"
#include "sched.h"

// Frame buffer storage (each "page" is 8 pixels high)
static uint8_t FB[FB_WIDTH / 8][FB_HEIGHT+1];

typedef struct __attribute__ ((packed)) {
	uint8_t bfType[2]; // 'BM' only in this case
	uint32_t bfSize; // Total size
	uint16_t bfReserved[2];
	uint32_t bfOffBits; // Pixel start byte
	uint32_t biSize; // 40 bytes for BITMAPINFOHEADER
	int32_t biWidth; // Image width in pixels
	int32_t biHeight; // Image height in pixels (if negative image is right-side-up)
	uint16_t biPlanes; // Must be 1
	uint16_t biBitCount; // Must be 1
	uint32_t biCompression; // Only 0 (uncompressed) supported at the moment
	uint32_t biSizeImage; // Pixel data size in bytes
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
	uint32_t aColors[2]; // Palette data, first color is used if pixel bit is 0, second if pixel bit is 1
} BMhdr_t;


void charoutbig(uint8_t theChar, uint16_t X, uint16_t Y, uint8_t theFormat) {
	uint16_t y;
	uint32_t xormask;
	uint16_t xoffset = X & 0x07;
	uint32_t temp,old,cmask;
	uint8_t height;
	X >>= 3;

	switch (theFormat & 0x7F) {
		case FONT12X16:
			height=16;
			cmask=~(0xFFF0 <<xoffset);
			if (theFormat & INVERT) {
				xormask=0xFFF0;
			} else {
				xormask=0x0000;
			}
			for (y=0;y<height; y++) {
				temp=(font_12x16[theChar][y*2+1]<<8)|(font_12x16[theChar][y*2]);
				temp ^=xormask;
				temp <<=xoffset;
				old= (FB[X+2][Y+y]<<16)|(FB[X+1][Y+y]<<8)|(FB[X][Y+y]);
				old&=cmask;
				temp |=old;
				FB[X+2][Y+y]=temp>>16;
				FB[X+1][Y+y]=((temp>>8) & 0xFF);
				FB[X][Y+y]=(temp & 0xFF);
			}
			break;
		case FONT6X8:
			height=8;
			cmask=~(0xFC <<xoffset);
			if (theFormat & INVERT) {
				xormask=0xFC;
			} else {
				xormask=0x00;
			}
			for (y=0;y<height; y++) {
				temp=font_6x8[theChar][y];
				temp^=xormask;
				temp <<=xoffset;
				old=(FB[X+1][Y+y]<<8)|(FB[X][Y+y]);
				old&=cmask;
				temp |=old;
				if ((Y+y)<FB_HEIGHT) {
					if (X<((FB_WIDTH-1)<<3)) FB[X][Y+y]=temp & 0xFF;
					if (X+1<((FB_WIDTH-1)<<3)) FB[X+1][Y+y]=(temp>>8)&0xFF;
				}

			}
			break;
	}
}


void LCD_disp_str(uint8_t* theStr, uint8_t theLen, uint16_t startx, uint8_t y, uint8_t theFormat) {
	for (uint8_t q = 0; q < theLen; q++) {
		charoutbig(theStr[q], startx, y, theFormat);
		switch (theFormat & 0x7F) {
			case FONT6X6:
				startx +=6;
				break;
			case FONT6X8:
				startx +=6;
				break;
			default:	// FONT12X16
				startx +=12;
		}
	}
}

void LCD_MultiLineH(uint8_t startx, uint8_t endx, uint64_t ymask) {
	for (uint8_t x = startx; x <= endx; x++) {
		FB[0][x] |= ymask & 0xff;
		FB[1][x] |= ymask >> 8;
		FB[2][x] |= ymask >> 16;
		FB[3][x] |= ymask >> 24;
#if FB_HEIGHT >= 64
		FB[4][x] |= ymask >> 32;
		FB[5][x] |= ymask >> 40;
		FB[6][x] |= ymask >> 48;
		FB[7][x] |= ymask >> 56;
#endif
	}
}

/*
 * At the moment this is a very basic BMP file reader with the following limitations:
 * The bitmap must be 1-bit, uncompressed with a BITMAPINFOHEADER.
 */
uint8_t LCD_BMPDisplay(uint8_t* thebmp, uint16_t xoffset, uint16_t yoffset) {
	BMhdr_t* bmhdr;
	uint8_t upsidedown = 1;
	uint8_t inverted = 0;
	uint16_t pixeloffset;
	uint8_t numpadbytes = 0;

	// The following code grabs the header portion of the bitmap and caches it locally on the stack
	BMhdr_t temp;
	uint8_t* xxx = (uint8_t*) &temp;
	for (uint16_t xx = 0; xx < sizeof(BMhdr_t); xx++) {
		xxx[xx] = *(thebmp + xx);
	}
	bmhdr = &temp;

//	printf("\n%s: bfSize=%x biSize=%x", __FUNCTION__, (uint16_t)bmhdr->bfSize, (uint16_t)bmhdr->biSize);
//	printf("\n%s: Image size is %d x %d", __FUNCTION__, (int16_t)bmhdr->biWidth, (int16_t)bmhdr->biHeight);
	if (bmhdr->biPlanes != 1 || bmhdr->biBitCount != 1 || bmhdr->biCompression != 0) {
		printf("\n%s: Incompatible bitmap format!", __FUNCTION__);
		return 1;
	}
	pixeloffset = bmhdr->bfOffBits;
	if (bmhdr->aColors[0] == 0) {
		inverted = 1;
	}
	if (bmhdr->biHeight<0) {
		bmhdr->biHeight = -bmhdr->biHeight;
		upsidedown = 0;
	}
	if ((bmhdr->biWidth+xoffset > FB_WIDTH) || (bmhdr->biHeight+yoffset > FB_HEIGHT)) {
		printf("\n%s: Image won't fit on display!", __FUNCTION__);
		return 1;
	}

	// Figure out how many dummy bytes that is present at the end of each line
	// If the image is 132 pixels wide then the pixel lines will be 20 bytes (160 pixels)
	// 132&31 is 4 which means that there are 3 bytes of padding
	numpadbytes = (4-((((bmhdr->biWidth) & 0x1f) + 7) >> 3)) & 0x03;
	uint8_t trim_mask=0;
	for (int i=0;i<(bmhdr->biWidth & 0x03);i++) {
		trim_mask|=128>>i;
	}
	printf("\n%s: Skipping %d padding bytes after each line", __FUNCTION__, numpadbytes);

	for (int16_t y = bmhdr->biHeight - 1; y >= 0; y--) {
		uint16_t realY = upsidedown ? (uint8_t)y : (uint8_t)(bmhdr->biHeight) - y;
		realY += yoffset;
		for(uint16_t x = 0; x < bmhdr->biWidth; x += 8) {
			uint8_t pixel = *(thebmp + (pixeloffset++));
			uint8_t revpixel=0;
			if (x==((bmhdr->biWidth >>3)<<3)) {
				pixel&=trim_mask;
			}
			if (inverted) { pixel^=0xff; }
			for (uint8_t bits=0;bits<8;bits++) {
				if (pixel & (1<<bits)) {
					revpixel|=(1<<(7-bits));
				}
			}
			uint16_t expixel=revpixel<<(xoffset &7);
			uint16_t mpixel = 0xFF<<(xoffset &7);
			FB[(x+xoffset)>>3][realY] &= ~(mpixel & 0xff);
			FB[((x+xoffset)>>3)+1][realY] &= ~(mpixel >>8);
			FB[(x+xoffset)>>3][realY] |= expixel & 0xff;
			FB[((x+xoffset)>>3)+1][realY]|= expixel >>8;
			}
		pixeloffset += numpadbytes;
		}
	return 0;
}

void LCD_SetPixel(uint8_t x, uint16_t y) {
	if (x >= FB_WIDTH || y >= FB_HEIGHT) {
		// No random memory overwrites thank you
		return;
	}
	FB[y >> 3][x] |= 1 << (y & 0x07);
}

void TFT_SetPixel(uint16_t x, uint16_t y) {
	if (x >= FB_WIDTH || y >= FB_HEIGHT) {
		// No random memory overwrites thank you
		return;
	}
	FB[x>>3][y]|=1<<(x & 0x07);
}

void TFT_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t dx, dy, err, e2 , sy,sx;

    if ((x0>=FB_WIDTH)||(y0>=FB_HEIGHT)||(x1>=FB_WIDTH)||(y1>=FB_HEIGHT)) return;
    dx =  abs(x1-x0);
    sx = x0<x1 ? 1 : -1;
    dy = -abs(y1-y0);
    sy = y0<y1 ? 1 : -1;
    err = dx+dy;  /* error value e_xy */
    while (1) {
        TFT_SetPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2*err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void TFT_DrawGrid(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax,uint16_t density) {
	uint16_t dx,dy,sdx, sdy,x,y,len,xscale,yscale;
	double units_x,units_y,grid_x,grid_y;
	char buf[4];

    if ((x0>=FB_WIDTH)||(y0>=FB_HEIGHT)||(x1>=FB_WIDTH)||(y1>=FB_HEIGHT)) return;
	if ((x0>=x1)||(y0>=y1)) return;
	if ((xmin>=xmax)||(ymin>=ymax)) return;

	dx=x1-x0;
	dy=y1-y0;
	sdx=xmax-xmin;
	sdy=ymax-ymin;
	grid_x=dx/density;
	grid_y=dy/density;
	units_x=sdx/density;
	units_y=sdy/density;

	if ((grid_x<=4)||(grid_y<=4)) return;

	TFT_DrawLine(x0,y0,x0,y1);
	TFT_DrawLine(x0,y1,x1,y1);

	y=y1;
	xscale=xmin;
	yscale=ymin;
	while (y>y0) {
		x=x0;
		if (y<y1) {	// do not display second '0' on the scale
			len = snprintf(buf, sizeof(buf), "%d",yscale);
			LCD_disp_str((uint8_t*)buf,len,x-len*6-4,y-3,FONT6X8);
		}
		yscale+=units_y;
		while (x<x1) {
			TFT_SetCross(x,y);
			if (y==y1) {
				len = snprintf(buf, sizeof(buf), "%d",xscale);
				LCD_disp_str((uint8_t*)buf,len,x-len*3,y+3,FONT6X8);
				xscale+=units_x;
			}
			x+=grid_x;
			if ((y==y1)&&(x==x1)) {
				len = snprintf(buf, sizeof(buf), "%d",xscale);
				LCD_disp_str((uint8_t*)buf,len,x-len*3,y+3,FONT6X8);
				xscale+=units_x;
			}

		}
		TFT_SetCross(x,y);
		y-=grid_y;
	}
}

void TFT_SetCross(uint16_t x,uint16_t y) {
	if (x >= FB_WIDTH || y >= FB_HEIGHT) {
		// No random memory overwrites thank you
		return;
	}
	TFT_SetPixel(x,y);
	TFT_SetPixel(x,y-1);
	TFT_SetPixel(x,y+1);
	TFT_SetPixel(x+1,y);
	TFT_SetPixel(x-1,y);
}
void LCD_SetBacklight(uint8_t backlight) {
	if (backlight) {
		FIO0SET = (1 << 11);
	} else {
		FIO0CLR = (1 << 11);
	}
}

#define UNTIL_BUSY_IS_CLEAR while (FIO1PIN & 0x800000); // Wait for busy to clear
#define TFT_RS_COMMAND	FIO0CLR = (1<< 22);
#define TFT_RS_DATA		FIO0SET = (1<< 22);
#define TFT_WR_LOW		FIO0CLR = (1<< 19);
#define TFT_WR_HIGH		FIO0SET = (1<< 19);
#define TFT_RD_LOW		FIO0CLR = (1<< 18);
#define TFT_RD_HIGH		FIO0SET = (1<< 18);
#define TFT_CS_LOW		FIO0CLR	= (1<< 13);
#define TFT_CS_HIGH		FIO0SET = (1<< 13);
#define	TFT_RST_LOW		FIO0CLR	= (1<< 12);
#define TFT_RST_HIGH	FIO0SET = (1<< 12);
#define TFT_DATA_INPUTS	FIO1DIR &= 0xFF00FFFF;
#define TFT_DATA_OUTPUTS FIO1DIR |= 0x00FF0000;
//#define TFT_DELAY 	BusyWait(TICKS_US(1));
#define TFT_DELAY { \
			FIO1PIN; }
#define TFT_WSTROBE {	\
			TFT_DELAY;	\
			TFT_WR_LOW;	\
			TFT_DELAY;	\
			TFT_WR_HIGH;}

static void TFT_WriteAdr(uint32_t regindex) {
	FIO1PIN;	// short delay
	TFT_RS_COMMAND;
	FIO1PIN = 0x00;
	TFT_WSTROBE;
	FIO1PIN = regindex << 16;
	TFT_WSTROBE;
}

static void TFT_WriteDta(uint32_t dta) {
	uint32_t tmp;
	tmp = (FIO1PIN)&(0xff00ffff) ;	// short delay
	TFT_RS_DATA;
	FIO1PIN = ((dta <<8)& 0x00FF0000) | tmp;
	TFT_WSTROBE;
	FIO1PIN = ((dta <<16) & 0x00FF0000) | tmp;
	TFT_WSTROBE;
}

static void TFT_WriteCmd(uint32_t regindex, uint32_t cmd) {
	TFT_DATA_OUTPUTS;
	TFT_WriteAdr(regindex);
	TFT_WriteDta(cmd);
}


void TFT_Init(void) {

	TFT_CS_HIGH;
	TFT_RD_HIGH;
	TFT_WR_HIGH;
	TFT_RS_DATA;
	TFT_RST_LOW;
	BusyWait(TICKS_MS(10));
	TFT_RST_HIGH;
	TFT_RST_LOW;
	BusyWait(TICKS_MS(10));
	TFT_RST_HIGH;
	BusyWait(TICKS_MS(50));

	TFT_CS_LOW;

	TFT_WriteCmd(ILI9325_DRIVER_CODE_REG, 0x0001);				// Set internal timing
	BusyWait(TICKS_MS(50));

	TFT_WriteCmd(ILI9325_Internal_Timing_1, 0x3008);				// Set internal timing
	TFT_WriteCmd(ILI9325_Internal_Timing_2, 0x0012);				// Set internal timing
	TFT_WriteCmd(ILI9325_Internal_Timing_3, 0x1231);				// Set internal timing

	//************* Start Initial Sequence **********//
	TFT_WriteCmd(ILI9325_Driver_Output_Control_1, 0x0000);			// Set SS=1 and SM=0 bit
	TFT_WriteCmd(ILI9325_LCD_Driving_Wave_Control, 0x0700);			// Set 1 line inversion
	TFT_WriteCmd(ILI9325_Entry_Mode, 0x1038);						// Set GRAM write direction and BGR=1 ****************
	TFT_WriteCmd(ILI9325_Resizing_Control_Register, 0x0000);		// Set Resize register
	TFT_WriteCmd(ILI9325_Display_Control_2, 0x0202);				// Set the back porch and front porch from AN(orginal in driver 0x0202)
	TFT_WriteCmd(ILI9325_Display_Control_3, 0x0000);				// Set non-display area refresh cycle ISC[3:0]
	TFT_WriteCmd(ILI9325_Display_Control_4, 0x0000);				// FMARK function
	TFT_WriteCmd(ILI9325_RGB_Display_Interface_Control_1, 0x0000);	// RGB interface setting
	TFT_WriteCmd(ILI9325_Frame_Maker_Position, 0x0000);				// Frame marker Position
	TFT_WriteCmd(ILI9325_RGB_Display_Interface_Control_2, 0x0000);	// RGB interface polarity

	//*************Power On sequence ****************//
	TFT_WriteCmd(ILI9325_Power_Control_1, 0x0000);					// SAP, BT[3:0], AP, DSTB, SLP, STB
	TFT_WriteCmd(ILI9325_Power_Control_2, 0x0000);					// DC1[2:0], DC0[2:0], VC[2:0]
	TFT_WriteCmd(ILI9325_Power_Control_3, 0x0139);					// VREG1OUT voltage
	TFT_WriteCmd(ILI9325_Power_Control_4, 0x0000);					// VDV[4:0] for VCOM amplitude
	BusyWait(TICKS_MS(200));

	TFT_WriteCmd(ILI9325_Power_Control_1, 0x1190);					// SAP, BT[3:0], AP, DSTB, SLP, STB (other value: 0x1190 0x1290 0x1490 0x1690)
	TFT_WriteCmd(ILI9325_Power_Control_2, 0x0227);					// DC1[2:0], DC0[2:0], VC[2:0] (other value: 0x0221 0x0227)
	BusyWait(TICKS_MS(50));

	TFT_WriteCmd(ILI9325_Power_Control_3, 0x001C);					// VREG1OUT voltage (other value: 0x0018 0x001A 0x001B 0x001C)
	BusyWait(TICKS_MS(50));

	TFT_WriteCmd(ILI9325_Power_Control_4, 0x1d00);					// VDV[4:0] for VCOM amplitude (other: 0x1A00)
	TFT_WriteCmd(ILI9325_Power_Control_7, 0x0013);					// Set VCM[5:0] for VCOMH (other: 0x0025)
//	TFT_WriteCmd(ILI9325_Frame_Rate_and_Color_Control, 0x000C);		// Set Frame Rate
	BusyWait(TICKS_MS(50));

	TFT_WriteCmd(ILI9325_Horizontal_GRAM_Address_Set, 0x0000);		// Set GRAM horizontal Address
	TFT_WriteCmd(ILI9325_Vertical_GRAM_Address_Set, 0x0000);		// Set GRAM Vertical Address

	// ----------- Adjust the Gamma Curve ----------//
	TFT_WriteCmd(ILI9325_Gamma_Control_1, 0x0007);
	TFT_WriteCmd(ILI9325_Gamma_Control_2, 0x0302);
	TFT_WriteCmd(ILI9325_Gamma_Control_3, 0x0105);
	TFT_WriteCmd(ILI9325_Gamma_Control_4, 0x0206);					// Gradient adjustment registers
	TFT_WriteCmd(ILI9325_Gamma_Control_5, 0x0808);					// Amplitude adjustment registers
	TFT_WriteCmd(ILI9325_Gamma_Control_6, 0x0206);
	TFT_WriteCmd(ILI9325_Gamma_Control_7, 0x0504);
	TFT_WriteCmd(ILI9325_Gamma_Control_8, 0x0007);
	TFT_WriteCmd(ILI9325_Gamma_Control_9, 0x0105);					// Gradient adjustment registers
	TFT_WriteCmd(ILI9325_Gamma_Control_10, 0x0808);					// Amplitude adjustment registers

	//------------------ Set GRAM area ---------------//
	TFT_WriteCmd(ILI9325_Horizontal_Address_Start_Position, 0x0000);	// Horizontal GRAM Start Address = 0
	TFT_WriteCmd(ILI9325_Horizontal_Address_End_Position, 0x00EF);		// Horizontal GRAM End Address = 239
	TFT_WriteCmd(ILI9325_Vertical_Address_Start_Position, 0x0000);		// Vertical GRAM Start Address = 0
	TFT_WriteCmd(ILI9325_Vertical_Address_End_Position, 0x013F);		// Vertical GRAM End Address = 319

	TFT_WriteCmd(ILI9325_Driver_Output_Control_2, 0xA700);				// Gate Scan Line	************************
	TFT_WriteCmd(ILI9325_Base_Image_Display_Control, 0x0001);			// NDL,VLE, REV (other: 0x0001)
	TFT_WriteCmd(ILI9325_Vertical_Scroll_Control, 0x0000);				// Set scrolling line

	//-------------- Partial Display Control ---------//
	TFT_WriteCmd(ILI9325_Partial_Image_1_Display_Position, 0x0000);
	TFT_WriteCmd(ILI9325_Partial_Image_1_Area_Start_Line, 0x0000);
	TFT_WriteCmd(ILI9325_Partial_Image_1_Area_End_Line, 0x0000);
	TFT_WriteCmd(ILI9325_Partial_Image_2_Display_Position, 0x0000);
	TFT_WriteCmd(ILI9325_Partial_Image_2_Area_Start_Line, 0x0000);
	TFT_WriteCmd(ILI9325_Partial_Image_2_Area_End_Line, 0x0000);

	//-------------- Panel Control -------------------//
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_1, 0x0010);
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_2, 0x0000);			// (other: 0x0600)
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_3, 0x0003);
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_4, 0x1100);
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_5, 0x0000);
	TFT_WriteCmd(ILI9325_Panel_Interface_Control_6, 0x0000);

	TFT_WriteCmd(ILI9325_Display_Control_1, 0x0133);					// 262K color and display ON

	TFT_CS_HIGH;

}

// No performance gain by inlining the command code
static void LCD_WriteCmd(uint32_t cmdbyte) {
	// Start by making sure none of the display controllers are busy
	FIO1DIR = 0x000000; // Data pins are now inputs
	FIO0CLR = (1 << 22) | (1 << 13); // RS low, also make sure other CS is low
	FIO0SET = (1 << 12); // One CS at a time
	FIO0SET = (1 << 19); // RW must go high before E does
	FIO0SET = (1 << 18); // E high for read
	FIO1PIN; // Need 320ns of timing margin here
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
//	UNTIL_BUSY_IS_CLEAR;
	FIO0CLR = (1 << 12); // Swap CS
	FIO0CLR = (1 << 18); // E low again
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0SET = (1 << 13); // One CS at a time
	FIO0SET = (1 << 18); // E high for read
	FIO1PIN; // Need 320ns of timing margin here
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
//	UNTIL_BUSY_IS_CLEAR;
	FIO0CLR = (1 << 19) | (1 << 18); // RW + E low again
	FIO1DIR = 0xff0000; // Data pins output again

	FIO0SET = (1 << 12) | (1 << 13); // Both CS active (one already activated above, doesn't matter)
	FIO1PIN = cmdbyte << 16; // Cmd on pins
	FIO1PIN; // Need ~200ns of timing margin here
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0SET = (1 << 18); // E high
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0CLR = (1 << 18); // E low
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0SET = (1 << 22); // RS high
}

// Because of the cycle time requirements for E inlining actually does not boost performance
//static inline void LCD_WriteData(uint32_t databyte, uint8_t chipnum) __attribute__((always_inline));
static inline void LCD_WriteData(uint32_t databyte, uint8_t chipnum) {
	// Start by making sure that the correct controller is selected, then make sure it's not busy
	uint32_t csmask = chipnum ? (1 << 12) : (1 << 13);
	uint32_t csmask2 = chipnum ? (1 << 13) : (1 << 12);
	FIO0SET = csmask; // CS active
	FIO0CLR = csmask2; // CS inactive
	FIO1DIR = 0x000000; // Data pins are now inputs
	FIO0CLR = (1 << 22); // RS low
	FIO0SET = (1 << 19); // RW must go high before E does
	FIO0SET = (1 << 18); // E high for read
	FIO1PIN; // Need 320ns of timing margin here
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
//	UNTIL_BUSY_IS_CLEAR;
	FIO0CLR = (1 << 18) | (1 << 19); // E and RW low
	FIO0SET = (1 << 22); // RS high
	FIO1DIR = 0xff0000; // Data pins output again
	FIO1PIN = databyte << 16; // Data on pins
	FIO1PIN; // Need ~200ns of timing margin here
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0SET = (1 << 18); // E high
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO0CLR = (1 << 18); // E low
/*	When inlining additional padding needs to be done
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;
	FIO1PIN;*/
}

#define LCD_ON (0x3f)
#define LCD_RESET_X (0xb8)
#define LCD_RESET_Y (0x40)
#define LCD_RESET_STARTLINE (0xc0)


void LCD_Init(void) {
	FIO1DIR = 0xff0000; // Data pins output
	LCD_WriteCmd(LCD_ON);
	LCD_WriteCmd(LCD_RESET_X);
	LCD_WriteCmd(LCD_RESET_Y);
	LCD_WriteCmd(LCD_RESET_STARTLINE);
	LCD_FB_Clear();
	LCD_FB_Update();
	LCD_SetBacklight(1);
}

void LCD_FB_Clear(void) {
	// Init FB storage
	for (uint8_t j = 0; j < (FB_HEIGHT / 8); j++) {
		memset(FB[j], 0, FB_WIDTH);
	}
}

void TFT_FB_Clear(void) {
		memset(FB, 0, FB_WIDTH*FB_HEIGHT/8);
}

void TFT_FB_Update() {
	uint32_t color=0;
	uint32_t x=0;
	uint32_t y=0;
	TFT_CS_LOW;

	for(y = 0; y < FB_HEIGHT; y++) {
		TFT_WriteCmd(0x20,y);
		TFT_WriteCmd(0x21,0);
		TFT_WriteAdr(0x22);
		for (x=0; x< FB_WIDTH; x++) {
			if (FB[((FB_WIDTH/8)-1)-(x>>3)][y]&(128>>((x & 0x07)))) {
					color=0xFFFF;
			} else {
				color=0;
			}
			TFT_WriteDta(color);
		}
	}
	TFT_CS_HIGH;
}

void LCD_FB_Update() {
	for (uint32_t page = 0; page < (FB_HEIGHT >> 3); page++) {
		LCD_WriteCmd(LCD_RESET_X + page);
		LCD_WriteCmd(LCD_RESET_Y);

		for(uint32_t i = 0; i < 64; i++) {
			LCD_WriteData(FB[page][i], 0);
			LCD_WriteData(FB[page][i + 64], 1);
		}
	}
}
