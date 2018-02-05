/*
 * systemfan.c - Temperature controlled system fan handling for T-962 reflow controller
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

/*
 * This utilizes the (previously unused) ADO testpoint that is connected to
 * GPIO0.25 to drive a N-ch/NPN transistor to control the (noisy) system fan.
 *
 * System fan connector pin 1 = GND, pin 2 = ~10VDC
 *
 * Disconnect fan from GND, connect this wire to transistor drain/collector. Also
 * connect the anode of a catch diode here, cathode to pin 2 (~10VDC).
 * Transistor source/emitter connects to ground, gate connects directly to ADO testpoint.
 * If using a bipolar transistor connect base through a 4k7 resistor to ADO testpoint.
 *
 * Example transistors that works: 2N7000 (N-ch FET) or BC547B (bipolar NPN)
 */

#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include "systemfan.h"
#include "sched.h"
#include "sensor.h"

#define SYSFAN_PWM_PERIOD (TICKS_MS( 10 ))

static uint32_t syspwmval = 0;

static int32_t SystemFanPWM_Work(void) {
	static uint8_t state = 0;
	int32_t retval;

	if (state) {
		FIO0CLR = (syspwmval != SYSFAN_PWM_PERIOD) ? (1<<25) : 0; // SysFan off
		retval = syspwmval ? (SYSFAN_PWM_PERIOD - syspwmval) : -1;
	} else {
		FIO0SET = syspwmval ? (1<<25) : 0; // SysFan on
		retval = (syspwmval != SYSFAN_PWM_PERIOD) ? syspwmval : -1;
	}
	state ^= 1;
	return retval;
}

static int32_t SystemFanSense_Work(void) {
	uint8_t sysfanspeed = 0;

	if (Sensor_IsValid(TC_COLD_JUNCTION)) {
		float systemp = Sensor_GetTemp(TC_COLD_JUNCTION);

		// Sort this out with something better at some point
		if (systemp > 50.0f) {
			sysfanspeed = 0xff;
		} else if (systemp > 45.0f) {
			sysfanspeed = 0xc0;
		} else if (systemp > 42.0f) {
			sysfanspeed = 0x80;
		} else if (systemp > 40.0f) {
			sysfanspeed = 0x50;
		}
	} else {
		// No sensor, run at full speed as a precaution
		sysfanspeed = 0xff;
	}

	uint32_t temp = SYSFAN_PWM_PERIOD >> 8;
	temp *= sysfanspeed;
	if (sysfanspeed == 0xff) {
		// Make sure we reach 100% duty cycle
		temp = SYSFAN_PWM_PERIOD;
	}
	syspwmval = temp;

	Sched_SetState(SYSFANPWM_WORK, 2, 0);

	return TICKS_SECS( 5 );
}

void SystemFan_Init(void) {
	printf("\n%s", __FUNCTION__);
	Sched_SetWorkfunc(SYSFANPWM_WORK, SystemFanPWM_Work);
	Sched_SetWorkfunc(SYSFANSENSE_WORK, SystemFanSense_Work);

	// Turn on fan briefly at boot to indicate that it actually works
	syspwmval = SYSFAN_PWM_PERIOD;
	Sched_SetState(SYSFANPWM_WORK, 2, 0); // Enable PWM task
	Sched_SetState(SYSFANSENSE_WORK, 1, TICKS_SECS( 2 ) ); // Enable Sense task
}
