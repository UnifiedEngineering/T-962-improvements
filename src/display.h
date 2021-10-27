/*
 * display.h
 *
 *  Created on: 25 paź 2021
 *      Author: s.bartkowicz
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#define USES_FRAMEBUFFER
#define USES_TFT_DISPLAY

#if defined( USES_ORIGINAL_DISPLAY)	// Standard 128x64 LCD display

#include "lcd.h"

#define DEFAULTFONT (FONT6X6)

#define DISPLAY_CENTER (LCD_CENTER)
#define DISPLAY_ALIGN_CENTER(x) (LCD_ALIGN_CENTER(x))
#define DISPLAY_ALIGN_RIGHT(x) (LCD_ALIGN_RIGHT(x))


#elif defined(USES_FRAMEBUFFER)

#include "tft.h"

#define FB_HEIGHT (240)
#define FB_WIDTH (320)
#define FB_BWIDTH (FB_WIDTH/8)
#define DEFAULTFONT (FONT12X16)
//#define DEFAULTFONT (FONTVERDANA)

#define FONT12X16 (0)
#define FONT6X6 (1)
#define FONT6X8 (2)
#define FONTVERDANA (3)
#define INVERT (0x80)

#define DISPLAY_CENTER (FB_WIDTH/2)
#define DISPLAY_ALIGN_CENTER(x) (DISPLAY_CENTER - (x * 6)-1)
#define DISPLAY_ALIGN_RIGHT(x) (FB_WIDTH - (x * 12)-1)

typedef struct sFontStruct{
	const uint8_t width;
	const uint8_t height;
	const uint8_t starting_char;
	const uint8_t ending_char;
	const uint32_t flags;
	const uint8_t *data;
}sFontStruct;


#endif /* Display HAL */

uint32_t fontbytesize(sFontStruct* font);
uint32_t fontbytesperfont(sFontStruct* font);
sFontStruct* fontget(uint8_t fontno);
uint32_t fontgetmask(sFontStruct* font);
uint8_t fontprintable(sFontStruct* font, uint8_t theChar);
uint32_t fontgetcharaddress(sFontStruct* font,uint8_t theChar);
uint32_t fontgetwidth(sFontStruct* font, uint8_t theChar);

uint32_t charoutbig(uint8_t theChar, uint16_t X, uint16_t Y, uint8_t theFormat);
void Display_disp_str(uint8_t* theStr, uint8_t theLen, uint16_t startx, uint16_t y, uint8_t theFormat);
uint32_t Display_get_str_lenght(uint8_t* theStr, uint8_t theLen, uint8_t theFormat);
uint8_t Display_BMPDisplay(uint8_t* thebmp, uint16_t xoffset, uint16_t yoffset);
void Display_SetPixel(uint16_t x, uint16_t y);
void Display_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void Display_DrawGrid(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t xmin, uint16_t ymin, uint16_t xmax, uint16_t ymax,uint16_t density);
void Display_SetBacklight(uint8_t backlight);
void Display_Init(void);
void Display_FB_Clear(void);
void Display_FB_Update(void);

#endif /* DISPLAY_H_ */
