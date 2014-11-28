/*
 * onewire.c - Bitbang 1-wire DS18B20 temp-sensor handling for T-962 reflow controller
 *
 * Copyright (C) 2014 Werner Johansson, wj@unifiedengineering.se
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
#include <stdio.h>
#include "onewire.h"
#include "timer.h"

static inline void setpin0() {
	FIO0CLR = (1<<7);
	FIO0DIR |= (1<<7);
}

static inline void setpin1() {
	FIO0SET = (1<<7);
	FIO0DIR |= (1<<7);
}

static inline void setpinhiz() {
	FIO0DIR &= ~(1<<7);
}

static inline uint32_t getpin() {
	return !!(FIO0PIN & (1<<7));
}

static inline uint32_t xferbyte(uint32_t thebyte) {
	for(uint32_t bits=0; bits<8; bits++) {
		setpin0();
		BusyWait8(12); // 1.5us
		if(thebyte&0x01) setpinhiz();
		thebyte >>= 1;
		BusyWait8(13<<3);
		if(getpin()) thebyte |= 0x80;
		BusyWait8(45<<3);
		setpinhiz();
		BusyWait8(10<<3);
	}
	return thebyte;
}

static inline uint32_t resetbus(void) {
	uint32_t devicepresent=0;

	setpin0();
	BusyWait8(480<<3);
	setpinhiz();
	BusyWait8(70<<3);
	if(getpin() == 0) devicepresent=1;
	BusyWait8(410<<3);

	return devicepresent;
}
#define OW_READ_ROM (0x33)
#define OW_SKIP_ROM (0xcc)
#define OW_CONVERT_T (0x44)
#define OW_WRITE_SCRATCHPAD (0x4e)
#define OW_READ_SCRATCHPAD (0xbe)
#define OW_FAMILY_TEMP (0x28)

uint32_t OneWire_Init( void ) {
	uint32_t devicepresent=0;
	uint32_t tempsensor=0;

	printf("\n%s called",__FUNCTION__);

	devicepresent = resetbus();

	if(devicepresent) {
		printf("\n1-wire device found : ");
		xferbyte(OW_READ_ROM);
		for(uint32_t bah=0;bah<8;bah++) {
			uint8_t data=xferbyte(0xff);
			printf("%02x ",data);
			if(bah==0 && data==OW_FAMILY_TEMP) tempsensor=1;
		}
		if(tempsensor) {
			printf("(it's a temperature sensor!)");
			xferbyte(OW_WRITE_SCRATCHPAD);
			xferbyte(0x00);
			xferbyte(0x00);
			xferbyte(0x1f); // Reduce resolution to 0.5C
		}
	}
	return tempsensor;
}

// Move temp sensor stuff out of onewire
float OneWire_GetTemperature(void) {
	uint8_t scratch[9];
	float retval = 0.0f;

	if(resetbus()) {
		//printf("\nStarting temperature measurement");
		xferbyte(OW_SKIP_ROM);
		xferbyte(OW_CONVERT_T);
		setpin1();
		//BusyWait8(750000<<3); // For 12-bit resolution
		BusyWait8(94000<<3); // For 9-bit resolution
		//setpinhiz();
		resetbus();
		xferbyte(OW_SKIP_ROM);
		//printf("\nDone: ");
		xferbyte(OW_READ_SCRATCHPAD);
		for(uint32_t iter=0; iter<2; iter++) { // Only read two bytes
			scratch[iter] = xferbyte(0xff);
			//printf("%02x ",scratch[iter]);
		}
		int16_t tmp = scratch[1]<<8 | scratch[0];
		retval=(float)tmp;
		retval/=16;
		printf(" (%.1f C)",retval);
	} else {
		printf("\nNo sensor found!");
		retval = 25.0f; // Compatibility with ovens without cold-junction mod, assume 25C ambient
		BusyWait8(94000<<3); // Maintain timing
	}
	return retval;
}
