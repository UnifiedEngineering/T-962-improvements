/*
 * eeprom.c - I2C EEPROM interface for T-962 reflow controller
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

//#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
//#include "t962.h"
#include "eeprom.h"
//#include "i2c.h"

#define EEADDR (0x50<<1)
//#define DUMP_EEPROM

static I2C_HandleTypeDef* s_i2c;

void EEPROM_Init(I2C_HandleTypeDef* i2c) {
	s_i2c = i2c;
	#ifdef DUMP_EEPROM
		EEPROM_Dump();
	#endif
	// No init needed at this point, maybe detect the actual presence some day
}

void EEPROM_Dump(void) {
	uint8_t dumpbuf[256];
	for (int j = 0; j < 32; j++) {
		EEPROM_Read(dumpbuf, j * 0x100, sizeof(dumpbuf));
		printf("EEPROM contents page %u:", j);
		for (int i = 0; i < sizeof(dumpbuf); i++) {
			if ((i & 0x0f) == 0){
				printf("\r\n0x%04x:", i + (j * 0x100));
			}
			printf(" %02x", dumpbuf[i]);
		}
		printf("\r\n");
	}
}

int32_t EEPROM_Read(uint8_t* dest, uint32_t startpos, uint32_t len) {
	int32_t retval = 0;
	if (startpos < 8192 && dest && len && len <= 8192) {

		HAL_StatusTypeDef halstatus = HAL_I2C_Mem_Read(s_i2c, EEADDR, startpos, I2C_MEMADD_SIZE_16BIT, dest, len, HAL_MAX_DELAY);
		if (halstatus != HAL_OK) {
			printf ("i2c error %x\r\n", halstatus);
		}
	}
	return retval;
}
int32_t EEPROM_Write(uint32_t startdestpos, uint8_t* src, uint32_t len) {
#if 0
	int32_t retval = 0;
	if (startdestpos < 256 && len && len <= 256) {
		uint8_t tmpbuf[9];
		uint8_t i = startdestpos;
		while (len) {
			uint32_t loopcnt = 0;
			uint8_t startoffset = i & 0x07;
			uint8_t maxcopysize = 8 - startoffset;
			// up to 8 bytes at a time depending on alignment
			uint8_t bytestocopy = (len > maxcopysize) ? maxcopysize : len;
			tmpbuf[0] = i;
			memcpy(tmpbuf + 1, src, bytestocopy);
			// Set address pointer and provide up to 8 bytes of data for page write
			retval = I2C_Xfer(EEADDR, tmpbuf, bytestocopy + 1, 1);
			if (!retval) {
				do {
					// Dummy write to poll timed write cycle completion
					retval = I2C_Xfer(EEADDR, tmpbuf, 1, 1);
					loopcnt++;
					// 5ms max write cycle. 200kHz bus freq & 10 bits per poll makes this a 20ms timeout
				} while (retval && loopcnt < 400);
				if (retval) {
					printf("\nTimeout getting ACK from EEPROM during write!");
					break;
				}
				len -= bytestocopy;
				i += bytestocopy;
				src += bytestocopy;
			} else {
				printf("\nFailed to write to EEPROM!");
				retval = -2;
				break;
			}
		}
	} else {
		printf("\nInvalid EEPROM addressing");
		retval = -3;
	}
	return retval;
#else
	printf("No eeprom write yet\r\n");
	return -3;
#endif
}
