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
#include "sched.h"
#include "onewire.h"
#include "adc.h"

float adcgainadj[2] = { 1.0f, 1.0f }; // Gain adjust, this may have to be calibrated per device if factory trimmer adjustments are off
float adcoffsetadj[2] = { -6.0f, -5.0f }; // Offset adjust, this will definitely have to be calibrated per device

extern uint8_t graphbmp[];
#define YAXIS (57)
#define XAXIS (12)

uint16_t profilebuf[48];
PidType PID;
uint16_t intsetpoint;
float avgtemp; // The feedback temperature
float coldjunction;
float temperature[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
uint8_t cjsensorpresent = 0;
uint8_t tempvalid = 0;
int16_t intavgtemp;
uint8_t reflowdone = 0;
ReflowMode_t mymode = REFLOW_STANDBY;

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

static int32_t Reflow_Work( void ) {
	uint16_t temp[2];
	uint8_t fan, heat;
	uint32_t ticks=RTC_Read();

	printf("\n");

	// These are the temperature readings we get from the thermocouple interfaces
	// Right now it is assumed that if they are indeed present the first two channels will be used as feedback
	float tctemp[4], tccj[4];
	uint8_t tcpresent[4];
	tempvalid = 0; // Assume no valid readings;
	for( int i=0; i<4; i++ ) { // Get 4 TC channels
		tcpresent[i] = OneWire_IsTCPresent( i );
		if( tcpresent[i] ) {
			tctemp[i] = OneWire_GetTCReading( i );
			tccj[i] = OneWire_GetTCColdReading( i );
			printf("TC%x=%5.1fC ",i,tctemp[i]);
			if(i>1) {
				temperature[i] = tctemp[i];
				tempvalid |= (1<<i);
			}
		} else {
			//printf("TC%x=---C ",i);
		}
	}
	cjsensorpresent = 0; // Assume no CJ sensor
	if(tcpresent[0] && tcpresent[1]) {
		avgtemp = (tctemp[0] + tctemp[1]) / 2.0f;
		temperature[0] = tctemp[0];
		temperature[1] = tctemp[1];
		tempvalid |= 0x03;
		coldjunction = (tccj[0] + tccj[1]) / 2.0f;
		cjsensorpresent = 1;
		printf("CJ=%5.1fC ",coldjunction);
	} else {
		// If the external TC interface is not present we fall back to the built-in ADC, with or without compensation
		coldjunction=OneWire_GetTempSensorReading();
		if( coldjunction < 127.0f ) {
			cjsensorpresent = 1;
		} else {
			coldjunction = 25.0f; // Assume 25C ambient if not found
		}
		temp[0]=ADC_Read(1);
		temp[1]=ADC_Read(2);
		//printf("(ADC readout 0x%04x 0x%04x) ",temp[0],temp[1]);
		temperature[0] = ((float)temp[0]) / 16.0f; // ADC oversamples to supply 4 additional bits of resolution
		temperature[1] = ((float)temp[1]) / 16.0f;

		temperature[0] *= adcgainadj[0]; // Gain adjust
		temperature[1] *= adcgainadj[1];

		temperature[0] += coldjunction + adcoffsetadj[0]; // Offset adjust
		temperature[1] += coldjunction + adcoffsetadj[1];

		tempvalid |= 0x03;

		avgtemp=(temperature[0]+temperature[1]) / 2.0f;
		printf("L=%5.1fC R=%5.1fC CJ=%5.1fC ",
			temperature[0],temperature[1],coldjunction);
	}

	const char* modestr = "UNKNOWN";

	// Depending on mode we should run this with different parameters
	if(mymode == REFLOW_STANDBY) {
		intsetpoint = 30;
		Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint); // Keep at 30C but don't heat to get there in standby
		heat=0;
		if( fan < 16 ) fan = 0; // Suppress slow-running fan in standby
		modestr = "STANDBY";
	} else if(mymode == REFLOW_BAKE) {
		Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint);
		modestr = "BAKE";
	} else if(mymode == REFLOW_REFLOW) {
		reflowdone = Reflow_Run(ticks, avgtemp, &heat, &fan, 0)?1:0;
		modestr = "REFLOW";
	} else {
		heat = fan = 0;
	}
	Set_Heater(heat);
	Set_Fan(fan);

	printf("Setpoint=%3uC Actual=%5.1fC Heat=0x%02x Fan=0x%02x Mode=%s",
		intsetpoint,avgtemp,heat,fan,modestr);

	intavgtemp = (int16_t)avgtemp; // Keep for UI

	return TICKS_MS( 250 );
}

void Reflow_Init(void) {
	Sched_SetWorkfunc( REFLOW_WORK, Reflow_Work );
//	PID_init(&PID,16,0.1,2,PID_Direction_Direct);
//	PID_init(&PID,17,0.11,2,PID_Direction_Direct);
	PID_init(&PID,10,0.04,5,PID_Direction_Direct);
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
	Sched_SetState( REFLOW_WORK, 2, 0 ); // Start work
}

void Reflow_SetMode(ReflowMode_t themode) {
	mymode = themode;
}

void Reflow_SetSetpoint(uint16_t thesetpoint) {
	intsetpoint = thesetpoint;
}

int16_t Reflow_GetActualTemp(void) {
	return intavgtemp;
}

float Reflow_GetTempSensor(TempSensor_t sensor) {
	if(sensor == TC_COLD_JUNCTION) {
		return coldjunction;
	} else if(sensor == TC_AVERAGE) {
		return avgtemp;
	} else if(sensor < TC_NUM_ITEMS) {
		return temperature[sensor - TC_LEFT];
	} else {
		 return 0.0f;
	}
}

uint8_t Reflow_IsTempSensorValid(TempSensor_t sensor) {
	if(sensor == TC_COLD_JUNCTION) {
		return cjsensorpresent;
	} else if(sensor == TC_AVERAGE) {
		return 1;
	} else if(sensor >= TC_NUM_ITEMS) {
		return 0;
	}
	return (tempvalid & (1<<(sensor - TC_LEFT))) ? 1:0;
}

uint8_t Reflow_IsDone(void) {
	return reflowdone;
}

void Reflow_PlotProfile(int highlight) {
	LCD_BMPDisplay(graphbmp,0,0);
	for(int x=1; x<48; x++) { // No need to plot first value as it is obscured by Y-axis
		int realx = (x << 1) + XAXIS;
		int y=profiles[profileidx]->temperatures[x] / 5;
		y = YAXIS-y;
		LCD_SetPixel(realx,y);
		if(highlight == x) {
			LCD_SetPixel(realx-1,y-1);
			LCD_SetPixel(realx+1,y+1);
			LCD_SetPixel(realx-1,y+1);
			LCD_SetPixel(realx+1,y-1);
		}
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

int Reflow_SelectEEProfileIdx(int idx) {
	if(idx==1) profileidx = (NUMPROFILES - 2);
	if(idx==2) profileidx = (NUMPROFILES - 1);
	return profileidx;
}

int Reflow_SaveEEProfile(void) {
	int retval = 0;
	uint8_t offset;
	uint16_t* tempptr;
	if(profileidx == (NUMPROFILES - 2)) {
		offset = 0;
		tempptr = ee1.temperatures;
	} else if( profileidx == (NUMPROFILES - 1)) {
		offset = 128;
		tempptr = ee2.temperatures;
	} else {
		return -1;
	}
	offset += 2; // Skip "magic"
	ByteswapTempProfile(tempptr);
	retval = EEPROM_Write(offset, (uint8_t*)tempptr, 96); // Store
	ByteswapTempProfile(tempptr);
	return retval;
}

const char* Reflow_GetProfileName(void) {
	return profiles[profileidx]->name;
}

uint16_t Reflow_GetSetpointAtIdx(uint8_t idx) {
	if(idx>47) return 0;
	return profiles[profileidx]->temperatures[idx];
}

void Reflow_SetSetpointAtIdx(uint8_t idx, uint16_t value) {
	if(idx>47) return;
	if(value>300) return;
	uint16_t* temp = (uint16_t*)&profiles[profileidx]->temperatures[idx];
	if(temp>=(uint16_t*)0x40000000) *temp = value; // If RAM-based
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
				intsetpoint = avg; // Keep this for UI...
				//PID.mySetpoint = (float)avg;
				PID.mySetpoint = (float)value2; // ...but using the future value for PID regulation produces better result
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
		//if(*pheat>192) *pfan=2; else *pfan=2; // When heating like crazy make sure we can reach our setpoint
		*pfan=2; // Run at a low fixed speed during heating for now
	}
	return retval;
}
