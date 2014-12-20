/*
 * io.c - I/O handling for T-962 reflow controller
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
#include "t962.h"
#include "io.h"
#include "sched.h"
#include "vic.h"

void Set_Heater(uint8_t enable) {
	if( enable < 0xff ) {
		PINSEL0 |= (2<<18); // Make sure PWM6 function is enabled
	} else { // Fully on is dealt with separately to avoid output glitch
		PINSEL0 &= ~(2<<18); // Disable PWM6 function on pin
		enable = 0xfe; // Not fully on according to PWM hardware but we force GPIO low anyway
	}
	PWMMR6 = 0xff - enable;
	PWMLER |= (1<<6);
}

void Set_Fan(uint8_t enable) {
	if( enable < 0xff ) {
		PINSEL0 |= (2<<16); // Make sure PWM4 function is enabled
	} else { // Fully on is dealt with separately to avoid output glitch
		PINSEL0 &= ~(2<<16); // Disable PWM4 function on pin
		enable = 0xfe; // Not fully on according to PWM hardware but we force GPIO low anyway
	}
	PWMMR4 = 0xff - enable;
	PWMLER |= (1<<4);
}

static int32_t Sleep_Work(void) {
	//FIO0PIN ^= (1<<31); // Toggle debug LED
	// For some reason P0.31 status cannot be read out, so the following is used instead:
	static uint8_t flip = 0;
	if( flip ) FIO0SET = (1<<31); else FIO0CLR = (1<<31);
	flip ^= 1;

	// If interrupts are used they must be disabled around the following two instructions!
	uint32_t save = VIC_DisableIRQ();
	WDFEED = 0xaa; // Feed watchdog
	WDFEED = 0x55;
	VIC_RestoreIRQ( save );
	return TICKS_SECS(1);
}

void IO_Init(void) {
	SCS = 0b11; // Enable fast GPIO on both port 0 and 1

	PINSEL0 = 0b10100000000001010101; // PWM6 + PWM4 + I2C0 + UART0
	PINSEL1 = 0b00000101000000000000000000000000; // ADC0 1+2

	FIO0MASK = 0b01001101000000100000010001100000; // Mask out all unknown/unused pins
	FIO1MASK = 0b11111111000000001111111111111111; // Only LCD D0-D7

	FIO0DIR = 0b10000000011011000011101100000001; // Default output pins
	FIO1DIR = 0b00000000000000000000000000000000;

	FIO0PIN = 0x00; // Turn LED on and make PWM outputs active when in GPIO mode (to help 100% duty cycle issue)

	PWMPR = PCLKFREQ/(256*5); // Let's have the PWM perform 5 cycles per second with 8 bits of precision (way overkill)
	PWMMCR = (1<<1); // Reset TC on mr0 overflow (period time)
	PWMMR0 = 0xff; // Period time
	PWMLER = (1<<0); // Enable latch on mr0 (Do I really need to do this?)
	PWMPCR = (1<<12) | (1<<14); // Enable PWM4 and 6
	PWMTCR = (1<<3) | (1<<0); // Enable timer in PWM mode

	Sched_SetWorkfunc(SLEEP_WORK, Sleep_Work);
	Sched_SetState(SLEEP_WORK, 2, 0);
}
