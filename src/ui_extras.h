/*
 * ui_extras.h
 *
 *  Created on: 19 Feb 2019
 *      Author: scott porter
 *
 *  Support functions for UI enhancements, and screensaver mode
 */

#ifndef UI_EXTRAS_H_
#define UI_EXTRAS_H_

#define TOTAL_SPRITES	5

typedef struct {

	int16_t x;
	int16_t y;
	int16_t speed;
	int16_t dir;
	int16_t changeCnt;
	int16_t animFrame;
	int16_t	startFrame;
	int16_t	totalFrames;
	int16_t baseFrame;
	int16_t state;

} spriteStruct;

void showHeader(char *s);
void showBar(uint16_t v,uint8_t y);
void initSprites(void);
void drawSprites(void);
void displayReflowScreen(uint32_t keyspressed, uint8_t modeChange,uint8_t isDone);
uint8_t timeForScreensaver();
void initScreensaverTimeout();

#endif /* UI_EXTRAS_H_ */
