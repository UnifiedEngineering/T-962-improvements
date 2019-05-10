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
#include "t962.h"
#include "log.h"
#include "keypad.h"
#include "io.h"
#include "sched.h"

#define F1KEY_PORTBIT (1 << 23)
#define F2KEY_PORTBIT (1 << 15)
#define F3KEY_PORTBIT (1 << 16)
#define F4KEY_PORTBIT (1 << 4)
#define S_KEY_PORTBIT (1 << 20)

#define KEYREPEATDELAY (6)

static uint32_t latchedkeypadstate = 0;

static uint32_t Keypad_GetRaw(void) {
	return ~FIO0PIN & (F1KEY_PORTBIT | F2KEY_PORTBIT | F3KEY_PORTBIT | F4KEY_PORTBIT | S_KEY_PORTBIT);
}

static int32_t Keypad_Work(void) {
	static uint32_t laststate = 0;
	static uint16_t laststateunchangedctr = 0;
	uint32_t keypadstate = 0;
	uint32_t inverted = Keypad_GetRaw();
	uint32_t changed = inverted ^ laststate;

	// At this point we only care about when button is pressed, not released
	changed &= inverted;

	if (laststate != inverted) {
		laststate = inverted;
		laststateunchangedctr = 0;
	} else {
		laststateunchangedctr++;
		if (laststateunchangedctr > KEYREPEATDELAY) {
			changed = laststate; // Feed key repeat
			// For accelerating key repeats
			keypadstate |= ((laststateunchangedctr - KEYREPEATDELAY) << 16);
		}
	}

	if (changed) {
		if (changed & F1KEY_PORTBIT) keypadstate |= KEY_F1;
		if (changed & F2KEY_PORTBIT) keypadstate |= KEY_F2;
		if (changed & F3KEY_PORTBIT) keypadstate |= KEY_F3;
		if (changed & F4KEY_PORTBIT) keypadstate |= KEY_F4;
		if (changed & S_KEY_PORTBIT) keypadstate |= KEY_S;
	}

	latchedkeypadstate &= 0xffff;
	latchedkeypadstate |= keypadstate; // Make sure software actually sees the transitions

	if (keypadstate & 0xff) {
		//printf("[KEYPAD %02x]",keypadstate & 0xff);
		Sched_SetState(MAIN_WORK, 2, 0); // Wake up main task to update UI
	}

	return TICKS_MS(100);
}

/*
 * get pressed key info: mask, acceleration and priorized key
 */
fkey_t Keypad_Get(uint16_t lowlimit, uint16_t highlimit) {
	fkey_t retval;

	retval.acceleration = latchedkeypadstate >> 16;
	if (retval.acceleration > highlimit)
		retval.acceleration = highlimit;
	if (retval.acceleration < lowlimit)
		retval.acceleration = lowlimit;

	retval.keymask = latchedkeypadstate & 0xffff;

	// ternery ?: has pretty unpredictable precedence, so be more eplicit
	retval.priorized_key = 0;
	if (retval.keymask & KEY_S)
		retval.priorized_key = KEY_S;
	else if (retval.keymask & KEY_F1)
		retval.priorized_key = KEY_F1;
	else if (retval.keymask & KEY_F2)
		retval.priorized_key = KEY_F2;
	else if (retval.keymask & KEY_F3)
		retval.priorized_key = KEY_F3;
	else if (retval.keymask & KEY_F4)
		retval.priorized_key = KEY_F4;

	latchedkeypadstate = 0;
	return retval;
}

void Keypad_Init(void) {
	Sched_SetWorkfunc(KEYPAD_WORK, Keypad_Work);
	log(LOG_DEBUG, "Waiting for keys to be released ...");
	// Note that if this takes longer than ~1 second the watchdog will bite
	while (Keypad_GetRaw()) ;
	log(LOG_DEBUG, "Done waiting for keys");

	// Potential noise gets suppressed as well
	Sched_SetState(KEYPAD_WORK, 1, TICKS_MS(250)); // Wait 250ms before starting to scan the keypad
}

