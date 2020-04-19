/*
 * ui_extras.c
 *
 *  Created on: 19 Feb 2019
 *      Author: scott
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lcd.h"
#include "rtc.h"
#include "sensor.h"
#include "reflow.h"
#include "reflow_profiles.h"
#include "keypad.h"
#include "nvstorage.h"
#include "setup.h"

#include "ui_extras.h"

spriteStruct sprite[TOTAL_SPRITES];

extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t selectbmp[];
extern uint8_t editbmp[];
extern uint8_t f3editbmp[];
extern uint8_t graph2bmp[];
extern uint8_t exitbmp[];

uint8_t animTicker=0;
uint8_t spriteAnimIX=0;
uint8_t blinkCnt=0,blinkOn=0;

int16_t totalReflowTicks=0;
int8_t reflowDisplay=0;

uint16_t screensaverTimeout=30;
uint16_t screensaverCnt=0;
uint16_t screensaverScore=0;
uint8_t screensaverScoreMul[]={1,2,4,16};

void showHeader(char *s){
	LCD_disp_str((uint8_t*)" ", 1, 0, 0, FONT6X6);
	LCD_disp_str((uint8_t*)"<<<<<<<<", 8, 5-animTicker, 0, FONT6X6);

	LCD_disp_str((uint8_t*)" ", 1, (20*6)+2, 0, FONT6X6);
	LCD_disp_str((uint8_t*)">>>>>>>>", 8, (12*6)+2+animTicker, 0, FONT6X6);

	uint8_t len = strlen(s);
	LCD_disp_str((uint8_t*)s, len, LCD_ALIGN_CENTER(len), 6 * 0, FONT6X6);

	if(++animTicker==6){
		animTicker=0;
	}
}

void showBar(uint16_t v,uint8_t y){
	if(v>52)
		v=52;
	v=52-v;
	uint8_t n,i;
	for(n=0;n<6;n++){
		for(i=0;i<v;i++){
			LCD_ClearPixel(i+7-n,y+n);
		}
	}
}

void initSprites(void){

	screensaverScore=0;

	int16_t initA[5][7]={
		{-80,  24, 2, 0, 3, 4, 0},
		{42,   63, 1, 1, 1, 2, 6},
		{62,   70, 1, 1, 1, 2, 6},
		{82,  127, 1, 1, 2, 2, 6},
		{102, 141, 1, 1, 2, 2, 6},
	};

	for(uint8_t n=0;n<TOTAL_SPRITES;n++){
		// dirs: 0=left,1=up,2=right,3=down
		sprite[n].x=			initA[n][0];
		sprite[n].y=			initA[n][1];
		sprite[n].dir=			initA[n][2];
		sprite[n].changeCnt=	(rand()%10)+70;
		sprite[n].animFrame=	initA[n][3];
		sprite[n].speed=		initA[n][4];
		sprite[n].startFrame=	initA[n][3];
		sprite[n].totalFrames=	initA[n][5];
		sprite[n].baseFrame=	initA[n][6];
		sprite[n].state=		0;	// 0=normal, >0=eaten - value is score (100,200,800,1600)
	}

}

void drawSprites(void){

	uint8_t animFrame,spriteHorizFlip;

	LCD_FB_Clear();

	if(spriteAnimIX>10){
		LCD_drawBigNum(screensaverScore,4,34,1,SPRITE9X16);
		LCD_DrawSprite(0,74,1,SPRITE9X16);
		LCD_DrawSprite(0,84,1,SPRITE9X16);
	}

	if(++spriteAnimIX>20)
		spriteAnimIX=0;

	for(uint8_t n=0;n<TOTAL_SPRITES;n++){

		if(sprite[n].state==0){

			if((n==0) || (spriteAnimIX&1)){
				if(++sprite[n].animFrame==sprite[n].totalFrames+sprite[n].startFrame){
					sprite[n].animFrame=sprite[n].startFrame;
				}
			}

			animFrame=sprite[n].animFrame;
			if(animFrame==3)
				animFrame=1;	// Special case for pacman :)

			spriteHorizFlip=0;

			switch(sprite[n].dir){

			case 0:	// left
				sprite[n].x-=sprite[n].speed;
				if(sprite[n].x<-15)
					sprite[n].x+=128+16;
				spriteHorizFlip=FLIP_HORIZONTAL;
				break;

			case 1: // up
				sprite[n].y-=sprite[n].speed;
				if(sprite[n].y<-15)
					sprite[n].y+=64+16;
				if(animFrame>0)
					animFrame+=2;
				break;

			case 2:	// right
				sprite[n].x+=sprite[n].speed;
				if(sprite[n].x>127)
					sprite[n].x-=128+16;
				break;

			case 3:	// down
				sprite[n].y+=sprite[n].speed;
				if(sprite[n].y>63)
					sprite[n].y-=64+16;
				if(animFrame>0)
					animFrame+=4;
				break;
			}

			if(--sprite[n].changeCnt<=0){
				sprite[n].changeCnt=(rand()%(60/sprite[n].speed))+20;
				if((rand()%2)==0){
					--sprite[n].dir;
				}else{
					++sprite[n].dir;
				}
				if(sprite[n].dir<0)
					sprite[n].dir=3;
				else if(sprite[n].dir>3)
					sprite[n].dir=0;
			}

			// Check for collision between ghost and pacman - pacman eats ghosts :)
			if(n>0 && sprite[n].state==0){
				if(!(sprite[n].x<sprite[0].x-10 || sprite[n].x>sprite[0].x+10 || sprite[n].y<sprite[0].y-10 || sprite[n].y>sprite[0].y+10)){
					uint8_t s=1;
					for(uint8_t i=1;i<5;i++){
						if(i!=n && sprite[i].state>0)
							++s;
					}
					sprite[n].state=s;
					screensaverScore+=screensaverScoreMul[s-1];
				}
			}
		}else{
			spriteHorizFlip=0;
			animFrame=6+sprite[n].state;
			if(--sprite[n].y<-10){
				sprite[n].state=0;
				switch(rand()%4){
				case 0:
					sprite[n].x=rand()%112;
					sprite[n].y=-15;
					sprite[n].dir=3;
					break;
				case 1:
					sprite[n].x=rand()%112;
					sprite[n].y=63;
					sprite[n].dir=1;
					break;
				case 2:
					sprite[n].y=rand()%48;
					sprite[n].x=-15;
					sprite[n].dir=2;
					break;
				case 3:
					sprite[n].y=rand()%48;
					sprite[n].x=127;
					sprite[n].dir=0;
					break;
				}
			}
		}

		LCD_DrawSprite(animFrame+sprite[n].baseFrame,sprite[n].x,sprite[n].y,SPRITE16X16|spriteHorizFlip);
	}
}

void displayReflowScreen(uint32_t keyspressed, uint8_t modeChange,uint8_t isDone){
	uint32_t ticks = RTC_Read();
	int8_t len=0;
	char buf[22];

	if(++blinkCnt>=5){
		blinkOn=(blinkOn==0?1:0);
		blinkCnt=0;
	}

	if (keyspressed & KEY_F1) {
		if(++reflowDisplay>=TOTAL_REFLOW_DISPLAYS)
			reflowDisplay=0;
	}
	if (keyspressed & KEY_F2) {
		Reflow_TogglePause();
		// Pause timer, to allow heat to soak at current set point
	}

	if(modeChange){
		// Work out total number of ticks (seconds) for profile
		uint8_t n;
		for(n=0;n<NUMPROFILETEMPS-1;n++){
			if(Reflow_GetSetpointAtIdx(n)==0 && Reflow_GetSetpointAtIdx(n+1)==0){
				totalReflowTicks=n-1;
				break;
			}
		}
		if(totalReflowTicks<=0){
			totalReflowTicks=n;
		}
		totalReflowTicks*=10;
	}

	int16_t diff=Reflow_GetSetpoint()-Reflow_GetActualTemp();

	if((isDone==1) || (reflowDisplay==0)){
		Reflow_PlotDots();

		if(isDone==1){
			LCD_BMPDisplay(exitbmp, 128 - 18, 64-18);
			LCD_disp_str((uint8_t*)"COMPLETED", 9, LCD_ALIGN_CENTER(9), 0, (blinkOn==1?FONT6X6|INVERT:FONT6X6));
		}else{
			LCD_BMPDisplay(stopbmp, 128 - 18, 0);

			len = snprintf(buf, sizeof(buf), "SET %03u", Reflow_GetSetpoint());
			LCD_disp_str((uint8_t*)buf, len, 15, 0, (diff>5 && blinkOn==1?FONT6X6|INVERT:FONT6X6));

			len = snprintf(buf, sizeof(buf), "ACTUAL %03u", Reflow_GetActualTemp());

			LCD_disp_str((uint8_t*)buf, len, 68, 0, (diff<-5 && blinkOn==1?FONT6X6|INVERT:FONT6X6));

			len = snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned int)(ticks/60),(unsigned int)(ticks%60));

			LCD_disp_str((uint8_t*)buf, len, 98, 7, (Reflow_IsPaused() && blinkOn==1)?FONT6X6|INVERT:FONT6X6);
		}
	}else if(reflowDisplay==1){
		LCD_FB_Clear();
		LCD_BMPDisplay(graph2bmp, 0, 0);

		uint16_t v=(((13000*4)/200)*Reflow_GetSetpoint())/1000;	// use Sensor_GetTemp() for float
		showBar(v,1);

		v=(((13000*4)/200)*Reflow_GetActualTemp())/1000;
		showBar(v,24);

		v=((52000/totalReflowTicks)*(unsigned int)ticks)/1000;
		showBar(52-v,47);

		if(blinkOn==1 && diff>5){
			LCD_DrawSprite(10,63,1,SPRITE9X16);
			LCD_DrawSprite(10,73,1,SPRITE9X16);
			LCD_DrawSprite(10,83,1,SPRITE9X16);
			LCD_DrawSprite(10,96,1,SPRITE9X16);
		}else{
			LCD_drawBigNum(Reflow_GetSetpoint(),3, 63,1,SPRITE9X16|INVERT);
			LCD_DrawSprite(0,96,1,SPRITE9X16|INVERT);
		}

		uint16_t t=(Sensor_GetTemp(TC_AVERAGE)*10);
		if(blinkOn==1 && diff<-5){
			LCD_DrawSprite(10,63,24,SPRITE9X16);
			LCD_DrawSprite(10,73,24,SPRITE9X16);
			LCD_DrawSprite(10,83,24,SPRITE9X16);
			LCD_DrawSprite(10,96,24,SPRITE9X16);
		}else{
			LCD_drawBigNum(t/10,3, 63,24,SPRITE9X16|INVERT);
			LCD_DrawSprite((int8_t)(t%10),96,24,SPRITE9X16|INVERT);
		}

		if(Reflow_IsPaused() && blinkOn==1){
			LCD_DrawSprite(10,63,47,SPRITE9X16);
			LCD_DrawSprite(10,73,47,SPRITE9X16);
			LCD_DrawSprite(10,86,47,SPRITE9X16);
			LCD_DrawSprite(10,96,47,SPRITE9X16);
		}else{
			LCD_drawBigNum((totalReflowTicks-ticks)/60,2, 63,47,SPRITE9X16|INVERT);
			LCD_drawBigNum((totalReflowTicks-ticks)%60,2, 86,47,SPRITE9X16|INVERT);
		}

	}
}

uint8_t timeForScreensaver(){
	if(screensaverTimeout==0)	// disabled
		return 0;
	if(++screensaverCnt>=screensaverTimeout){
		screensaverCnt=0;
		return 1;
	}
	return 0;
}

void initScreensaverTimeout(){
	screensaverCnt=0;
	screensaverTimeout=NV_GetConfig(SCREENSAVER_ACTIVE)*600;	// Outside range of floats, so can't use the Setup_getValue method
}
