/*
 * main.c - T-962 reflow controller
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
#include <string.h>
#include <stdbool.h>

#include "serial.h"
#include "log.h"
#include "lcd.h"
#include "io.h"
#include "sched.h"
#include "onewire.h"
#include "adc.h"
#include "i2c.h"
#include "rtc.h"
#include "eeprom.h"
#include "keypad.h"
#include "reflow.h"
#include "reflow_profiles.h"
#include "sensor.h"
#include "buzzer.h"
#include "nvstorage.h"
#include "version.h"
#include "vic.h"
#include "max31855.h"
#include "systemfan.h"
#include "setup.h"

extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t coolbmp[];
extern uint8_t selectbmp[];
extern uint8_t editbmp[];
extern uint8_t f3editbmp[];

// No version.c file generated for LPCXpresso builds, fall back to this
__attribute__((weak)) const char* Version_GetGitVersion(void) {
	return "no version info";
}

// render time string from seconds into internal buffer
static char *time_string(int seconds)
{
	static char buffer[9]; // like "99:59:59" or "59:59" or "H 101"

	if (seconds < 0)
		return "-:--:--";

	if (seconds < 3600)
		sprintf(buffer, "%d:%02d", seconds / 60, seconds % 60);
	else if (seconds < 3600 * 100)
		sprintf(buffer, "%d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
	else
		// only hours above H 100 (int will fit!)
		sprintf(buffer, "H %d", seconds / 3600);

	return buffer;
}

#define X_AXIS	12
#define Y_AXIS	57
extern uint8_t graphbmp[];

void plot_profile(int highlight)
{
	uint16_t x, y;
	LCD_BMPDisplay(graphbmp, 0, 0);

	// No need to plot first value as it is obscured by Y-axis
	for(uint8_t i = 1; ; i++) {
		x = (i << 1) + X_AXIS;
		y = Reflow_GetSetpointAtIdx(i) / 5;
		if (y == 0)
			break;
		y = Y_AXIS - y;

		LCD_SetPixel(x, y);
		if (highlight == i) {
			LCD_SetPixel(x - 1, y - 1);
			LCD_SetPixel(x + 1, y + 1);
			LCD_SetPixel(x - 1, y + 1);
			LCD_SetPixel(x + 1, y - 1);
		}
	}
}

static int32_t Main_Work(void);

extern int32_t Shell_Work(void);
extern void Shell_Init(void);

int main(void) {
	char buf[22];

	IO_JumpBootloader();

	PLLCFG = (1 << 5) | (4 << 0); //PLL MSEL=0x4 (+1), PSEL=0x1 (/2) so 11.0592*5 = 55.296MHz, Fcco = (2x55.296)*2 = 221MHz which is within 156 to 320MHz
	PLLCON = 0x01;
	PLLFEED = 0xaa;
	PLLFEED = 0x55; // Feed complete
	while (!(PLLSTAT & (1 << 10))); // Wait for PLL to lock
	PLLCON = 0x03;
	PLLFEED = 0xaa;
	PLLFEED = 0x55; // Feed complete
	VPBDIV = 0x01; // APB runs at the same frequency as the CPU (55.296MHz)
	MAMTIM = 0x03; // 3 cycles flash access recommended >40MHz
	MAMCR = 0x02; // Fully enable memory accelerator

	VIC_Init();
	Sched_Init();
	IO_Init();
	Set_Heater(0);
	Set_Fan(0);
	Serial_Init();
	log(LOG_INFO, "Starting version: %s", Version_GetGitVersion());

	I2C_Init();
	EEPROM_Init();
	NV_Init();

	LCD_Init();
	LCD_BMPDisplay(logobmp, 0, 0);

	IO_InitWatchdog();

	IO_Partinfo(buf, sizeof(buf), "%s rev %c");
	LCD_printf(0, 58, 0, buf);
	log(LOG_INFO, "Running on an %s", buf);

	LCD_printf(0, 0, CENTERED, Version_GetGitVersion());

	LCD_FB_Update();
	Keypad_Init();
	Buzzer_Init();
	ADC_Init();
	RTC_Init();
	OneWire_Init();
	SPI_TC_Init();
	Reflow_Init();
	SystemFan_Init();
	Shell_Init();

	Sched_SetWorkfunc(MAIN_WORK, Main_Work);
	Sched_SetState(MAIN_WORK, 1, TICKS_SECS(2)); // Enable in 2 seconds
	Sched_SetWorkfunc(SHELL_WORK, Shell_Work);
	Sched_SetState(SHELL_WORK, 1, TICKS_SECS(2)); // Enable in 2 seconds

	Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100));

	while (1) {
#ifdef ENABLE_SLEEP
		int32_t sleeptime;
		sleeptime = Sched_Do(0); // No fast-forward support
		//printf("\n%d ticks 'til next activity"),sleeptime);
#else
		Sched_Do(0); // No fast-forward support
#endif
	}
	return 0;
}

// global abort flag: if set mode returns to main mode and clears the flat
static bool abort = false;

static MainMode_t Home_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	// Make sure reflow complete beep is silenced when pressing any key
	if (key.keymask != 0) {
		Buzzer_Beep(BUZZ_NONE, 0, 0);
	}

	// this skips out on each key, if none shows menu in intervals
	// no abort handling here
	switch (key.priorized_key) {
	case KEY_F1:
		return MAIN_ABOUT;
	case KEY_F2:
		return MAIN_SETUP;
	case KEY_F3:
		return MAIN_BAKE_SETUP;
	case KEY_F4:
		return MAIN_SELECT_PROFILE;
	case KEY_S:
		return MAIN_REFLOW;
	}

	LCD_FB_Clear();

	LCD_printf(0,  0, CENTERED, "MAIN MENU");
	LCD_printf(0,  8, INVERT, "F1"); LCD_printf(14,  8, 0, "ABOUT");
	LCD_printf(0, 16, INVERT, "F2"); LCD_printf(14, 16, 0, "SETUP");
	LCD_printf(0, 24, INVERT, "F3"); LCD_printf(14, 24, 0, "BAKE MODE");
	LCD_printf(0, 32, INVERT, "F4"); LCD_printf(14, 32, 0, "SELECT PROFILE");
	LCD_printf(0, 40, INVERT,  "S"); LCD_printf(14, 40, 0, "RUN REFLOW PROFILE");
	LCD_printf(0, 48, INVERT | CENTERED, Reflow_GetProfileName(-1));
	LCD_printf(0, 58, CENTERED, "OVEN TEMPERATURE %3u`", (uint16_t) Sensor_GetTemp(TC_AVERAGE));

	return mode;
}

static MainMode_t About_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	// Leave about with any key, no abort
	if (key.keymask != 0)
		return MAIN_HOME;

	LCD_FB_Clear();
	LCD_BMPDisplay(logobmp, 0, 0);
	LCD_printf(0, 0, CENTERED, "T-962 controller");
	LCD_printf(0, 58, CENTERED, Version_GetGitVersion());
	LCD_BMPDisplay(stopbmp, 127 - 17, 0);

	return mode;
}

static MainMode_t Setup_Mode(MainMode_t mode) {
	static int selected = 0;
	static bool prolog = true;

	if (prolog) {
		// TODO:
		// Reflow_SetMode(REFLOW_STANDBYFAN);
		prolog = false;
	}

	fkey_t key = Keypad_Get(2, 60);
	// no abort handling

	switch (key.priorized_key) {
	case KEY_F1:
		selected = wrap(selected - 1, 0, Setup_getNumItems() - 1);
		break;
	case KEY_F2:
		selected = wrap(selected + 1, 0, Setup_getNumItems() - 1);
		break;
	case KEY_F3:
		Setup_decreaseValue(selected, key.acceleration / 2);
		break;
	case KEY_F4:
		Setup_increaseValue(selected, key.acceleration / 2);
		break;
	case KEY_S:
		mode = MAIN_HOME;
		// TODO:
		// Reflow_SetMode(REFLOW_STANDBY);
		prolog = true;
		return mode;
	}

	LCD_FB_Clear();
	LCD_printf(0, 0, CENTERED, "SETUP");

	for (int i = 0, y = 7; i < Setup_getNumItems() ; i++) {
		char buf[22];

		Setup_snprintFormattedValue(buf, sizeof(buf), i);
		LCD_printf(0, y, selected == i ? INVERT : 0, buf);
		y += 7;
	}

	// buttons
	LCD_printf( 0, 57, INVERT, " < ");
	LCD_printf(20, 57, INVERT, " > ");
	LCD_printf(45, 57, INVERT, " - ");
	LCD_printf(65, 57, INVERT, " + ");
	LCD_printf(91, 57, INVERT, " DONE ");

	return mode;
}

/*
 * Reflow will leave this mode only after cooling, so it can be
 * restarted immediately, just as bake mode!
 */
static MainMode_t Reflow_Mode(MainMode_t mode) {
	static bool prolog = true;

	if (prolog) {
		LCD_FB_Clear();
		log(LOG_INFO, "Starting reflow with profile: %s", Reflow_GetProfileName(-1));
		plot_profile(-1);
		LCD_BMPDisplay(stopbmp, 127 - 17, 0);
		LCD_printf(13, 0, 0, Reflow_GetProfileName(-1));
		Reflow_ActivateReflow();
		prolog = false;
	}

	const ReflowInformation_t *i = Reflow_Information();
	LCD_printf(110,  7, 0, "SET"); LCD_printf(110, 13, 0, "%03u", (uint16_t) i->setpoint);
	LCD_printf(110, 20, 0, "ACT"); LCD_printf(110, 26, 0, "%03u", (uint16_t) i->temperature);
	LCD_printf(110, 33, 0, "RUN"); LCD_printf(110, 39, 0, "%03u", i->time_done);

	// Plot actual temperature on top of desired profile
	LCD_SetPixel(X_AXIS + i->time_done / 5, Y_AXIS - (uint16_t) i->temperature / 5);

	fkey_t key = Keypad_Get(1, 1);

	if (key.priorized_key == KEY_S || abort) {
		Reflow_Abort();
		abort = false;
		prolog = true;
		return MAIN_COOLING;
	}

	if (Reflow_IsStandby()) {
		prolog = true;
		return MAIN_HOME;
	}

	return mode;
}

static MainMode_t Select_Profile_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	// no abort
	switch (key.priorized_key) {
	case KEY_F1:
		Reflow_SelectProfileIdx(Reflow_GetProfileIdx() - 1);
		break;
	case KEY_F2:
		Reflow_SelectProfileIdx(Reflow_GetProfileIdx() + 1);
		break;
	case KEY_F3:
		return MAIN_EDIT_PROFILE;
	case KEY_F4:
		// ignored
		break;
	case KEY_S:
		return MAIN_HOME;
	}

	LCD_FB_Clear();
	plot_profile(-1);
	LCD_BMPDisplay(selectbmp, 127 - 17, 0);
	 // Display edit button
	if (Reflow_IdxIsInEEPROM(-1)) {
		LCD_BMPDisplay(f3editbmp, 127 - 17, 29);
	}
	LCD_printf(13, 0, 0, Reflow_GetProfileName(-1));

	return mode;
}

// TODO: allow 0 as value here?
static int16_t update_setpoint(uint8_t idx, int16_t sp) {
	sp = coerce(sp, 0, SETPOINT_MAX);
	Reflow_SetSetpointAtIdx(idx, sp);
	return sp;
}

static MainMode_t Edit_Profile_Mode(MainMode_t mode) {
	static uint8_t profile_time_idx = 0;
	int16_t cursetpoint;

	fkey_t key = Keypad_Get(2, 60);
	// no abort

	cursetpoint = Reflow_GetSetpointAtIdx(profile_time_idx);

	// only one key is processed in one run!
	switch(key.priorized_key) {
	case KEY_F1:
		profile_time_idx--;
		break;
	case KEY_F2:
		profile_time_idx++;
		break;
	case KEY_F3:
		cursetpoint -= key.acceleration / 2;
		cursetpoint = update_setpoint(profile_time_idx, cursetpoint);
		break;
	case KEY_F4:
		cursetpoint += key.acceleration / 2;
		cursetpoint = update_setpoint(profile_time_idx, cursetpoint);
		break;
	case KEY_S:
		Reflow_SaveEEProfile();
		mode = MAIN_HOME;
		break;
	}

	profile_time_idx = coerce(profile_time_idx, 0, 47);

	LCD_FB_Clear();
	plot_profile(profile_time_idx);
	LCD_BMPDisplay(editbmp, 127 - 17, 0);
	LCD_printf(13, 0, 0, "%02u0s %03u`", profile_time_idx, cursetpoint);

	return mode;
}

/*
 * render the bake screen for BAKE_SETUP and BAKE
 *  if setpoint is 0, don't show edit keys and use Reflow_Information()
 */
static void render_bake_screen(uint16_t setpoint, uint32_t timer) {
	const ReflowInformation_t *i = Reflow_Information();

	// update display
	LCD_FB_Clear();
	LCD_printf(0, 0, CENTERED, "BAKE MODE");

	if (setpoint == 0) {
		LCD_printf(0, 10, CENTERED, "SETPOINT %d`", (uint16_t) i->setpoint);
		LCD_printf(0, 18, CENTERED, "TIMER %s", time_string(i->time_to_go - i->time_done));
	} else {
		LCD_printf(0, 10, INVERT, "F1");
		LCD_printf(0, 10, CENTERED, "- SETPOINT %d` +", setpoint);
		LCD_printf(0, 10, RIGHT_ALIGNED | INVERT, "F2");

		LCD_printf(0, 18, INVERT, "F3");
		LCD_printf(0, 18, CENTERED, "- TIMER %s +", time_string(timer));
		LCD_printf(0, 18, RIGHT_ALIGNED | INVERT, "F4");
	}

	LCD_printf(0, 26, 0, "ACT %3.1f`", Sensor_GetTemp(TC_AVERAGE));

	LCD_printf(0, 34, 0, "  L %3.1f`", Sensor_GetTemp(TC_LEFT));
	LCD_printf(LCD_CENTER, 34, 0, "  R %3.1f`", Sensor_GetTemp(TC_RIGHT));

	if (Sensor_IsValid(TC_EXTRA1)) {
		LCD_printf(0, 42, 0, " X1 %3.1f`", Sensor_GetTemp(TC_EXTRA1));
	}
	if (Sensor_IsValid(TC_EXTRA2)) {
		LCD_printf(LCD_CENTER, 42, 0, " X2 %3.1f`", Sensor_GetTemp(TC_EXTRA2));
	}

	if (Sensor_IsValid(TC_COLD_JUNCTION)) {
		LCD_printf(0, 50, 0, "COLDJUNCTION %3.1f`", Sensor_GetTemp(TC_COLD_JUNCTION));
	}

	LCD_BMPDisplay(stopbmp, 127 - 17, 0);
}

/*
 * Bake modes will leave only after cooling, so it can be
 * restarted immediately, just as reflow mode!
 */
static MainMode_t Bake_Setup_Mode(MainMode_t mode) {
	static int timer;
	static int setpoint;
	static bool prolog = true;
	const ReflowInformation_t *i = Reflow_Information();

	if (prolog) {
		// might be started by the shell, don't interfere
		setpoint = i->setpoint;
		timer = i->time_to_go;
		prolog = false;
	}

	fkey_t key = Keypad_Get(2, 60);
	int increment = key.acceleration / 2;

	if (abort) {
		// inject KEY_S
		key.priorized_key = KEY_S;
		abort = false;
	}

	switch (key.priorized_key) {
	case KEY_F1:
		setpoint -= increment;
		break;
	case KEY_F2:
		setpoint += increment;
		break;
	case KEY_F3:
		timer -= increment;
		break;
	case KEY_F4:
		timer += increment;
		break;
	case KEY_S:
		Reflow_Abort();
		prolog = true;
		return MAIN_COOLING;
	}

	setpoint = coerce(setpoint, SETPOINT_MIN, SETPOINT_MAX);
	// TODO: BAKE_TIMER_MAX is 60h!
	timer = coerce(timer, 0, 10 * 3600); // fits to display

	// start baking when timer != 0 (time to go is reset in standby)
	if (timer != 0) {
		// this can re-adjust the setpoint and the timer, but only in preheat mode
		if (Reflow_ActivateBake(setpoint, timer) != 0) {
			// did not work, we are baking!
			prolog = true;
			return MAIN_BAKE;
		}
	}

	render_bake_screen(setpoint, timer);
	return mode;
}

static MainMode_t Bake_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	if (key.priorized_key == KEY_S || abort) {
		abort = false;
		Reflow_Abort();
		return MAIN_COOLING;
	}

	if (Reflow_IsStandby()) {
		return MAIN_HOME;
	}

	render_bake_screen(0, 0);
	return mode;
}

static MainMode_t Cooling_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	// beep on any key press, this is still cooling!
	if (key.keymask != 0) {
		Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
	}

	if (Reflow_IsStandby()) {
		mode = MAIN_HOME;
	}

	LCD_FB_Clear();
	LCD_printf(0, 26, RIGHT_ALIGNED, "COOL");
	LCD_printf(0, 34, RIGHT_ALIGNED, "%3.1f`", Sensor_GetTemp(TC_AVERAGE));
	LCD_BMPDisplay(coolbmp, 0, 0);

	return mode;
}


static MainMode_t current_mode = MAIN_HOME;

// external interface (for the shell) to switch modes
int Set_Mode(MainMode_t new_mode)
{
	// this is used to escape from bake or reflow mode
	if (new_mode == MAIN_HOME &&
		(current_mode == MAIN_BAKE || current_mode == MAIN_BAKE_SETUP || current_mode == MAIN_REFLOW)) {
		abort = true;
		return true;
	}
	// only switch to new mode if in HOME, avoid strange things ...
	//  don't allow same mode! shell might want to start reflow which is in progress already
	if (current_mode == MAIN_HOME) {
		current_mode = new_mode;
		// true if switched
		return true;
	}
	return false;
}

static int32_t Main_Work(void) {
	int32_t retval = TICKS_MS(500);
	MainMode_t new_mode = MAIN_HOME;

	// main menu state machine
	switch(current_mode) {
	case MAIN_HOME:
		new_mode = Home_Mode(current_mode);
		break;
	case MAIN_ABOUT:
		new_mode = About_Mode(current_mode);
		break;
	case MAIN_SETUP:
		new_mode = Setup_Mode(current_mode);
		break;
	case MAIN_BAKE_SETUP:
		new_mode = Bake_Setup_Mode(current_mode);
		break;
	case MAIN_BAKE:
		new_mode = Bake_Mode(current_mode);
		break;
	case MAIN_SELECT_PROFILE:
		new_mode = Select_Profile_Mode(current_mode);
		break;
	case MAIN_EDIT_PROFILE:
		new_mode = Edit_Profile_Mode(current_mode);
		break;
	case MAIN_REFLOW:
		new_mode = Reflow_Mode(current_mode);
		break;
	case MAIN_COOLING:
		new_mode = Cooling_Mode(current_mode);
		break;
	}

	LCD_FB_Update();
	// whenever mode is switched return immediately
	if (new_mode != current_mode) {
		retval = 0;
		current_mode = new_mode;
	}

	return retval;
}
