/*
 * lcd.h
 *
 *  Created on: 14 nov 2014
 *      Author: wjo
 */

#ifndef LCD_H_
#define LCD_H_

#define FB_HEIGHT	64
#define FB_WIDTH	128
#define LCD_CENTER	64

#define INVERT			0x80	// used internally (within a 7bit value)
#define CENTERED		1
#define RIGHT_ALIGNED	2

void LCD_printf(uint8_t x, uint8_t y, uint8_t flags, const char *format, ...)
    __attribute__((format (printf, 4, 5)));

void LCD_MultiLineH(uint8_t startx, uint8_t endx, uint64_t ymask);
uint8_t LCD_BMPDisplay(uint8_t* thebmp, uint8_t xoffset, uint8_t yoffset);
void LCD_SetPixel(uint8_t x, uint8_t y);
void LCD_SetBacklight(uint8_t backlight);
void LCD_Init(void);
void LCD_FB_Clear(void);
void LCD_FB_Update(void);

#endif /* LCD_H_ */
