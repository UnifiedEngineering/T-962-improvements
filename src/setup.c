/*
 * setup.c - T-962 reflow controller
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

#include <stdint.h>
#include <stdio.h>
#include "nvstorage.h"
#include "reflow_profiles.h"
#include "setup.h"

static setupMenuStruct setupmenu[] = {
	{"Min fan speed    %4.0f", REFLOW_MIN_FAN_SPEED, 0, 254, 0, 1.0f},
	{"Cycle done beep %4.1fs", REFLOW_BEEP_DONE_LEN, 0, 254, 0, 0.1f},
	{"Left TC gain     %1.2f", TC_LEFT_GAIN, 10, 190, 0, 0.01f},
	{"Left TC offset  %+1.2f", TC_LEFT_OFFSET, 0, 200, -100, 0.25f},
	{"Right TC gain    %1.2f", TC_RIGHT_GAIN, 10, 190, 0, 0.01f},
	{"Right TC offset %+1.2f", TC_RIGHT_OFFSET, 0, 200, -100, 0.25f},
};
#define NUM_SETUP_ITEMS (sizeof(setupmenu) / sizeof(setupmenu[0]))

int Setup_getNumItems(void) {
	return NUM_SETUP_ITEMS;
}

int _getRawValue(int item) {
	return NV_GetConfig(setupmenu[item].nvval);
}

float Setup_getValue(int item) {
	int intval = _getRawValue(item);
	intval += setupmenu[item].offset;
	return ((float)intval) * setupmenu[item].multiplier;
}

void Setup_setValue(int item, int value) {
	NV_SetConfig(setupmenu[item].nvval, value);
	Reflow_ValidateNV();
}

void Setup_setRealValue(int item, float value) {
	int intval = (int)(value / setupmenu[item].multiplier);
	intval -= setupmenu[item].offset;
	Setup_setValue(item, intval);
}

void Setup_increaseValue(int item, int amount) {
	int curval = _getRawValue(item) + amount;

	int maxval = setupmenu[item].maxval;
	if (curval > maxval) curval = maxval;

	Setup_setValue(item, curval);
}

void Setup_decreaseValue(int item, int amount) {
	int curval = _getRawValue(item) - amount;

	int minval = setupmenu[item].minval;
	if (curval < minval) curval = minval;

	Setup_setValue(item, curval);
}

void Setup_printFormattedValue(int item) {
	printf(setupmenu[item].formatstr, Setup_getValue(item));
}

int Setup_snprintFormattedValue(char* buf, int n, int item) {
	return snprintf(buf, n, setupmenu[item].formatstr, Setup_getValue(item));
}
