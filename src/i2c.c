/*
 * i2c.c - I2C interface for T-962 reflow controller
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
#include "t962.h"
#include "i2c.h"

// Limit to i2c speed 200kHz because of the relatively weak 4k7 pullups
#define I2CSPEED (200000)

void I2C_Init(void) {
	uint8_t dummybyte;
	I20SCLL = I20SCLH = PCLKFREQ / I2CSPEED / 2;
	I20CONCLR = 0xff;
	I20CONSET = (1 << 6); // I2EN
	I2C_Xfer(0xff, &dummybyte, 0, 1); // Dummy initial xfer
}

#define I2CSTART (0x08)
#define I2CRSTART (0x10)
#define I2CWAACK (0x18)
#define I2CWANOACK (0x20)
#define I2CWDACK (0x28)
#define I2CWDNOACK (0x30)
#define I2CARBLOST (0x38)
#define I2CRAACK (0x40)
#define I2CRANOACK (0x48)
#define I2CRDACK (0x50)
#define I2CRDNOACK (0x58)

int32_t I2C_Xfer(uint8_t slaveaddr, uint8_t* theBuffer, uint32_t theLength, uint8_t trailingStop) {
	int32_t retval = 0;
	int done = 0;
	uint8_t stat;

	I20CONSET = (1 << 5); // STA
	//printf("\n[STA]");

	while (!done) {
		while (!(I20CONSET & (1 << 3))); // SI
		stat = I20STAT;
		//printf("[0x%02x]", stat);
		switch(stat) {
			case I2CSTART:
			case I2CRSTART:
				I20DAT = slaveaddr;
				I20CONCLR = (1 << 5); // Clear STA
				//printf("[WADDR]");
				break;
			case I2CWANOACK:
			case I2CWDNOACK:
			case I2CARBLOST:
			case I2CRANOACK:
				//printf("[I2C error!]");
				trailingStop = 1; // Force STOP condition at the end no matter what
				retval = -1;
				done = 1;
				break;

			case I2CWAACK:
			case I2CWDACK:
				if (theLength) {
					I20DAT = *theBuffer++;
					theLength--;
					//printf("[WDATA]");
				} else {
					done=1;
				}
				break;

			case I2CRAACK:
				//printf("[RADDR]");
				I20CONSET = (1 << 2); // Set AA
				break;

			case I2CRDACK:
			case I2CRDNOACK:
				*theBuffer++ = I20DAT;
				theLength--;
				if (theLength == 1) {
					I20CONCLR = (1 << 2); // Clear AA for last byte
				} else if (theLength == 0) {
					done = 1;
				}
				//if(!done) printf("[RDATA]");
				break;
		}
		I20CONCLR = (1 << 3); // Clear SI
	}

	if (trailingStop) {
		I20CONSET = (1 << 4); // STO
		//printf("[STO]");
		while (I20CONSET & (1 << 4)); // Wait for STO to clear
	}
	return retval;
}
