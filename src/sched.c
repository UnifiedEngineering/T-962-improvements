/*
 * sched.c - Simple scheduling for T-962 reflow controller
 *
 * Copyright (C) 2011,2012,2014 Werner Johansson, wj@unifiedengineering.se
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
#include "sched.h"

// Not a public struct - therefore it's defined here
typedef struct SchedItem {
    uint32_t dueTicks;
	SchedCall_t workFunc;
	uint8_t enabled;
} SchedItem_t;

static SchedItem_t tasks[SCHED_NUM_ITEMS];

void Sched_Init(void) {
	T0CTCR = 0; // Normal timer mode
	T0PR = TIMER_PRESCALER - 1; // Prescaler divisor, timer now ticks in usecs/8 (-1.25% off as clk is 55.296MHz)
	T0TCR = 0x01; // Enable timer
	T0MCR = 0x01; // Interrupt on MCR0 match
}

uint32_t Sched_GetTick(void) {
	return T0TC;
}

// The enable is atomic but the dueTicks are not, so only call this with 0 or 2 as enable parameter
// (2 will force scheduling as soon as possible without relying on dueTicks to be correct)
void Sched_SetState(Task_t tasknum, uint8_t enable, int32_t future) {
	if (enable == 1) {
		tasks[tasknum].dueTicks = future;
	}
	tasks[tasknum].enabled = enable;
}

uint8_t Sched_IsOverride(void) {
	uint8_t retval = 0; // No override by default
	for (uint8_t lp = 0; lp < SCHED_NUM_ITEMS; lp++) {
		if (tasks[lp].enabled == 2) {
			retval = 1;
			//if(SelectiveDebugIsEnabled(SD_SCHED_OVERRIDE)) wjprintf_P(PSTR("\nTask 0x%x overrides sleep!"), lp);
		}
	}
	return retval;
}

void Sched_SetWorkfunc(Task_t tasknum, SchedCall_t func) {
	tasks[tasknum].workFunc = func;
}

int32_t Sched_Do(uint32_t fastforward) {
	static uint32_t oldTick = 0;
	int32_t shortestwait = 0x7fffffff;
	uint32_t curTick = Sched_GetTick();

	// How many ticks will we should roll forward (including sleep time)
	uint32_t numRollFwd = (curTick - oldTick) + fastforward;
	oldTick = curTick;

	for (uint8_t lp = 0; lp < SCHED_NUM_ITEMS; lp++) {
		if (tasks[lp].enabled >= 1) { // Only deal with enabled tasks
			uint32_t tmp = tasks[lp].dueTicks;
			if ((tasks[lp].enabled == 2) || (tmp <= numRollFwd)) { // Time to execute this task
				int32_t nextdelta = tasks[lp].workFunc(); // Call the scheduled work
				if (nextdelta >= 0) { // Re-arm
					tmp = nextdelta;
					tasks[lp].enabled = 1;
				} else { // Putting task to sleep until awakened by Sched_SetState
					tasks[lp].enabled = 0;
				}
			} else {
				tmp -= numRollFwd;
			}
			tasks[lp].dueTicks = tmp;
			if (tmp < shortestwait) {
				shortestwait = tmp;
			}
		}
	}
	// Unless a (wake-up) interrupt calls Sched_SetState, this is how
	// long it's OK to sleep until next task is due
	return shortestwait;
}

void BusyWait( uint32_t numticks ) {
	T0IR = 0x01; // Reset interrupt
	T0MR0 = 1 + T0TC + numticks; // It's perfectly fine if this wraps
	while (!(T0IR & 0x01)); // Wait for match
}
