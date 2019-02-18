/*
 * max31855.c - SPI TC interface handling for T-962 reflow controller
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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "sc18is602b.h"
#include "sched.h"

#define INVALID_VALUE (999.0f)

#define MAX_SPI_DEVICES (4)
static int16_t spidevreadout[MAX_SPI_DEVICES]; // Keeps last readout from each device
static int16_t spiextrareadout[MAX_SPI_DEVICES]; // Keeps last readout from each device
static int numspidevices = 0;

static int32_t SPI_TC_Work( void ) {
	for (int i = 0; i < numspidevices; i++) {
		SPIxfer_t xfer;
		xfer.ssmask = 1 << i;
		xfer.len = 4; // 32 bits from TC interface

		// Doesn't matter what data contains when sending, MOSI not connected
		if (SC18IS602B_SPI_Xfer(&xfer) == 0) {
			int16_t tmp = xfer.data[0] << 8 | xfer.data[1];
			spidevreadout[i] = tmp;

			tmp = xfer.data[2] << 8 | xfer.data[3];
			spiextrareadout[i] = tmp;
		}
	}
	return TICKS_MS(100);
}

uint32_t SPI_TC_Init(void) {
	printf("\n%s called", __FUNCTION__);
	Sched_SetWorkfunc(SPI_TC_WORK, SPI_TC_Work);

	for (int i = 0; i < MAX_SPI_DEVICES; i++) {
		spidevreadout[i] = -1; // Assume we don't find any thermocouple interfaces
		spiextrareadout[i] = -1;
	}

	// Only continue of we find the I2C to SPI bridge chip
	if (SC18IS602B_Init(SPICLK_1843KHZ, SPIMODE_0, SPIORDER_MSBFIRST) >= 0) {
		printf("\nProbing for MAX31855 devices...");

		// Assume all devices are present for SPI_TC_Work
		numspidevices = MAX_SPI_DEVICES;

		// Run one iteration to update all data
		SPI_TC_Work();

		// And reset it afterwards
		numspidevices = 0;

		for (int i = 0; i < MAX_SPI_DEVICES; i++) {
			if ((spidevreadout[i] == -1 && spiextrareadout[i] == -1) ||
			    (spidevreadout[i] == 0 && spiextrareadout[i] == 0)) {
				//printf(" Unknown/Invalid/Absent device");
			} else {
				printf("\nSS%x: [SPI Thermocouple interface]", i);
				// A bit of a hack as it assumes all earlier devices are present
				numspidevices = i + 1;
			}
		}

		if (numspidevices) {
			 // Enable SPI TC task if there's at least one device
			Sched_SetState(SPI_TC_WORK, 2, 0);
		} else {
			printf(" No MAX31855 devices found!");
		}
	}
	return numspidevices;
}

int SPI_IsTCPresent(uint8_t tcid) {
	if (tcid < numspidevices) {
		if (!(spidevreadout[tcid] & 0x01)) {
			 // A faulty/not connected TC will not be flagged as present
			 return 1;
		}
	}
	return 0;
}

float SPI_GetTCReading(uint8_t tcid) {
	// Report 0C for missing sensors
	float retval = 0.0f;
	if (tcid < numspidevices) {
		if (spidevreadout[tcid] & 0x01) { // Fault detected
			retval = INVALID_VALUE;
		} else {
			retval = (float)(spidevreadout[tcid] & 0xfffc); // Mask reserved bit
			retval /= 16;
		}
	}
	return retval;
}

float SPI_GetTCColdReading(uint8_t tcid) {
	// Report 0C for missing sensors
	float retval = 0.0f;
	if (tcid < numspidevices) {
		if (spiextrareadout[tcid] & 0x07) { // Any fault detected
			retval = INVALID_VALUE;
		} else {
			retval = (float)(spiextrareadout[tcid] & 0xfff0); // Mask reserved/fault bits
			retval /= 256;
		}
	}
	return retval;
}
