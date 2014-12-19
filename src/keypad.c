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
#include "sched.h"

#define KEYREPEATDELAY (6)

uint32_t latchedkeypadstate = 0;

uint32_t Keypad_GetRaw(void) {
	return ~FIO0PIN & (RAWONLY_F1KEY_PORTBIT | RAWONLY_F2KEY_PORTBIT | RAWONLY_F3KEY_PORTBIT | RAWONLY_F4KEY_PORTBIT | RAWONLY_S_KEY_PORTBIT);
}

static int32_t Keypad_Work(void) {
	static uint32_t laststate = 0;
	static uint16_t laststateunchangedctr = 0;
	uint32_t keypadstate = 0;
	uint32_t inverted = Keypad_GetRaw();
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
			keypadstate |= ((laststateunchangedctr-KEYREPEATDELAY)<<16); // For accelerating key repeats
		}
	}

	if(changed) {
		if(changed & RAWONLY_F1KEY_PORTBIT) keypadstate |= KEY_F1;
		if(changed & RAWONLY_F2KEY_PORTBIT) keypadstate |= KEY_F2;
		if(changed & RAWONLY_F3KEY_PORTBIT) keypadstate |= KEY_F3;
		if(changed & RAWONLY_F4KEY_PORTBIT) keypadstate |= KEY_F4;
		if(changed & RAWONLY_S_KEY_PORTBIT) keypadstate |= KEY_S;
	}

	latchedkeypadstate &= 0xffff;
	latchedkeypadstate |= keypadstate; // Make sure software actually sees the transitions

	if(keypadstate & 0xff) {
		//printf("[KEYPAD %02x]",keypadstate & 0xff);
		Sched_SetState(MAIN_WORK, 2, 0); // Wake up main task to update UI
	}

	return TICKS_MS(100);
}

uint32_t Keypad_Get(void) {
	uint32_t retval = latchedkeypadstate;
	latchedkeypadstate = 0;
	return retval;
}

void Keypad_Init( void ) {
	Sched_SetWorkfunc( KEYPAD_WORK, Keypad_Work );
	printf("\nWaiting for keys to be released... ");
	// Note that if this takes longer than ~1 second the watchdog will bite
	while( Keypad_GetRaw() );
	printf("Done!");
	// Potential noise gets suppressed as well
	Sched_SetState( KEYPAD_WORK, 1, TICKS_MS( 250 ) ); // Wait 250ms before starting to scan the keypad
}

