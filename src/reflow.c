/*
 * reflow.c - Actual reflow profile logic for T-962 reflow controller
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
#include "reflow.h"
#include "eeprom.h"
#include "io.h"
#include "lcd.h"
#include "rtc.h"
#include "PID_v1.h"

extern uint8_t graphbmp[];
#define YAXIS (57)
#define XAXIS (12)

uint16_t profilebuf[48];
PidType PID;
uint16_t intsetpoint;

typedef struct {
	const char* name;
	const uint16_t temperatures[48];
} profile;

typedef struct {
	const char* name;
	uint16_t temperatures[48];
} ramprofile;

// Amtech 4300 63Sn/37Pb leaded profile
const profile am4300profile = { "4300 63SN/37PB",
	{40, 40, 47, 60, 73, 86,100,113,126,140,143,147,150,154,157,161,  // 0-150s
	164,168,171,175,179,183,195,207,215,207,195,183,168,154,140,125,  // Adjust peak from 205 to 220C
	111, 97, 82, 68, 54, 40, 25,  0,  0,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

// NC-31 low-temp lead-free profile
const profile nc31profile = { "NC-31 LOW-TEMP LF",
	{40, 40, 40, 40, 55, 70, 85, 90, 95,100,102,105,107,110,112,115,  // 0-150s
	117,120,122,127,132,138,148,158,160,158,148,138,130,122,114,106,  // Adjust peak from 158 to 165C
	 98, 90, 82, 74, 66, 58, 50, 42, 34,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

// SynTECH-LF normal temp lead-free profile
const profile syntechlfprofile = { "AMTECH SYNTECH-LF",
	{40, 40, 40, 50, 60, 70, 80, 90,100,110,120,130,140,149,158,166,  // 0-150s
	175,184,193,201,210,219,230,240,245,240,230,219,212,205,198,191,  // Adjust peak from 230 to 249C
	184,177,157,137,117, 97, 77, 57, 37,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

// EEPROM profile 1
ramprofile ee1 = { "CUSTOM #1" };

// EEPROM profile 2
ramprofile ee2 = { "CUSTOM #2" };

const profile* profiles[] = { &syntechlfprofile, &nc31profile, &am4300profile , (profile*)&ee1, (profile*)&ee2 };
#define NUMPROFILES (sizeof(profiles)/sizeof(profiles[0]))
uint8_t profileidx=0;

static void ByteswapTempProfile(uint16_t* buf) {
	for(int i=0; i<48; i++) {
		uint16_t word=buf[i];
		buf[i] = word>>8 | word << 8;
	}
}

void Reflow_Init(void) {
//	PID_init(&PID,16,0.1,2,PID_Direction_Direct);
	PID_init(&PID,17,0.11,2,PID_Direction_Direct);
	EEPROM_Read((uint8_t*)ee1.temperatures, 2, 96);
	ByteswapTempProfile(ee1.temperatures);
	EEPROM_Read((uint8_t*)ee2.temperatures, 128+2, 96);
	ByteswapTempProfile(ee2.temperatures);
	intsetpoint = 30;
	PID.mySetpoint = 30.0f; // Default setpoint
	PID_SetOutputLimits(&PID, 0,255+248);
	PID_SetMode(&PID, PID_Mode_Manual);
	PID.myOutput = 248; // Between fan and heat
	PID_SetMode(&PID, PID_Mode_Automatic);
	RTC_Zero();
}

void Reflow_PlotProfile() {
	LCD_BMPDisplay(graphbmp,0,0);
	for(int x=1; x<48; x++) { // No need to plot first value as it is obscured by Y-axis
		int realx = (x << 1) + XAXIS;
		int y=profiles[profileidx]->temperatures[x] / 5;
		y = YAXIS-y;
		LCD_SetPixel(realx,y);
	}
}

int Reflow_GetProfileIdx(void) {
	return profileidx;
}

int Reflow_SelectProfileIdx(int idx) {
	if(idx < 0) {
		profileidx = (NUMPROFILES - 1);
	} else if(idx >= NUMPROFILES) {
		profileidx = 0;
	} else {
		profileidx = idx;
	}
	return profileidx;
}

const char* Reflow_GetProfileName(void) {
	return profiles[profileidx]->name;
}

uint16_t Reflow_GetSetpoint(void) {
	return intsetpoint;
}

int32_t Reflow_Run(uint32_t thetime, float meastemp, uint8_t* pheat, uint8_t* pfan, int32_t manualsetpoint) {
	int32_t retval=0;
	if(manualsetpoint) {
		PID.mySetpoint = (float)manualsetpoint;
	} else { // Figure out what setpoint to use from the profile, brute-force way. Fix this.
		uint8_t idx = thetime / 10;
		uint16_t start = idx * 10;
		uint16_t offset = thetime-start;
		if(idx<47) {
			uint32_t value = profiles[profileidx]->temperatures[idx];
			if(value>0) {
				uint32_t value2 = profiles[profileidx]->temperatures[idx+1];
				uint32_t avg = (value*(10-offset) + value2*offset)/10;
				printf(" setpoint %uC",avg);
				intsetpoint = avg; // Keep this for UI
				PID.mySetpoint = (float)avg;
			} else {
				retval = -1;
			}
		} else {
			retval = -1;
		}
	}

	if(!manualsetpoint) {
		// Plot actual temperature on top of desired profile
		int realx = (thetime / 5) + XAXIS;
		int y=(uint16_t)(meastemp * 0.2f);
		y = YAXIS-y;
		LCD_SetPixel(realx,y);
	}

	PID.myInput=meastemp;
	PID_Compute(&PID);
	uint32_t out = PID.myOutput;
	if(out<248) { // Fan in reverse
		*pfan=255-out;
		*pheat=0;
	} else {
		*pheat=out-248;
		if(*pheat>192) *pfan=2; else *pfan=4; // When heating like crazy make sure we can reach our setpoint
	}
	return retval;
}
