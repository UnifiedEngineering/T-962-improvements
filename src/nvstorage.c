/*
 * nvstorage.c - Persistent settings storage for T-962 reflow controller
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "log.h"
#include "nvstorage.h"
#include "eeprom.h"
#include "sched.h"

// change this if layout changes
#define NVMAGIC 			0xFECA  // CAFE
#define MAX_CONFIG_ENTRIES	14
#define NO_OF_PROFILES		4
#define TEMPERATURE_OFFSET	20

typedef struct __attribute__ ((__packed__)) {
	uint8_t name[12];			// 'custom' is prepended, zero terminate if smaller than size
	uint8_t temperatures[48];   // temperature with offset --> 255 means 275C, but 0 stays 0!
} NV_profile_t;

// this is the layout of the eeprom starting at 0
typedef struct __attribute__ ((__packed__)) {
	uint16_t format_magic;
	uint8_t config[MAX_CONFIG_ENTRIES];
	NV_profile_t profiles[NO_OF_PROFILES];	// 4 * 60 = 240
} NV_t;

static NV_t myNV; // RAM copy of the NV data
static uint8_t nvupdatepending=0;

static void SetNVUpdatePending(void) {
	nvupdatepending = 1;
	Sched_SetState(NV_WORK, 1, 0);
}

void NV_Init(void) {
	// fetch whole content to RAM copy
	EEPROM_Read((uint8_t*) &myNV, 0, sizeof(myNV));

	if (myNV.format_magic != NVMAGIC ) {
		log(LOG_INFO, "wrong magic found, formatting EEPROM");

		// default values go here
		myNV.format_magic = NVMAGIC;
		myNV.config[REFLOW_BEEP_DONE_LEN] = 10;	// default 1s
		myNV.config[REFLOW_PROFILE] = 1;
		myNV.config[TC_LEFT_GAIN] = 100;
		myNV.config[TC_LEFT_OFFSET] = 100;
		myNV.config[TC_RIGHT_GAIN] = 100;
		myNV.config[TC_RIGHT_OFFSET] = 100;
		myNV.config[REFLOW_MIN_FAN_SPEED] = 8;

		// clear all profiles
		for (int i=0; i<NO_OF_PROFILES; i++)
			memset(&myNV.profiles[i], 0, sizeof(NV_profile_t));

		EEPROM_Write(0, (uint8_t *) &myNV, sizeof(myNV));
	}

#ifndef MINIMALISTIC
	Sched_SetWorkfunc(NV_WORK,NV_Work);
	Sched_SetState(NV_WORK, 1, 0);
#endif
}

// interface for EEProm profiles
int NV_NoOfProfiles(void) {
	return NO_OF_PROFILES;
}

// no range checks, done in layer above
uint16_t NV_GetSetpoint(uint8_t profile, uint8_t idx) {
	uint16_t t = myNV.profiles[profile].temperatures[idx];

	if (t == 0)
		return 0;
	return t + TEMPERATURE_OFFSET;
}

void NV_SetSetpoint(uint8_t profile, uint8_t idx, uint16_t value) {
	uint8_t t = 0;

	if (value > TEMPERATURE_OFFSET)
		t = (uint8_t) (value - TEMPERATURE_OFFSET);
	myNV.profiles[profile].temperatures[idx] = t;
}

#define NAME_SIZE sizeof(myNV.profiles[0].name)

// returns a local buffer filled with a zero terminated string
// generates a default name 'CUSTOM #n' if first char is '\xFF'
char *NV_GetProfileName(uint8_t profile) {
	static char buffer[NAME_SIZE + 1];

	if (myNV.profiles[profile].name[0] == '\0') {
		sprintf(buffer, "CUSTOM #%d", profile);
	} else {
		strncpy(buffer, (char *) myNV.profiles[profile].name, NAME_SIZE);
		buffer[NAME_SIZE] = '\0';
	}
	return buffer;
}

void NV_SetProfileName(uint8_t profile, const char *name) {
	strncpy((char *) myNV.profiles[profile].name, name, NAME_SIZE);
}

int NV_StoreProfile(uint8_t profile) {
	return EEPROM_Write(offsetof(NV_t, profiles[profile]),
			(uint8_t *) &myNV.profiles[profile], sizeof(NV_profile_t));
}

uint8_t NV_GetConfig(NVItem_t item) {
	return myNV.config[item];
}

void NV_SetConfig(NVItem_t item, uint8_t value) {
	if (value != myNV.config[item]) {
		myNV.config[item] = value;
		SetNVUpdatePending();
	}
}

// Periodic updater of NV, only updates the config part
// TODO: actually this should not be necessary
int32_t NV_Work(void) {
	static uint8_t count = 0;

	if (nvupdatepending) count ++;
	if (count == 4) {
		nvupdatepending = count = 0;
		log(LOG_VERBOSE, "Flushing NV configuration to EEProm");
		EEPROM_Write(offsetof(NV_t, config), myNV.config, NVITEM_NUM_ITEMS);
	}
	return nvupdatepending ? (int32_t) (TICKS_SECS(2)) : -1;
}
