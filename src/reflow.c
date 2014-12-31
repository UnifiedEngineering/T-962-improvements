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
#include "nvstorage.h"
#include "max31855.h"

// Normally the control input is the average of the first two TCs.
// By defining this any TC that has a readout 5C (or more) higher
// than the TC0 and TC1 average will be used as control input instead.
// Use if you have very sensitive components. Note that this will also
// kick in if the two sides of the oven has different readouts, as the
// code treats all four TCs the same way.
//#define MAXTEMPOVERRIDE

//#define RAMPTEST
#define STANDBYTEMP (50) // Standby temperature in degrees Celsius

#define PID_TIMEBASE (250) // 250ms between each run

float adcgainadj[2]; // Gain adjust, this may have to be calibrated per device if factory trimmer adjustments are off
float adcoffsetadj[2]; // Offset adjust, this will definitely have to be calibrated per device

extern uint8_t graphbmp[];
#define YAXIS (57)
#define XAXIS (12)

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

#define NUMPROFILETEMPS (48)

typedef struct {
	const char* name;
	const uint16_t temperatures[NUMPROFILETEMPS];
} profile;

typedef struct {
	const char* name;
	uint16_t temperatures[NUMPROFILETEMPS];
} ramprofile;

// Amtech 4300 63Sn/37Pb leaded profile
const profile am4300profile = { "4300 63SN/37PB",
	{50, 50, 50, 60, 73, 86,100,113,126,140,143,147,150,154,157,161,  // 0-150s
	164,168,171,175,179,183,195,207,215,207,195,183,168,154,140,125,  // Adjust peak from 205 to 220C
	111, 97, 82, 68, 54,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

// NC-31 low-temp lead-free profile
const profile nc31profile = { "NC-31 LOW-TEMP LF",
	{50, 50, 50, 50, 55, 70, 85, 90, 95,100,102,105,107,110,112,115,  // 0-150s
	117,120,122,127,132,138,148,158,160,158,148,138,130,122,114,106,  // Adjust peak from 158 to 165C
	 98, 90, 82, 74, 66, 58,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

// SynTECH-LF normal temp lead-free profile
const profile syntechlfprofile = { "AMTECH SYNTECH-LF",
	{50, 50, 50, 50, 60, 70, 80, 90,100,110,120,130,140,149,158,166,  // 0-150s
	175,184,193,201,210,219,230,240,245,240,230,219,212,205,198,191,  // Adjust peak from 230 to 249C
	184,177,157,137,117, 97, 77, 57,  0,  0,  0,  0,  0,  0,  0,  0}};// 320-470s

#ifdef RAMPTEST
// Ramp speed test temp profile
const profile rampspeedtestprofile = { "RAMP SPEED TEST",
	{50, 50, 50, 50,245,245,245,245,245,245,245,245,245,245,245,245,  // 0-150s
	245,245,245,245,245,245,245,245,245, 50, 50, 50, 50, 50, 50, 50,  // Adjust peak from 230 to 249C
	 50, 50, 50, 50, 50, 50, 50, 50,  0,  0,  0,  0,  0,  0,  0,  0}};// 320-470s
#endif

// EEPROM profile 1
ramprofile ee1 = { "CUSTOM #1" };

// EEPROM profile 2
ramprofile ee2 = { "CUSTOM #2" };

const profile* profiles[] = { &syntechlfprofile,
								&nc31profile,
								&am4300profile,
#ifdef RAMPTEST
								&rampspeedtestprofile,
#endif
								(profile*)&ee1, (profile*)&ee2 };
#define NUMPROFILES (sizeof(profiles)/sizeof(profiles[0]))
uint8_t profileidx=0;

static void ByteswapTempProfile(uint16_t* buf) {
	for(int i=0; i<NUMPROFILETEMPS; i++) {
		uint16_t word=buf[i];
		buf[i] = word>>8 | word << 8;
	}
}

static int32_t Reflow_Work( void ) {
	static ReflowMode_t oldmode = REFLOW_INITIAL;
	static uint16_t numticks = 0;
	static uint32_t lasttick = 0;
	uint16_t temp[2];
	uint8_t fan, heat;
	uint32_t ticks=RTC_Read();

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
			if(i>1) {
				temperature[i] = tctemp[i];
				tempvalid |= (1<<i);
			}
		} else {
			tcpresent[i] = SPI_IsTCPresent( i );
			if( tcpresent[i] ) {
				tctemp[i] = SPI_GetTCReading( i );
				tccj[i] = SPI_GetTCColdReading( i );
				if(i>1) {
					temperature[i] = tctemp[i];
					tempvalid |= (1<<i);
				}
			}
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
	}

#ifdef MAXTEMPOVERRIDE
	// If one of the temperature sensors reports higher than 5C above the average, use that as control input
	float newtemp = avgtemp;
	for( int i=0; i<4; i++) {
		if( tcpresent[i] && temperature[i] > (avgtemp+5.0f) && temperature[i] > newtemp ) {
			newtemp = temperature[i];
		}
	}
	if( avgtemp != newtemp ) {
		avgtemp = newtemp;
	}
#endif
	const char* modestr = "UNKNOWN";

	// Depending on mode we should run this with different parameters
	if(mymode == REFLOW_STANDBY || mymode == REFLOW_STANDBYFAN) {
		intsetpoint = STANDBYTEMP;
		Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint); // Cool to standby temp but don't heat to get there
		heat=0;
		if( mymode == REFLOW_STANDBY && avgtemp < (float)STANDBYTEMP ) fan = 0; // Suppress slow-running fan in standby
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

	if(mymode != oldmode) {
		printf("\n# Time,  Temp0, Temp1, Temp2, Temp3,  Set,Actual, Heat, Fan,  ColdJ, Mode");
		oldmode = mymode;
		numticks = 0;
	} else {
		if(mymode == REFLOW_BAKE || mymode == REFLOW_REFLOW) numticks++;
	}

	printf("\n%6.1f,  %5.1f, %5.1f, %5.1f, %5.1f,  %3u, %5.1f,  %3u, %3u,  %5.1f, %s",
			((float)numticks / (1000.0f/PID_TIMEBASE)),
			temperature[0], temperature[1],
			(tempvalid&(1<<2))?temperature[2]:0.0f, (tempvalid&(1<<3))?temperature[3]:0.0f,
			intsetpoint, avgtemp, heat, fan,
			cjsensorpresent?coldjunction:0.0f,
			modestr);

	intavgtemp = (int16_t)avgtemp; // Keep for UI

	if(numticks & 1) Sched_SetState( MAIN_WORK, 2, 0 ); // Force UI refresh every other cycle

	uint32_t thistick = Sched_GetTick();
	if (lasttick == 0) lasttick = thistick - TICKS_MS( PID_TIMEBASE );
	int32_t nexttick = (2 * TICKS_MS( PID_TIMEBASE )) - (thistick - lasttick);

	if ((thistick - lasttick) > (2 * TICKS_MS( PID_TIMEBASE ))) {
		printf("\nReflow can't keep up with desired PID_TIMEBASE!");
		nexttick = 0;
	}
	lasttick += TICKS_MS( PID_TIMEBASE );
	return nexttick;
}

void Reflow_ValidateNV(void) {
	int temp;

	if( NV_GetConfig(REFLOW_BEEP_DONE_LEN) == 255 ) {
		NV_SetConfig(REFLOW_BEEP_DONE_LEN, 10); // Default 1 second beep length
	}

	if( NV_GetConfig(REFLOW_MIN_FAN_SPEED) == 255 ) {
		NV_SetConfig(REFLOW_MIN_FAN_SPEED, 8); // Default fan speed is now 8
	}

	temp = NV_GetConfig(TC_LEFT_GAIN);
	if( temp == 255 ) {
		temp = 100;
		NV_SetConfig(TC_LEFT_GAIN, temp); // Default unity gain
	}
	adcgainadj[0] = ((float)temp) * 0.01f;

	temp = NV_GetConfig(TC_RIGHT_GAIN);
	if( temp == 255 ) {
		temp = 100;
		NV_SetConfig(TC_RIGHT_GAIN, temp); // Default unity gain
	}
	adcgainadj[1] = ((float)temp) * 0.01f;

	temp = NV_GetConfig(TC_LEFT_OFFSET);
	if( temp == 255 ) {
		temp = 100;
		NV_SetConfig(TC_LEFT_OFFSET, temp); // Default +/-0 offset
	}
	adcoffsetadj[0] = ((float)(temp-100)) * 0.25f;

	temp = NV_GetConfig(TC_RIGHT_OFFSET);
	if( temp == 255 ) {
		temp = 100;
		NV_SetConfig(TC_RIGHT_OFFSET, temp); // Default +/-0 offset
	}
	adcoffsetadj[1] = ((float)(temp-100)) * 0.25f;
	//printf("\nlo=%f lg=%f ro=%f, rg=%f ",adcoffsetadj[0], adcgainadj[0], adcoffsetadj[1], adcgainadj[1]);
}

void Reflow_Init(void) {
	Sched_SetWorkfunc( REFLOW_WORK, Reflow_Work );
	//PID_init(&PID,10,0.04,5,PID_Direction_Direct); // This does not reach the setpoint fast enough
	//PID_init(&PID,30,0.2,5,PID_Direction_Direct); // This reaches the setpoint but oscillates a bit especially during cooling
	//PID_init(&PID,30,0.2,15,PID_Direction_Direct); // This overshoots the setpoint
	//PID_init(&PID,25,0.15,15,PID_Direction_Direct); // This overshoots the setpoint slightly
	//PID_init(&PID,20,0.07,25,PID_Direction_Direct);
	PID_init(&PID,20,0.04,25,PID_Direction_Direct); // Improvement as far as I can tell, still work in progress
	EEPROM_Read((uint8_t*)ee1.temperatures, 2, 96);
	ByteswapTempProfile(ee1.temperatures);
	EEPROM_Read((uint8_t*)ee2.temperatures, 128+2, 96);
	ByteswapTempProfile(ee2.temperatures);
	Reflow_SelectProfileIdx(NV_GetConfig(REFLOW_PROFILE));
	Reflow_ValidateNV();
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
	for(int x=1; x<NUMPROFILETEMPS; x++) { // No need to plot first value as it is obscured by Y-axis
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
	NV_SetConfig(REFLOW_PROFILE, profileidx);
	return profileidx;
}

int Reflow_SelectEEProfileIdx(int idx) {
	if(idx==1) profileidx = (NUMPROFILES - 2);
	if(idx==2) profileidx = (NUMPROFILES - 1);
	return profileidx;
}

int Reflow_GetEEProfileIdx(void) {
	int retval = 0;
	if( profileidx == (NUMPROFILES - 2) ) retval = 1;
	if( profileidx == (NUMPROFILES - 1) ) retval = 2;
	return retval;
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
	if(idx>(NUMPROFILETEMPS-1)) return 0;
	return profiles[profileidx]->temperatures[idx];
}

void Reflow_SetSetpointAtIdx(uint8_t idx, uint16_t value) {
	if(idx>(NUMPROFILETEMPS-1)) return;
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
		if(idx<(NUMPROFILETEMPS-2)) {
			uint32_t value = profiles[profileidx]->temperatures[idx];
			uint32_t value2 = profiles[profileidx]->temperatures[idx+1];
			if(value>0 && value2>0) {
				uint32_t avg = (value*(10-offset) + value2*offset)/10;
				intsetpoint = avg; // Keep this for UI...
				if( value2 > avg ) { // Temperature is rising
					// Using the future value for PID regulation produces better result when heating
					PID.mySetpoint = (float)value2;
				} else {
					// Use the interpolated value when cooling
					PID.mySetpoint = (float)avg;
				}
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
		*pfan = NV_GetConfig(REFLOW_MIN_FAN_SPEED); // Run at a low fixed speed during heating for now
	}
	return retval;
}
