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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "onewire.h"
#include "sched.h"
#include "vic.h"

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
	uint32_t save = VIC_DisableIRQ();

	for (uint32_t bits = 0; bits < 8; bits++) {
		setpin0();
		BusyWait(TICKS_US(1.5)); // 1.5us
		if (thebyte&0x01) setpinhiz();
		thebyte >>= 1;
		BusyWait(TICKS_US(13));
		if (getpin()) thebyte |= 0x80;
		BusyWait(TICKS_US(45));
		setpinhiz();
		BusyWait(TICKS_US(10));
	}

	VIC_RestoreIRQ(save); //Enable interrupts
	return thebyte;
}

static inline uint32_t xferbit(uint32_t thebit) {
	uint32_t save = VIC_DisableIRQ();

	setpin0();
	BusyWait(TICKS_US(1.5));
	if (thebit) setpinhiz();
	thebit = 0;
	BusyWait(TICKS_US(13));
	if (getpin()) thebit = 0x01;
	BusyWait(TICKS_US(45));
	setpinhiz();
	BusyWait(TICKS_US(10));

	VIC_RestoreIRQ(save); //Enable interrupts
	return thebit;
}

static inline uint32_t resetbus(void) {
	uint32_t devicepresent = 0;
	uint32_t save = VIC_DisableIRQ();

	setpin0();
	BusyWait(TICKS_US(480));
	setpinhiz();
	BusyWait(TICKS_US(70));
	if (getpin() == 0) devicepresent = 1;
	BusyWait(TICKS_US(410));

	VIC_RestoreIRQ(save); //Enable interrupts
//	BusyWait(TICKS_US(500));
	return devicepresent;
}

#define OW_SEARCH_ROM (0xf0)
#define OW_READ_ROM (0x33)
#define OW_MATCH_ROM (0x55)
#define OW_SKIP_ROM (0xcc)
#define OW_CONVERT_T (0x44)
#define OW_WRITE_SCRATCHPAD (0x4e)
#define OW_READ_SCRATCHPAD (0xbe)
#define OW_FAMILY_TEMP1 (0x22) // DS1822
#define OW_FAMILY_TEMP2 (0x28) // DS18B20
#define OW_FAMILY_TEMP3 (0x10) // DS18S20
#define OW_FAMILY_TC (0x3b)

#define MAX_OW_DEVICES (5)
static uint8_t owdeviceids[MAX_OW_DEVICES][8]; // uint64_t results in really odd code
static int16_t devreadout[MAX_OW_DEVICES]; // Keeps last readout from each device
static int16_t extrareadout[MAX_OW_DEVICES]; // Keeps last readout from each device
static int numowdevices = 0;
static int8_t tcidmapping[16]; // Map TC ID to ROM ID index
static int8_t tempidx; // Which ROM ID index that contains the temperature sensor

// OW functions from Application note 187 (modified for readability)
// global search state
static uint8_t ROM_NO[8];
static int LastDiscrepancy;
static int LastFamilyDiscrepancy;
static int LastDeviceFlag;
static uint8_t crc8;

static const unsigned char dscrc_table[] = {
	0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
};

/*
 * Calculate the CRC8 of the byte value provided with the current
 * global 'crc8' value.
 * Returns current global crc8 value
 */
static uint8_t docrc8(uint8_t value) {
	// See Application Note 27

	// TEST BUILD
	crc8 = dscrc_table[crc8 ^ value];
	return crc8;
}

/* Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
 * search state.
 * Return TRUE : device found, ROM number in ROM_NO buffer
 * FALSE : device not found, end of search
 */
static int OWSearch() {
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	uint8_t rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	crc8 = 0;
	// if the last call was not the last one
	if (!LastDeviceFlag) {
		// 1-Wire reset
		if (!resetbus()) { // No devices found
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = false;
			LastFamilyDiscrepancy = 0;
			return false;
		}
		// issue the search command
		xferbyte(OW_SEARCH_ROM);
		// loop to do the search
		do {
			// read a bit and its complement
			id_bit = xferbit( true );
			cmp_id_bit = xferbit( true );
			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1)) {
				break;
			} else {
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit) {
					search_direction = id_bit; // bit write value for search
				} else {
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy) {
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					} else {
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);
					}
					// if 0 was picked then record its position in LastZero
					if (search_direction == 0) {
						last_zero = id_bit_number;
						// check for Last discrepancy in family
						if (last_zero < 9) {
							LastFamilyDiscrepancy = last_zero;
						}
					}
				}
				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1) {
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				} else {
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;
				}
				// serial number search direction write bit
				xferbit(search_direction);
				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;
				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0) {
					docrc8(ROM_NO[rom_byte_number]); // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		} while(rom_byte_number < 8); // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (crc8 != 0))) {
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;
			// check for last device
			if (LastDiscrepancy == 0) {
				LastDeviceFlag = true;
			}
			search_result = true;
		}
	}
	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !ROM_NO[0]) {
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		LastFamilyDiscrepancy = 0;
		search_result = false;
	}
	return search_result;
}

/*
 * Find the 'first' devices on the 1-Wire bus
 * Return TRUE : device found, ROM number in ROM_NO buffer
 * FALSE : no device present
 */
static int OWFirst() {
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = false;
	LastFamilyDiscrepancy = 0;
	return OWSearch();
}

/*
 * Find the 'next' devices on the 1-Wire bus
 * Return TRUE : device found, ROM number in ROM_NO buffer
 * FALSE : device not found, end of search
 */
static int OWNext() {
	// leave the search state alone
	return OWSearch();
}

static void selectdevbyidx(int idx) {
	if (idx < numowdevices) {
//		uint32_t save = VIC_DisableIRQ(); //Disable interrupts
		resetbus();
//		VIC_RestoreIRQ(save); //Enable interrupts
//		save = VIC_DisableIRQ();
		xferbyte(OW_MATCH_ROM);
//		VIC_RestoreIRQ(save);
		for (int idloop = 0; idloop < 8; idloop++) { // Send ROM device ID
//			save = VIC_DisableIRQ();
			xferbyte(owdeviceids[idx][idloop]);
//			VIC_RestoreIRQ(save);
		}
	}
}

static int32_t OneWire_Work(void) {
	static uint8_t mystate = 0;
	uint8_t scratch[9];
	int32_t retval = 0;

	if (mystate == 0) {
//		uint32_t save = VIC_DisableIRQ();
		if (resetbus()) {
//			VIC_RestoreIRQ(save);
//			save = VIC_DisableIRQ();
			xferbyte(OW_SKIP_ROM); // All devices on the bus are addressed here
//			VIC_RestoreIRQ(save);
//			save = VIC_DisableIRQ();
			xferbyte(OW_CONVERT_T);
			setpin1();
			//retval = TICKS_MS(94); // For 9-bit resolution
			retval = TICKS_MS(100); // TC interface needs max 100ms to be ready
			mystate++;
		}
//		VIC_RestoreIRQ( save );
	} else if (mystate == 1) {
		for (int i = 0; i < numowdevices; i++) {
			//uint32_t save = VIC_DisableIRQ();
			selectdevbyidx(i);
//			uint32_t save = VIC_DisableIRQ();
			xferbyte(OW_READ_SCRATCHPAD);
//			VIC_RestoreIRQ(save);
			for (uint32_t iter = 0; iter < 4; iter++) { // Read four bytes
//				save = VIC_DisableIRQ();
				scratch[iter] = xferbyte(0xff);
//				VIC_RestoreIRQ(save);
			}
			//VIC_RestoreIRQ(save);
			int16_t tmp = scratch[1]<<8 | scratch[0];
			devreadout[i] = tmp;
			tmp = scratch[3]<<8 | scratch[2];
			extrareadout[i] = tmp;
		}
		mystate = 0;
	} else {
		retval = -1;
	}

	return retval;
}

uint32_t OneWire_Init(void) {
	printf("\n%s called", __FUNCTION__);
	Sched_SetWorkfunc(ONEWIRE_WORK, OneWire_Work);
	printf("\nScanning 1-wire bus...");

	tempidx = -1; // Assume we don't find a temperature sensor
	for (int i = 0; i < sizeof(tcidmapping); i++) {
		tcidmapping[i] = -1; // Assume we don't find any thermocouple interfaces
	}

//	uint32_t save = VIC_DisableIRQ();
	int rslt = OWFirst();
//	VIC_RestoreIRQ( save );

	numowdevices = 0;
	while (rslt && numowdevices < MAX_OW_DEVICES) {
		memcpy(owdeviceids[numowdevices], ROM_NO, sizeof(ROM_NO));
		numowdevices++;
//		save = VIC_DisableIRQ();
		rslt = OWNext();
//		VIC_RestoreIRQ( save );
	}

	if (numowdevices) {
		for (int iter = 0; iter < numowdevices; iter++) {
			printf("\n Found ");
			for (int idloop = 7; idloop >= 0; idloop--) {
				printf("%02x", owdeviceids[iter][idloop]);
			}
			uint8_t family = owdeviceids[iter][0];
			if (family == OW_FAMILY_TEMP1 || family == OW_FAMILY_TEMP2 || family == OW_FAMILY_TEMP3) {
				const char* sensorname = "UNKNOWN";
				if (family == OW_FAMILY_TEMP1) {
					sensorname = "DS1822";
				} else if (family == OW_FAMILY_TEMP2) {
					sensorname = "DS18B20";
				} else if (family == OW_FAMILY_TEMP3) {
					sensorname = "DS18S20";
				}
//				save = VIC_DisableIRQ();
				selectdevbyidx(iter);
				xferbyte(OW_WRITE_SCRATCHPAD);
				xferbyte(0x00);
				xferbyte(0x00);
				xferbyte(0x1f); // Reduce resolution to 0.5C to keep conversion time reasonable
//				VIC_RestoreIRQ(save);
				tempidx = iter; // Keep track of where we saw the last/only temperature sensor
				printf(" [%s Temperature sensor]", sensorname);
			} else if (family == OW_FAMILY_TC) {
//				save = VIC_DisableIRQ();
				selectdevbyidx(iter);
				xferbyte(OW_READ_SCRATCHPAD);
				xferbyte(0xff);
				xferbyte(0xff);
				xferbyte(0xff);
				xferbyte(0xff);
				uint8_t tcid = xferbyte(0xff) & 0x0f;
//				VIC_RestoreIRQ( save );
				tcidmapping[tcid] = iter; // Keep track of the ID mapping
				printf(" [Thermocouple interface, ID %x]",tcid);
			}
		}
	} else {
		printf(" No devices found!");
	}

	if (numowdevices) {
		Sched_SetState(ONEWIRE_WORK, 2, 0); // Enable OneWire task if there's at least one device
	}
	return numowdevices;
}

float OneWire_GetTempSensorReading(void) {
	float retval = 999.0f; // Report invalid temp if not found
	if(tempidx >= 0) {
		retval = (float)devreadout[tempidx];
		retval /= 16;
		//printf(" (%.1f C)",retval);
	} else {
		//printf(" (%.1f C assumed)",retval);
	}
	return retval;
}

int OneWire_IsTCPresent(uint8_t tcid) {
	if (tcid < sizeof(tcidmapping) && tcidmapping[tcid] >= 0) {
		if (!(devreadout[tcidmapping[tcid]] & 0x01)) {
			// A faulty/not connected TC will not be flagged as present
			return 1;
		}
	}
	return 0;
}

float OneWire_GetTCReading(uint8_t tcid) {
	float retval = 0.0f; // Report 0C for missing sensors
	if (tcid < sizeof(tcidmapping)) {
		uint8_t idx = tcidmapping[tcid];
		if (idx >=0) { // Is this ID present?
			if (devreadout[idx] & 0x01) { // Fault detected
				retval = 999.0f; // Invalid
			} else {
				retval = (float)(devreadout[idx] & 0xfffc); // Mask reserved bit
				retval /= 16;
				//printf(" (%x=%.1f C)",tcid,retval);
			}
		}
	}
	return retval;
}

float OneWire_GetTCColdReading(uint8_t tcid) {
	float retval = 0.0f; // Report 0C for missing sensors
	if (tcid < sizeof(tcidmapping)) {
		uint8_t idx = tcidmapping[tcid];
		if (idx >=0) { // Is this ID present?
			if (extrareadout[idx] & 0x07) { // Any fault detected
				retval = 999.0f; // Invalid
			} else {
				retval = (float)(extrareadout[idx] & 0xfff0); // Mask reserved/fault bits
				retval /= 256;
				//printf(" (%x=%.1f C)",tcid,retval);
			}
		}
	}
	return retval;
}
