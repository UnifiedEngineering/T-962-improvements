/*
 * keypad.c - Keypad interface for T-962 reflow controller
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
#include "keypad.h"
#include "io.h"

#define F1KEY_PORTBIT (1<<23)
#define F2KEY_PORTBIT (1<<15)
#define F3KEY_PORTBIT (1<<16)
#define F4KEY_PORTBIT (1<<4)
#define S_KEY_PORTBIT (1<<20)

#define KEYREPEATDELAY (6)

uint32_t Keypad_Poll(void) {
	static uint32_t laststate = 0;
	static uint16_t laststateunchangedctr = 0;
	uint32_t retval = 0;
	uint32_t inverted = ~FIO0PIN & (F1KEY_PORTBIT | F2KEY_PORTBIT | F3KEY_PORTBIT | F4KEY_PORTBIT | S_KEY_PORTBIT);
	uint32_t changed = inverted ^ laststate;

	// At this point we only care about when button is pressed, not released
	changed &= inverted;

	if( laststate != inverted ) {
		laststate = inverted;
		laststateunchangedctr = 0;
	} else {
		laststateunchangedctr++;
		if(laststateunchangedctr > KEYREPEATDELAY) {
			changed = laststate; // Feed key repeat
			retval |= ((laststateunchangedctr-KEYREPEATDELAY)<<16); // For accelerating key repeats
		}
	}

	if(changed) {
		if(changed & F1KEY_PORTBIT) retval |= KEY_F1;
		if(changed & F2KEY_PORTBIT) retval |= KEY_F2;
		if(changed & F3KEY_PORTBIT) retval |= KEY_F3;
		if(changed & F4KEY_PORTBIT) retval |= KEY_F4;
		if(changed & S_KEY_PORTBIT) retval |= KEY_S;
	}

	return retval;
}
