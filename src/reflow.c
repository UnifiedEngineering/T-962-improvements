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
#include "reflow_profiles.h"
#include "io.h"
#include "lcd.h"
#include "rtc.h"
#include "PID_v1.h"
#include "sched.h"
#include "nvstorage.h"
#include "sensor.h"
#include "reflow.h"

// Standby temperature in degrees Celsius
#define STANDBYTEMP (50)

// 250ms between each run
#define PID_TIMEBASE (250)

#define TICKS_PER_SECOND (1000 / PID_TIMEBASE)

static PidType PID;

static uint16_t intsetpoint;
static int bake_timer = 0;

static float avgtemp;

static uint8_t reflowdone = 0;
static ReflowMode_t mymode = REFLOW_STANDBY;
static uint16_t numticks = 0;

static int standby_logging = 0;

static int32_t Reflow_Work(void) {
	static ReflowMode_t oldmode = REFLOW_INITIAL;
	static uint32_t lasttick = 0;
	uint8_t fan, heat;
	uint32_t ticks = RTC_Read();

	Sensor_DoConversion();
	avgtemp = Sensor_GetTemp(TC_AVERAGE);

	const char* modestr = "UNKNOWN";

	// Depending on mode we should run this with different parameters
	if (mymode == REFLOW_STANDBY || mymode == REFLOW_STANDBYFAN) {
		intsetpoint = STANDBYTEMP;
		// Cool to standby temp but don't heat to get there
		Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint);
		heat = 0;

		// Suppress slow-running fan in standby
		if (mymode == REFLOW_STANDBY && avgtemp < (float)STANDBYTEMP) {
			 fan = 0;
		}
		modestr = "STANDBY";

	} else if(mymode == REFLOW_BAKE) {
		reflowdone = Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint) ? 1 : 0;
		modestr = "BAKE";

	} else if(mymode == REFLOW_REFLOW) {
		reflowdone = Reflow_Run(ticks, avgtemp, &heat, &fan, 0) ? 1 : 0;
		modestr = "REFLOW";

	} else {
		heat = fan = 0;
	}
	Set_Heater(heat);
	Set_Fan(fan);

	if (mymode != oldmode) {
		printf("\n# Time,  Temp0, Temp1, Temp2, Temp3,  Set,Actual, Heat, Fan,  ColdJ, Mode");
		oldmode = mymode;
		numticks = 0;
	} else if (mymode == REFLOW_BAKE) {
		if (bake_timer > 0 && numticks >= bake_timer) {
			printf("\n DONE baking, set bake timer to 0.");
			bake_timer = 0;
			Reflow_SetMode(REFLOW_STANDBY);
		}

		// start increasing ticks after setpoint is reached...
		if (avgtemp < intsetpoint && bake_timer > 0) {
			modestr = "BAKE-PREHEAT";
		} else {
			numticks++;
		}
	} else if (mymode == REFLOW_REFLOW) {
		numticks++;
	}

	if (!(mymode == REFLOW_STANDBY && standby_logging == 0)) {
		printf("\n%6.1f,  %5.1f, %5.1f, %5.1f, %5.1f,  %3u, %5.1f,  %3u, %3u,  %5.1f, %s",
		       ((float)numticks / TICKS_PER_SECOND),
		       Sensor_GetTemp(TC_LEFT),
		       Sensor_GetTemp(TC_RIGHT),
		       Sensor_GetTemp(TC_EXTRA1),
		       Sensor_GetTemp(TC_EXTRA2),
		       intsetpoint, avgtemp,
		       heat, fan,
		       Sensor_GetTemp(TC_COLD_JUNCTION),
		       modestr);
	}

	if (numticks & 1) {
		// Force UI refresh every other cycle
		Sched_SetState(MAIN_WORK, 2, 0);
	}

	uint32_t thistick = Sched_GetTick();
	if (lasttick == 0) {
		lasttick = thistick - TICKS_MS(PID_TIMEBASE);
	}

	int32_t nexttick = (2 * TICKS_MS(PID_TIMEBASE)) - (thistick - lasttick);
	if ((thistick - lasttick) > (2 * TICKS_MS(PID_TIMEBASE))) {
		printf("\nReflow can't keep up with desired PID_TIMEBASE!");
		nexttick = 0;
	}
	lasttick += TICKS_MS(PID_TIMEBASE);
	return nexttick;
}

void Reflow_Init(void) {
	Sched_SetWorkfunc(REFLOW_WORK, Reflow_Work);
	//PID_init(&PID, 10, 0.04, 5, PID_Direction_Direct); // This does not reach the setpoint fast enough
	//PID_init(&PID, 30, 0.2, 5, PID_Direction_Direct); // This reaches the setpoint but oscillates a bit especially during cooling
	//PID_init(&PID, 30, 0.2, 15, PID_Direction_Direct); // This overshoots the setpoint
	//PID_init(&PID, 25, 0.15, 15, PID_Direction_Direct); // This overshoots the setpoint slightly
	//PID_init(&PID, 20, 0.07, 25, PID_Direction_Direct);
	//PID_init(&PID, 20, 0.04, 25, PID_Direction_Direct); // Improvement as far as I can tell, still work in progress
	PID_init(&PID, 0, 0, 0, PID_Direction_Direct); // Can't supply tuning to PID_Init when not using the default timebase
	PID_SetSampleTime(&PID, PID_TIMEBASE);
	PID_SetTunings(&PID, 20, 0.016, 62.5); // Adjusted values to compensate for the incorrect timebase earlier
	//PID_SetTunings(&PID, 80, 0, 0); // This results in oscillations with 14.5s cycle time
	//PID_SetTunings(&PID, 30, 0, 0); // This results in oscillations with 14.5s cycle time
	//PID_SetTunings(&PID, 15, 0, 0);
	//PID_SetTunings(&PID, 10, 0, 0); // no oscillations, but offset
	//PID_SetTunings(&PID, 10, 0.020, 0); // getting there
	//PID_SetTunings(&PID, 10, 0.013, 0);
	//PID_SetTunings(&PID, 10, 0.0066, 0);
	//PID_SetTunings(&PID, 10, 0.2, 0);
	//PID_SetTunings(&PID, 10, 0.020, 1.0); // Experimental

	Reflow_LoadCustomProfiles();

	Reflow_ValidateNV();
	Sensor_ValidateNV();

	Reflow_LoadSetpoint();

	PID.mySetpoint = (float)SETPOINT_DEFAULT;
	PID_SetOutputLimits(&PID, 0, 255 + 248);
	PID_SetMode(&PID, PID_Mode_Manual);
	PID.myOutput = 248; // Between fan and heat
	PID_SetMode(&PID, PID_Mode_Automatic);
	RTC_Zero();

	// Start work
	Sched_SetState(REFLOW_WORK, 2, 0);
}

void Reflow_SetMode(ReflowMode_t themode) {
	mymode = themode;
	// reset reflowdone if mode is set to standby.
	if (themode == REFLOW_STANDBY)  {
		reflowdone = 0;
	}
}

void Reflow_SetSetpoint(uint16_t thesetpoint) {
	intsetpoint = thesetpoint;

	NV_SetConfig(REFLOW_BAKE_SETPOINT_H, (uint8_t)(thesetpoint >> 8));
	NV_SetConfig(REFLOW_BAKE_SETPOINT_L, (uint8_t)thesetpoint);
}

void Reflow_LoadSetpoint(void) {
	intsetpoint = NV_GetConfig(REFLOW_BAKE_SETPOINT_H) << 8;
	intsetpoint |= NV_GetConfig(REFLOW_BAKE_SETPOINT_L);

	printf("\n bake setpoint values: %x, %x, %d\n",
		NV_GetConfig(REFLOW_BAKE_SETPOINT_H),
		NV_GetConfig(REFLOW_BAKE_SETPOINT_L), intsetpoint);
}

int16_t Reflow_GetActualTemp(void) {
	return (int)Sensor_GetTemp(TC_AVERAGE);
}

uint8_t Reflow_IsDone(void) {
	return reflowdone;
}

uint16_t Reflow_GetSetpoint(void) {
	return intsetpoint;
}

void Reflow_SetBakeTimer(int seconds) {
	// reset ticks to 0 when adjusting timer.
	numticks = 0;
	bake_timer = seconds * TICKS_PER_SECOND;
}

int Reflow_IsPreheating(void) {
	return bake_timer > 0 && avgtemp < intsetpoint;
}

int Reflow_GetTimeLeft(void) {
	if (bake_timer == 0) {
		return -1;
	}
	return (bake_timer - numticks) / TICKS_PER_SECOND;
}

// returns -1 if the reflow process is done.
int32_t Reflow_Run(uint32_t thetime, float meastemp, uint8_t* pheat, uint8_t* pfan, int32_t manualsetpoint) {
	int32_t retval = 0;

	if (manualsetpoint) {
		PID.mySetpoint = (float)manualsetpoint;

		if (bake_timer > 0 && (Reflow_GetTimeLeft() == 0 || Reflow_GetTimeLeft() == -1)) {
			retval = -1;
		}
	} else {
		// Figure out what setpoint to use from the profile, brute-force way. Fix this.
		uint8_t idx = thetime / 10;
		uint16_t start = idx * 10;
		uint16_t offset = thetime - start;
		if (idx < (NUMPROFILETEMPS - 2)) {
			uint16_t value = Reflow_GetSetpointAtIdx(idx);
			uint16_t value2 = Reflow_GetSetpointAtIdx(idx + 1);

			if (value > 0 && value2 > 0) {
				uint16_t avg = (value * (10 - offset) + value2 * offset) / 10;

				// Keep the setpoint for the UI...
				intsetpoint = avg;
				if (value2 > avg) {
					// Temperature is rising,
					// using the future value for PID regulation produces better result when heating
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

	if (!manualsetpoint) {
		// Plot actual temperature on top of desired profile
		int realx = (thetime / 5) + XAXIS;
		int y = (uint16_t)(meastemp * 0.2f);
		y = YAXIS - y;
		LCD_SetPixel(realx, y);
	}

	PID.myInput = meastemp;
	PID_Compute(&PID);
	uint32_t out = PID.myOutput;
	if (out < 248) { // Fan in reverse
		*pfan = 255 - out;
		*pheat = 0;
	} else {
		*pheat = out - 248;

		// When heating like crazy make sure we can reach our setpoint
		// if(*pheat>192) { *pfan=2; } else { *pfan=2; }

		// Run at a low fixed speed during heating for now
		*pfan = NV_GetConfig(REFLOW_MIN_FAN_SPEED);
	}
	return retval;
}

void Reflow_ToggleStandbyLogging(void) {
	standby_logging = !standby_logging;
}
