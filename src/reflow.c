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

// modified by akita11 (akita@ifdl.jp), 2022/02/16: PID control and keep temperature for specified period

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

uint8_t reflow_state = 0;

/*
{ T1a, T1b, t1 }

control heatr&fan for temperature going T1a, when temperature reaches T1b, retain for t1
*/

uint16_t reflow_profile[] = {
  190, 150, 100,
  240, 230, 10,
  50, 80, 20,
};
uint8_t num_reflow_state = sizeof(reflow_profile) / sizeof(uint16_t) / 3;
uint16_t target_temp = 0;
uint16_t threshold_temp = 0;
uint8_t f_retain = 0;
uint32_t thetime_retain_start = 0;
uint16_t duration = 0;

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
	//uint32_t out;
	int16_t out;
	// Depending on mode we should run this with different parameters
	if (mymode == REFLOW_STANDBY || mymode == REFLOW_STANDBYFAN) {
		intsetpoint = STANDBYTEMP;
		// Cool to standby temp but don't heat to get there
		Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint, &out);
		heat = 0;

		// Suppress slow-running fan in standby
		if (mymode == REFLOW_STANDBY && avgtemp < (float)STANDBYTEMP) {
			 fan = 0;
		}
		modestr = "STANDBY";

	} else if(mymode == REFLOW_BAKE) {
		reflowdone = Reflow_Run(0, avgtemp, &heat, &fan, intsetpoint, &out) ? 1 : 0;
		modestr = "BAKE";

	} else if(mymode == REFLOW_REFLOW) {
		reflowdone = Reflow_Run(ticks, avgtemp, &heat, &fan, 0, &out) ? 1 : 0;
		modestr = "REFLOW";

	} else {
		heat = fan = 0;
	}
	Set_Heater(heat);
	Set_Fan(fan);

	if (mymode != oldmode) {
	  printf("\n# Time, Temp0, Temp1, Temp2, Temp3, Set_a, Set_b, Act'l, st, fr, Heat, Fan,   out, ColdJ, Mode");
	  oldmode = mymode;
	  numticks = 0;
	  reflow_state = 0;
	  PID.ITerm = 0.0;
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
	  //printf("\n%6.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %2d, %2d, %3u, %3u,  %5.1f, %s",
	  //	  printf("\n%6.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %5.1f, %2d, %2d, %3u, %3u,  %5d, %5.1f, %s",
	  /*
	  printf("\n%6.1f, %5.1f, %5.1f, %5.1f, %3d, %2d, %2d, %3u, %3u,  %5d",
		 ((float)numticks / TICKS_PER_SECOND),
		 (float)target_temp,
		 (float)threshold_temp, 
		 avgtemp,
		 duration, 
		 reflow_state,
		 f_retain,
		 heat,
		 fan,
		 out);
	  */
	  printf("\n%6.1f %5.1f %5.1f %5.1f %3d %2d %2d %3u %3u  %5d",
		 ((float)numticks / TICKS_PER_SECOND),
		 avgtemp,
		 (float)target_temp,
		 (float)threshold_temp, 
		 duration, 
		 reflow_state,
		 f_retain,
		 heat,
		 fan,
		 out);
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
	//	PID_SetTunings(&PID, 20, 0.016, 62.5); // Adjusted values to compensate for the incorrect timebase earlier
	PID_SetTunings(&PID, 30, 0.016, 60); // tentative by akita11
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
int32_t Reflow_Run(uint32_t thetime, float meastemp, uint8_t* pheat, uint8_t* pfan, int32_t manualsetpoint, int16_t *out_val) {
	target_temp = reflow_profile[reflow_state * 3 + 0];
	threshold_temp = reflow_profile[reflow_state * 3 + 1];
	duration = reflow_profile[reflow_state * 3 + 2];

	PID.mySetpoint = (float)target_temp;
	PID.outMax = 500.0;
	PID.outMin = -500.0;

	FloatType out = 0.0;
	PID.myInput = meastemp;
	PID_Compute(&PID);
	out = PID.myOutput;

	if (f_retain == 0){
	  FloatType Tdiff;
	  if (meastemp > threshold_temp) Tdiff = meastemp - threshold_temp;
	  else Tdiff = threshold_temp - meastemp;
	  if (Tdiff < 2){ f_retain = 1; thetime_retain_start = thetime; }
	}
	else if (f_retain == 1 && thetime > thetime_retain_start + duration){
	  // retain finished
	  reflow_state++;
	  f_retain = 0;
	  PID.ITerm = 0.0;
	}

	if (out > 0.0){
	  if (out > 255) *pheat = 255;
	  else *pheat = (uint8_t)out;
	  *pfan = 0;
	}
	else{
	  FloatType outm = -out;
	  if (outm > 255) *pfan = 255;
	  else *pfan = (uint8_t)outm;
	  *pheat = 0;
	}

	*out_val = (int16_t)out;
	
	if (reflow_state == num_reflow_state) return(-1); else return(0);
}


void Reflow_ToggleStandbyLogging(void) {
	standby_logging = !standby_logging;
}
