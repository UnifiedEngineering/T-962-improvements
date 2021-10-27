/*
 * tft.c
 *  Basic TFT related function
 *  Created on: 25 paź 2021
 *      Author: s.bartkowicz
 */

#include "LPC214x.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sched.h"
#include "tft.h"
#include "display.h"

#ifdef USES_TFT_DISPLAY

static void TFT_WriteAdr8(uint32_t regindex) {
	uint32_t tmp;
	TFT_DATA_OUTPUTS;
	tmp = (FIO1PIN)&(0xff00ffff) ;	// short delay
	TFT_RS_COMMAND;
	TFT_WR_LOW;
	tmp|=regindex<<16;
	FIO1PIN = tmp;
	TFT_WR_HIGH;
}
static void TFT_WriteDta8(uint32_t dta) {
	uint32_t tmp;
	TFT_DATA_OUTPUTS;
	tmp = (FIO1PIN)&(0xff00ffff) ;	// short delay
	TFT_RS_DATA;
	TFT_WR_LOW;
	tmp|=dta<<16;
	FIO1PIN = tmp;
	TFT_WR_HIGH;

}

void TFT_Init(void) {

	TFT_CS_HIGH;
	TFT_RD_HIGH;
	TFT_WR_HIGH;
	TFT_RS_DATA;
	TFT_RST_HIGH;
	BusyWait(TICKS_MS(100));
	TFT_RST_LOW;
	BusyWait(TICKS_MS(10));
	TFT_RST_HIGH;
	BusyWait(TICKS_MS(300));

	TFT_CS_LOW;

	int tmp,i;
	i=0;
	tmp=0;
	tmp=TFT_init_sequence[i++];
	while (i<sizeof(TFT_init_sequence)||(tmp!=-3)) {
		if (tmp==-3) {
				break;
		} else if (tmp==-2) {
			tmp=TFT_init_sequence[i++];
			BusyWait(TICKS_MS(tmp));
			tmp=TFT_init_sequence[i++];
		} else if (tmp==-1) {
			tmp=TFT_init_sequence[i++];
			TFT_WriteAdr8(0x00);	// send a NOP command or high byte of address register depending on TFT controller selected
			TFT_WriteAdr8(tmp);
			tmp=TFT_init_sequence[i++];
			while (tmp>=0) {
				TFT_WriteDta8(tmp);
				tmp=TFT_init_sequence[i++];
			}
		}
	}
	TFT_CS_HIGH;


}

void TFT_FB_Clear(void) {
}

void TFT_Set_Region(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
#if TFT_CONTROLLER==ILI9325
	TFT_WriteAdr8(0x00);
	TFT_WriteAdr8(0x20);
	TFT_WriteDta8(0x00);
	TFT_WriteDta8(0x00);

	TFT_WriteAdr8(0x00);
	TFT_WriteAdr8(0x21);
	TFT_WriteDta8(0x00);
	TFT_WriteDta8(0x00);

	TFT_WriteAdr8(0x00);
	TFT_WriteAdr8(0x22);
#else
	TFT_WriteAdr8(0x00);
	TFT_WriteAdr8(0x2C);
#endif
}
#ifdef USES_FRAMEBUFFER

void TFT_FB_Update(uint8_t* fb) {
	uint32_t color=0;
	uint32_t x=0;
	uint32_t y=0;
	TFT_CS_LOW;
	TFT_Set_Region(0,0,FB_HEIGHT,FB_WIDTH);

	for(y = 0; y < FB_HEIGHT; y++) {
		for (x=0; x< FB_WIDTH; x++) {
			if (fb[(x>>3)+FB_BWIDTH*y]&(1<<((x & 0x07)))) {
					color=0xFFFF;
			} else {
				color=0;
			}
			TFT_WriteDta8(color>>8);
			TFT_WriteDta8(color&0xFF);
		}
	}
	TFT_CS_HIGH;
}
#endif	//uses framebuffer
#endif	//uses tft display
