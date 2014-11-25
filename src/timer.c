/*
 * timer.c - Timing for T-962 reflow controller
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

#include "lpc214x.h"
#include <stdint.h>
#include "timer.h"

void Timer_Init( void ) {
	T0CTCR = 0; // Normal timer mode
//	T0PR = 55; // Prescaler divisor, timer now ticks in usecs (0.5% off as clk is 55.296MHz)
	T0PR = 7-1; // Prescaler divisor, timer now ticks in usecs/8 (-1.25% off as clk is 55.296MHz)
	T0TCR = 0x01; // Enable timer
	T0MCR = 0x01; // Interrupt on MCR0 match
}

void BusyWait8( uint32_t numusecstimes8 ) {
	T0IR = 0x01; // Reset interrupt
	T0MR0 = 1 + T0TC + numusecstimes8; // It's perfectly fine if this wraps
	while(!(T0IR & 0x01)); // Wait for match
}

uint32_t Timer_Get(void) {
	return T0TC>>3;
}
