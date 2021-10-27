/*
 * display.c
 *
 *  Created on: 25 paź 2021
 *      Author: s.bartkowicz
 */


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
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "display.h"

#ifdef USES_ORIGINAL_DISPLAY

#include "lcd.h"

void Display_disp_str(uint8_t* theStr, uint8_t theLen, uint16_t startx, uint16_t y, uint8_t theFormat) {
	if ((startx<256)&&(y<256)) {
		LCD_disp_str(theStr,theLen,(uint8_t)startx,(uint8_t)y,theFormat);
	}
}
uint8_t Display_BMPDisplay(uint8_t* thebmp, uint16_t xoffset, uint16_t yoffset) {
	uint8_t retval=0;
	if ((xoffset<256)&&(yoffset<256)) {
		retval=Display_BMPDisplay(thebmp,xoffset,yoffset);
	}
	return retval;
}
void Display_SetPixel(uint16_t x, uint16_t y) {
	if ((x<256)&&(y<256)) {
		LCD_SetPixel((uint8_t)x,(uint8_t)y);
	}

}

// two not used function for standard display (no use with 128x64 pixel resolution)
void Display_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {}
void Display_DrawGrid(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax,uint16_t density) {}

void Display_SetBacklight(uint8_t backlight) {
	LCD_SetBacklight(backlight);
}
void Display_Init(void) {
	LCD_Init();
}
void Display_FB_Clear(void) {
	LCD_FB_Clear();
}
void Display_FB_Update(void) {
	LCD_FB_Update();
}


#elif defined (USES_FRAMEBUFFER)		// limited to 320x240 displays, only monochrome (due to memory constrains)

#include "bigfont.h"
#include "tft.h"

static uint8_t FB[((FB_WIDTH+7) >> 3)*FB_HEIGHT];

// Frame buffer definition for monochome display

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


//uint32_t charoutdirect(uint8_t theChar, uint16_t X, uint16_t, Y, uint8_t theFormat, uint16_t FGColor, uint16_t BGColor) {
// converts internal bitmap font to color information and feeds it directly to the TFT controller Frame Buffer
//	uint16_t
//}

uint32_t fontbytesize(sFontStruct* font) {
	uint32_t bytesize;
	if (font->flags&FFLAG_VERTICAL) {
		bytesize=font->height>>3;
		if ((font->height&0x07)>0) bytesize++;
	} else {
		bytesize=font->width>>3;
		if ((font->width&0x07)>0) bytesize++;
	}
	return bytesize;
}
uint32_t fontbytesperfont(sFontStruct* font) {
	uint32_t bytesperfont=font->height;
	if (font->flags&FFLAG_VERTICAL) {
		bytesperfont=font->width;
	}
	bytesperfont*=fontbytesize(font);
	return bytesperfont;
}
sFontStruct* fontget(uint8_t fontno) {
	sFontStruct* retval;
	switch (fontno) {
	case FONT12X16:
		retval=&font_12x16;
		break;
	case FONT6X8:
		retval=&font_6x8;
		break;
	case FONTVERDANA:
		retval=&verdana_13x12;
		break;
	default:
		retval=&font_12x16;
	}
	return retval;
}

uint32_t fontgetmask(sFontStruct* font) {
	uint32_t retval=1;
	for (int i=0;i<font->width;i++) {
		retval<<=1;
		retval|=1;
	}
	uint32_t adjust=(fontbytesize(font)<<3)-font->width;
	retval<<=adjust;	// left adjustment
	return retval;
}

uint8_t fontprintable(sFontStruct* font, uint8_t theChar) {
	if ((theChar>=font->starting_char)&&(theChar<=font->ending_char)) return 1;
	else return 0;
}

uint32_t fontgetcharaddress(sFontStruct* font,uint8_t theChar) {
	uint32_t returnval=0;
	if (fontprintable(font,theChar)) {
		if (font->flags&FFLAG_PROPORTIONAL) returnval=(font->ending_char+1-font->starting_char);
		returnval+=(theChar-font->starting_char)*fontbytesperfont(font);
	}
	return returnval;
}

uint32_t fontgetwidth(sFontStruct* font, uint8_t theChar) {
	if (fontprintable(font,theChar)) {

		if (font->flags&FFLAG_PROPORTIONAL) {
			return (uint32_t)font->data[theChar-font->starting_char];
		} else return font->width;
	} else return 0;
}

uint32_t Display_get_str_lenght(uint8_t* theStr, uint8_t theLen, uint8_t theFormat) {
	sFontStruct* font=fontget(theFormat & 0x7f);
	uint32_t lenght=0;
	for (int i=0;i<theLen;i++) {
		lenght+=fontgetwidth(font,theStr[i]);
	}
	return lenght;
}

uint32_t charoutbig(uint8_t theChar, uint16_t X, uint16_t Y, uint8_t theFormat) {
	uint32_t temp,old,xormask,cmask;
	uint32_t xoffset = X & 0x07;
	sFontStruct* font=fontget(theFormat & 0x7f);
	uint32_t fontoffset=fontgetcharaddress(font,theChar);
	uint32_t bytesize=fontbytesize(font);
	uint32_t height=font->height;
	uint8_t bitmask;
	cmask=fontgetmask(font);

	if (theFormat & INVERT) {
		xormask=cmask;
	} else {
		xormask=0;
	}

	if (fontprintable(font,theChar)) {
		if ((font->flags)&FFLAG_VERTICAL) {
			xormask>>=8-(height&7);
			for (int x=0;x<fontgetwidth(font,theChar);x++) {
				if ((X+x)<=(FB_WIDTH-1)) {
					bitmask=1<<((X+x)&0x07);
					temp=0;
					for (int i=bytesize-1; i>=0;i--) {
						temp<<=8;
						temp|=(uint32_t)font->data[fontoffset+i+bytesize*x];
					}
					if (theFormat & INVERT) temp^=xormask;
					for (int y=0;y<height;y++) {
						if (Y+y<FB_HEIGHT) {
							if (temp&(1<<y)) {
									FB[((X+x)>>3)+FB_BWIDTH*(Y+y)]|=bitmask;
							} else {
								FB[((X+x)>>3)+FB_BWIDTH*(Y+y)]&=~bitmask;
							}
						}
					}
				}
			}
		} else {
			cmask<<=xoffset;
			cmask=~cmask;
			X >>= 3;
			for (int y=0;y<height; y++) {
				temp=0;
				for (int i=bytesize-1;i>=0;i--) {
					temp<<=8;
					temp|=(uint32_t)font->data[fontoffset+y*bytesize+i];
				}
				temp ^=xormask;
				temp <<=xoffset;
				old=0;
				if ((Y+y)<FB_HEIGHT) {
					if (X+3<=(FB_BWIDTH-1)) old|=FB[X+3+FB_BWIDTH*(Y+y)]<<24;
					if (X+2<=(FB_BWIDTH-1)) old|=FB[X+2+FB_BWIDTH*(Y+y)]<<16;
					if (X+1<=(FB_BWIDTH-1)) old|=FB[X+1+FB_BWIDTH*(Y+y)]<<8;
					if (X<=(FB_BWIDTH-1)) old|=FB[X+FB_BWIDTH*(Y+y)];
				}
				old&=cmask;
				temp |=old;
				if ((Y+y)<FB_HEIGHT) {
					if (X+3<=(FB_BWIDTH-1)) FB[X+3+FB_BWIDTH*(Y+y)]=(temp>>24) & 0xFF;
					if (X+2<=(FB_BWIDTH-1)) FB[X+2+FB_BWIDTH*(Y+y)]=(temp>>16) & 0xFF;
					if (X+1<=(FB_BWIDTH-1)) FB[X+1+FB_BWIDTH*(Y+y)]=(temp>>8) & 0xFF;
					if (X<=(FB_BWIDTH-1)) FB[X+FB_BWIDTH*(Y+y)]=temp & 0xFF;
				}
			}
		}
	}
	return fontgetwidth(font,theChar);
}


void Display_disp_str(uint8_t* theStr, uint8_t theLen, uint16_t startx, uint16_t y, uint8_t theFormat) {
	for (uint8_t q = 0; q < theLen; q++) {
		startx+=charoutbig(theStr[q], startx, y, theFormat);
	}
}


/*
 * At the moment this is a very basic BMP file reader with the following limitations:
 * The bitmap must be 1-bit, uncompressed with a BITMAPINFOHEADER.
 */
uint8_t Display_BMPDisplay(uint8_t* thebmp, uint16_t xoffset, uint16_t yoffset) {
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
//	printf("\n%s: Skipping %d padding bytes after each line", __FUNCTION__, numpadbytes);

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
			FB[((x+xoffset)>>3)+FB_BWIDTH*realY] &= ~(mpixel & 0xff);
			FB[((x+xoffset)>>3)+1+FB_BWIDTH*realY] &= ~(mpixel >>8);
			FB[((x+xoffset)>>3)+FB_BWIDTH*realY] |= expixel & 0xff;
			FB[((x+xoffset)>>3)+1+FB_BWIDTH*realY]|= expixel >>8;
			}
		pixeloffset += numpadbytes;
		}
	return 0;
}


void Display_SetPixel(uint16_t x, uint16_t y) {
	if (x >= FB_WIDTH || y >= FB_HEIGHT) {
		// No random memory overwrites thank you
		return;
	}
	FB[(x>>3)+FB_BWIDTH*y]|=1<<(x & 0x07);
}

void Display_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t dx, dy, err, e2 , sy,sx;

    if ((x0>=FB_WIDTH)||(y0>=FB_HEIGHT)||(x1>=FB_WIDTH)||(y1>=FB_HEIGHT)) return;
    dx =  abs(x1-x0);
    sx = x0<x1 ? 1 : -1;
    dy = -abs(y1-y0);
    sy = y0<y1 ? 1 : -1;
    err = dx+dy;  /* error value e_xy */
    while (1) {
        Display_SetPixel(x0, y0);
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

void Display_SetCross(uint16_t x,uint16_t y) {
	if (x >= FB_WIDTH || y >= FB_HEIGHT) {
		// No random memory overwrites thank you
		return;
	}
	Display_SetPixel(x,y);
	Display_SetPixel(x,y-1);
	Display_SetPixel(x,y+1);
	Display_SetPixel(x+1,y);
	Display_SetPixel(x-1,y);
}
void Display_DrawGrid(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax,uint16_t density) {
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

	Display_DrawLine(x0,y0,x0,y1);
	Display_DrawLine(x0,y1,x1,y1);

	y=y1;
	xscale=xmin;
	yscale=ymin;
	while (y>y0) {
		x=x0;
		if (y<y1) {	// do not display second '0' on the scale
			len = snprintf(buf, sizeof(buf), "%d",yscale);
			Display_disp_str((uint8_t*)buf,len,x-len*6-4,y-3,FONT6X8);
		}
		yscale+=units_y;
		while (x<x1) {
			Display_SetCross(x,y);
			if (y==y1) {
				len = snprintf(buf, sizeof(buf), "%d",xscale);
				Display_disp_str((uint8_t*)buf,len,x-len*3,y+3,FONT6X8);
				xscale+=units_x;
			}
			x+=grid_x;
			if ((y==y1)&&(x==x1)) {
				len = snprintf(buf, sizeof(buf), "%d",xscale);
				Display_disp_str((uint8_t*)buf,len,x-len*3,y+3,FONT6X8);
				xscale+=units_x;
			}

		}
		Display_SetCross(x,y);
		y-=grid_y;
	}
}

void Display_Init(void) {
	TFT_Init();
}

void Display_FB_Clear(void) {
		memset(FB, 0, FB_WIDTH*FB_HEIGHT/8);
}

void Display_FB_Update(void) {
	TFT_FB_Update(FB);
}

#else		// direct TFT access, full color, any resolution

#endif
