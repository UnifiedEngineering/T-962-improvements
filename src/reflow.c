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
 * ---------------------------------------------------------------------
 *
 * Test setups and results, see Reflow_Init()
 *
 * 	PID_init(&PID, 10, 0.04, 5, PID_Direction_Direct); // This does not reach the setpoint fast enough
 * 	PID_init(&PID, 30, 0.2, 5, PID_Direction_Direct); // This reaches the setpoint but oscillates a bit especially during cooling
 * 	PID_init(&PID, 30, 0.2, 15, PID_Direction_Direct); // This overshoots the setpoint
 * 	PID_init(&PID, 25, 0.15, 15, PID_Direction_Direct); // This overshoots the setpoint slightly
 * 	PID_init(&PID, 20, 0.07, 25, PID_Direction_Direct);
 * 	PID_init(&PID, 20, 0.04, 25, PID_Direction_Direct); // Improvement as far as I can tell, still work in progress
 * 	PID_SetTunings(&PID, 80, 0, 0); // This results in oscillations with 14.5s cycle time
 * 	PID_SetTunings(&PID, 30, 0, 0); // This results in oscillations with 14.5s cycle time
 * 	PID_SetTunings(&PID, 15, 0, 0);
 * 	PID_SetTunings(&PID, 10, 0, 0); // no oscillations, but offset
 * 	PID_SetTunings(&PID, 10, 0.020, 0); // getting there
 * 	PID_SetTunings(&PID, 10, 0.013, 0);
 * 	PID_SetTunings(&PID, 10, 0.0066, 0);
 * 	PID_SetTunings(&PID, 10, 0.2, 0);
 * 	PID_SetTunings(&PID, 10, 0.020, 1.0); // Experimental
 *
 */

// #include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>

#include "t962.h"
#include "log.h"
#include "reflow_profiles.h"
#include "io.h"
#include "rtc.h"
#include "PID_v1.h"
#include "sched.h"
#include "nvstorage.h"
#include "sensor.h"
#include "reflow.h"
#include "buzzer.h"

#define STANDBYTEMP		45		// standby temperature in degree Celsius
#define PID_CYCLE_MS	250		// PID cycle is 250ms

static PidType PID;
static ReflowState_t reflow_state = REFLOW_STANDBY;

// default to no logging at all, needs serial communication (i.e. shell) anyway
static int reflow_log_level = LOG_NONE;
static ReflowInformation_t reflow_info;
static uint32_t loops_since_activation = 0;

// external interface

/*!
 * allow external world to enter bake mode
 * Note: this can be called repeatedly if the oven is still
 *  in BAKE_PREHEAT, however if the setpoint is set to
 *  below the current oven temperature the state is switched
 *  to BAKE and this call returns errors
 */
int Reflow_ActivateBake(int setpoint, int time)
{
	if (reflow_state != REFLOW_STANDBY && reflow_state != REFLOW_BAKE_PREHEAT)
		return -1;

	/* set or reset internal set */
	if (reflow_state != REFLOW_BAKE_PREHEAT)
		log(LOG_INFO, "Bake started setpoint: %3u, time: %ds", setpoint, time);

	loops_since_activation = 0;
	reflow_info.setpoint = setpoint;
	reflow_info.time_to_go = time;
	reflow_state = REFLOW_BAKE_PREHEAT;
	return 0;
}

/*!
 * allow external world to enter reflow mode
 */
int Reflow_ActivateReflow(void)
{
	if (reflow_state != REFLOW_STANDBY)
		return -1;

	loops_since_activation = 0;
	log(LOG_INFO, "Reflow started");
	reflow_state = REFLOW_REFLOW;
	reflow_info.time_to_go = 0;
	return 0;
}


/*!
 * stops running reflow or bake mode (if started)
 */
void Reflow_Abort(void)
{
	switch (reflow_state) {
	case REFLOW_REFLOW:
	case REFLOW_BAKE_PREHEAT:
	case REFLOW_BAKE:
		reflow_state = REFLOW_COOLING;
		log(LOG_INFO, "Reflow/Bake aborted");
		break;
	default:
		break;
	}
}

static const char const *mode_string[] = {
	[REFLOW_STANDBY] = "STANDBY",
	[REFLOW_REFLOW] = "REFLOW",
	[REFLOW_BAKE_PREHEAT] = "PREHEAT",
	[REFLOW_BAKE] = "BAKE",
	[REFLOW_COOLING] = "COOLING"
};

/*!
 * return a string (to be displayed or logged) that represents the
 * current mode
 */
const char *Reflow_ModeString(void)
{
	return mode_string[reflow_state];
}

bool Reflow_IsStandby(void)
{
	return reflow_state == REFLOW_STANDBY;
}

void Reflow_SetLogLevel(int lvl)
{
	reflow_log_level = lvl;
}

const ReflowInformation_t *Reflow_Information(void)
{
	return &reflow_info;
}

//// local helpers

/*!
 * get tick delta for 'near real-time intervals
 *   with the current settings a tick is about 127ns and uint32_t wraps
 *   every 545s.
 * return the tick value for the next cycle (PID_CYCLE_MS) to start
 *
 * TODO: this might come in handy for other tasks?! Be careful with TICKS_MS()
 *   if this is not constant it will use doubles!
 */
static uint32_t constant_time_interval(void)
{
	static uint32_t last_tick = 0;
	uint32_t this_tick = Sched_GetTick();

	// dynamically init static, might introduce a rare error during normal run
	if (last_tick == 0)
		last_tick = this_tick - TICKS_MS(PID_CYCLE_MS);

	uint32_t tick_delta = this_tick - last_tick;
	// tick_delta should be approximately one PID cycle, error if larger than 2!
	if (tick_delta > 2 * TICKS_MS(PID_CYCLE_MS)) {
		log(LOG_ERROR, "Reflow can't keep up with desired PID_CYCLE_MS!");
		return 0;
	}

	// constant interval
	last_tick += TICKS_MS(PID_CYCLE_MS);
	return 2 * TICKS_MS(PID_CYCLE_MS) - tick_delta;
}


// packed into 32bit, should fit nicely
typedef struct {
	uint16_t heater;
	uint16_t fan;
} heater_fan_t;

/*
 *  get response from PID controller, calculate fan and heater value
 *  from the output, coerce fan values.
 */
static heater_fan_t get_PID_response(uint16_t temperature, uint16_t setpoint)
{
	heater_fan_t hf = { 0, 0 };
	uint8_t min_fan = NV_GetConfig(REFLOW_MIN_FAN_SPEED);

	PID.mySetpoint = (float) setpoint;
	PID.myInput = (float) temperature;
	PID_Compute(&PID);
	uint16_t out = (uint16_t) PID.myOutput;

	// fan <-> heater magic here, PID output max is 255+248
	if (out < 248)
		hf.fan = 255 - out;
	else {
		hf.heater = out - 248;
		hf.fan = min_fan;	// at minimum while heating
	}

	// never lower than minimum (it will probably stall), turn it off
	if (hf.fan < min_fan)
		hf.fan = 0;

	return hf;
}

// set both heater and fan value
static void set_heater_fan(heater_fan_t hf)
{
	Set_Heater(hf.heater);
	Set_Secondary_Heater(hf.heater);
	Set_Fan(hf.fan);

	// current values to report
	reflow_info.heater = hf.heater;
	reflow_info.fan = hf.fan;
}

/*!
 * print a log if log level is high enough, if header is true add a header line
 * \todo implement json log output
 */
static void log_reflow(bool header, heater_fan_t hf)
{
	if (reflow_log_level >= LOG_VERBOSE ||
		(reflow_log_level >= LOG_INFO && reflow_state != REFLOW_STANDBY && reflow_state != REFLOW_COOLING)) {
		if (header)
			printf("\n# Time,  Temp0, Temp1, Temp2, Temp3,  Set,Actual, Heat, Fan,  ColdJ, Mode");
		printf("\n%6.1f,  %5.1f, %5.1f, %5.1f, %5.1f,  %5.1f, %5.1f,  %3u, %3u,  %5.1f, %s",
				(float) loops_since_activation * ((float) PID_CYCLE_MS / 1000.0),
				Sensor_GetTemp(TC_LEFT), Sensor_GetTemp(TC_RIGHT),
				Sensor_GetTemp(TC_EXTRA1), Sensor_GetTemp(TC_EXTRA2),
				reflow_info.setpoint, reflow_info.temperature,
				hf.heater, hf.fan,
				Sensor_GetTemp(TC_COLD_JUNCTION),
				Reflow_ModeString());
	}

}

/*
 * control the heater and fan using the PID, log without header
 *   .. it takes the temperature from the global information, but uses
 *   a local setpoint argument (as it might be a look-ahead value.
 */
static void control_heater_fan(float setpoint, bool force_heater_off)
{
	heater_fan_t hf = get_PID_response(reflow_info.temperature, setpoint);
	if (force_heater_off)
		hf.heater = 0;
	set_heater_fan(hf);
	log_reflow(false, hf);
}

// get this in seconds (no floats)
static inline uint32_t seconds_since_start(void)
{
	return loops_since_activation * PID_CYCLE_MS / 1000;
}

/*!
 * reflow worker, this implements the PID controller state machine
 *
 *   STANDBY: run the fan if temperature is still high (above 50 deg C)
 *   REFLOW:  run fan and heater according to the selected profile
 *   BAKE_PREHEAT: try to reach bake temperature
 *   BAKE: bake temperature is reached, count down timer
 *   ABORTING: turn all off and return to STANDBY mode
 */
static int32_t Reflow_Work(void)
{
	uint16_t value, value_in10s;
	static const heater_fan_t all_off = { 0, 0 };

	// get temperature
	Sensor_DoConversion();
	reflow_info.temperature = Sensor_GetTemp(TC_CONTROL);
	loops_since_activation++;

	switch(reflow_state) {
	case REFLOW_STANDBY:
		// turn off all
		set_heater_fan(all_off);
		log_reflow(false, all_off);
		break;
	case REFLOW_REFLOW:
		// get setpoint from profile and look-ahead value
		value = Reflow_GetSetpointAtTime(seconds_since_start());
		value_in10s = Reflow_GetSetpointAtTime(seconds_since_start() + 10);
		// reported value is the current setpoint, steering might be different
		reflow_info.setpoint = (float) value;

		if (value && value_in10s) {
			// use look-ahead if heating (it works better)
			if (value_in10s > value)
				value = value_in10s;
			reflow_info.time_done = seconds_since_start();
			control_heater_fan(value, false);
		} else {
			log(LOG_INFO, "Reflow done");
			Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			reflow_state = REFLOW_COOLING;
		}
		break;
	case REFLOW_BAKE_PREHEAT:
		if (reflow_info.setpoint < reflow_info.temperature) {
			loops_since_activation = 0;
			reflow_state = REFLOW_BAKE;
		}
		reflow_info.time_done = 0;
		control_heater_fan(reflow_info.setpoint, false);
		break;
	case REFLOW_BAKE:
		reflow_info.time_done = seconds_since_start();
		if (reflow_info.time_to_go < reflow_info.time_done) {
			log(LOG_INFO, "Bake done");
			Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			reflow_state = REFLOW_COOLING;
		}
		control_heater_fan(reflow_info.setpoint, false);
		break;
	case REFLOW_COOLING:
		// cool down to STANDBYTEMP, go full speed
		reflow_info.setpoint = (float) STANDBYTEMP;
		control_heater_fan(0, true);

		if (reflow_info.temperature < (float) STANDBYTEMP) {
			reflow_info.time_to_go = 0;
			reflow_state = REFLOW_STANDBY;
		}
		break;
	}

	return constant_time_interval();
}

void Reflow_Init(void)
{
	reflow_state = REFLOW_STANDBY;

	Reflow_InitNV();
	Sensor_InitNV();

	PID_init(&PID, 0, 0, 0, PID_Direction_Direct); // Can't supply tuning to PID_Init when not using the default timebase
	PID_SetSampleTime(&PID, PID_CYCLE_MS);
	PID_SetTunings(&PID, 20, 0.016, 62.5); // Adjusted values to compensate for the incorrect timebase earlier
	PID.mySetpoint = (float) SETPOINT_MIN;
	PID_SetOutputLimits(&PID, 0, 255 + 248);
	PID_SetMode(&PID, PID_Mode_Manual);
	PID.myOutput = 248; // Between fan and heat
	PID_SetMode(&PID, PID_Mode_Automatic);

	// Start work
	Sched_SetWorkfunc(REFLOW_WORK, Reflow_Work);
	Sched_SetState(REFLOW_WORK, 2, 0);
}
