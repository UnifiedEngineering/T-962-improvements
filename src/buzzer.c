/*
 * buzzer.c - Simple buzzer interface for T-962 reflow controller
 *
 * Copyright (C) 2011,2014 Werner Johansson, wj@unifiedengineering.se
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
#include "buzzer.h"
#include <stdio.h>
#include "sched.h"

BuzzFreq_t requested_buzz_freq;
uint8_t requested_buzz_volume;
int32_t requested_buzz_length;

// The buzzer is hooked up to PWM5 output, but contains an oscillator of its own so volume and freq are ignored for now. :(

static int32_t Buzzer_Work(void) {
	//wjprintf_P(PSTR("\nBuzzer work %d"),requested_buzz_freq);
	if(requested_buzz_freq!=BUZZ_NONE) {
		FIO0SET = (1<<21);
		requested_buzz_freq=BUZZ_NONE;
	} else {
		requested_buzz_length=-1; // Don't schedule until next beep is requested
		FIO0CLR = (1<<21);
	}
	return requested_buzz_length;
}

void Buzzer_Init(void) {
	printf("\n%s ",__FUNCTION__);

	Sched_SetWorkfunc(BUZZER_WORK,Buzzer_Work);
}

void Buzzer_Beep(BuzzFreq_t freq,uint8_t volume,int32_t ticks) {
	//wjprintf_P(PSTR("\nBuzzer beep "));
	if( ticks > 0 ) {
		requested_buzz_freq=freq;
		requested_buzz_volume=volume;
		requested_buzz_length=ticks;
		Sched_SetState(BUZZER_WORK,2,0);
	}
}
