/*
 * lcd.h
 *
 *  Created on: 14 nov 2014
 *      Author: wjo
 */

#ifndef LCD_H_
#define LCD_H_

#define FB_HEIGHT (64)
#define FB_WIDTH (128)

#define FONT6X6 (0)
#define INVERT (0x80)

#define LCD_CENTER (64)
#define LCD_ALIGN_CENTER(x) (LCD_CENTER - (x * 3))
#define LCD_ALIGN_RIGHT(x) (127 - (x * 6))

void charoutsmall(uint8_t theChar, uint8_t X, uint8_t Y);
void LCD_disp_str(uint8_t* theStr, uint8_t theLen, uint8_t startx, uint8_t y, uint8_t theFormat);
void LCD_MultiLineH(uint8_t startx, uint8_t endx, uint64_t ymask);
uint8_t LCD_BMPDisplay(uint8_t* thebmp, uint8_t xoffset, uint8_t yoffset);
void LCD_SetPixel(uint8_t x, uint8_t y);
void LCD_SetBacklight(uint8_t backlight);
void LCD_Init(void);
void LCD_FB_Clear(void);
void LCD_FB_Update(void);

#endif /* LCD_H_ */
