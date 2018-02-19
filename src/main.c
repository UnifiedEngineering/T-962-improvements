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
#include "serial.h"
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
extern uint8_t selectbmp[];
extern uint8_t editbmp[];
extern uint8_t f3editbmp[];

// No version.c file generated for LPCXpresso builds, fall back to this
__attribute__((weak)) const char* Version_GetGitVersion(void) {
	return "no version info";
}

static char* format_about = \
"\nT-962-controller open source firmware (%s)" \
"\n" \
"\nSee https://github.com/UnifiedEngineering/T-962-improvement for more details." \
"\n" \
"\nInitializing improved reflow oven...";

static char* help_text = \
"\nT-962-controller serial interface.\n\n" \
" about                   Show about + debug information\n" \
" bake <setpoint>         Enter Bake mode with setpoint\n" \
" bake <setpoint> <time>  Enter Bake mode with setpoint for <time> seconds\n" \
" help                    Display help text\n" \
" list profiles           List available reflow profiles\n" \
" list settings           List machine settings\n" \
" quiet                   No logging in standby mode\n" \
" reflow                  Start reflow with selected profile\n" \
" setting <id> <value>    Set setting id to value\n" \
" select profile <id>     Select reflow profile by id\n" \
" stop                    Exit reflow or bake mode\n" \
" values                  Dump currently measured values\n" \
"\n";

static int32_t Main_Work(void);

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
	printf(format_about, Version_GetGitVersion());

	I2C_Init();
	EEPROM_Init();
	NV_Init();

	LCD_Init();
	LCD_BMPDisplay(logobmp, 0, 0);

	IO_InitWatchdog();
	IO_PrintResetReason();

	IO_Partinfo(buf, sizeof(buf), "%s rev %c");
	LCD_printf(0, 58, 0, buf);
	printf("\nRunning on an %s", buf);

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

	Sched_SetWorkfunc(MAIN_WORK, Main_Work);
	Sched_SetState(MAIN_WORK, 1, TICKS_SECS(2)); // Enable in 2 seconds

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

typedef enum eMainMode {
	MAIN_HOME = 0,
	MAIN_ABOUT,
	MAIN_SETUP,
	MAIN_BAKE,
	MAIN_SELECT_PROFILE,
	MAIN_EDIT_PROFILE,
	MAIN_REFLOW
} MainMode_t;

#if 0
/*
 * this is simply stripped out of Main_Work() to simplify
 *   replacement.
 */
static void minimal_shell(void) {
	char serial_cmd[255] = "";
	char* cmd_select_profile = "select profile %d";
	char* cmd_bake = "bake %d %d";
	char* cmd_dump_profile = "dump profile %d";
	char* cmd_setting = "setting %d %f";

	if (uart_isrxready()) {
		int len = uart_readline(serial_cmd, 255);

		if (len > 0) {
			int param, param1;
			float paramF;

			if (strcmp(serial_cmd, "about") == 0) {
				printf(format_about, Version_GetGitVersion());
				len = IO_Partinfo(buf, sizeof(buf), "\nPart number: %s rev %c\n");
				printf(buf);
				EEPROM_Dump();

				printf("\nSensor values:\n");
				Sensor_ListAll();

			} else if (strcmp(serial_cmd, "help") == 0 || strcmp(serial_cmd, "?") == 0) {
				printf(help_text);

			} else if (strcmp(serial_cmd, "list profiles") == 0) {
				printf("\nReflow profiles available:\n");

				Reflow_ListProfiles();
				printf("\n");

			} else if (strcmp(serial_cmd, "reflow") == 0) {
				printf("\nStarting reflow with profile: %s\n", Reflow_GetProfileName());
				mode = MAIN_HOME;
				// this is a bit dirty, but with the least code duplication.
				keyspressed = KEY_S;

			} else if (strcmp(serial_cmd, "list settings") == 0) {
				printf("\nCurrent settings:\n\n");
				for (int i = 0; i < Setup_getNumItems() ; i++) {
					printf("%d: ", i);
					Setup_printFormattedValue(i);
					printf("\n");
				}

			} else if (strcmp(serial_cmd, "stop") == 0) {
				printf("\nStopping bake/reflow");
				mode = MAIN_HOME;
				Reflow_SetMode(REFLOW_STANDBY);
				retval = 0;

			} else if (strcmp(serial_cmd, "quiet") == 0) {
				Reflow_ToggleStandbyLogging();
				printf("\nToggled standby logging\n");

			} else if (strcmp(serial_cmd, "values") == 0) {
				printf("\nActual measured values:\n");
				Sensor_ListAll();
				printf("\n");

			} else if (sscanf(serial_cmd, cmd_select_profile, &param) > 0) {
				// select profile
				Reflow_SelectProfileIdx(param);
				printf("\nSelected profile %d: %s\n", param, Reflow_GetProfileName());

			} else if (sscanf(serial_cmd, cmd_bake, &param, &param1) > 0) {
				if (param < SETPOINT_MIN) {
					printf("\nSetpoint must be >= %ddegC\n", SETPOINT_MIN);
					param = SETPOINT_MIN;
				}
				if (param > SETPOINT_MAX) {
					printf("\nSetpont must be <= %ddegC\n", SETPOINT_MAX);
					param = SETPOINT_MAX;
				}
				if (param1 < 1) {
					printf("\nTimer must be greater than 0\n");
					param1 = 1;
				}

				if (param1 < BAKE_TIMER_MAX) {
					printf("\nStarting bake with setpoint %ddegC for %ds after reaching setpoint\n", param, param1);
					timer = param1;
					Reflow_SetBakeTimer(timer);
				} else {
					printf("\nStarting bake with setpoint %ddegC\n", param);
				}

				setpoint = param;
				Reflow_SetSetpoint(setpoint);
				mode = MAIN_BAKE;
				Reflow_SetMode(REFLOW_BAKE);

			} else if (sscanf(serial_cmd, cmd_dump_profile, &param) > 0) {
				printf("\nDumping profile %d: %s\n ", param, Reflow_GetProfileName());
				Reflow_DumpProfile(param);

			} else if (sscanf(serial_cmd, cmd_setting, &param, &paramF) > 0) {
				Setup_setRealValue(param, paramF);
				printf("\nAdjusted setting: ");
				Setup_printFormattedValue(param);

			} else {
				printf("\nUnknown command '%s', ? for help\n", serial_cmd);
			}
		}
	}
}
#endif

static MainMode_t Home_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);
	LCD_FB_Clear();

	LCD_printf(0,  0, CENTERED, "MAIN MENU");
	LCD_printf(0,  8, INVERT, "F1"); LCD_printf(14,  8, 0, "ABOUT");
	LCD_printf(0, 16, INVERT, "F2"); LCD_printf(14, 16, 0, "SETUP");
	LCD_printf(0, 24, INVERT, "F3"); LCD_printf(14, 24, 0, "BAKE/MANUAL MODE");
	LCD_printf(0, 32, INVERT, "F4"); LCD_printf(14, 32, 0, "SELECT PROFILE");
	LCD_printf(0, 40, INVERT,  "S"); LCD_printf(14, 40, 0, "RUN REFLOW PROFILE");
	LCD_printf(0, 48, INVERT | CENTERED, Reflow_GetProfileName());
	LCD_printf(0, 58, CENTERED, "OVEN TEMPERATURE %d`", Reflow_GetActualTemp());

	// Make sure reflow complete beep is silenced when pressing any key
	if (key.keymask != 0) {
		Buzzer_Beep(BUZZ_NONE, 0, 0);
	}

	switch (key.priorized_key) {
	case KEY_F1:
		mode = MAIN_ABOUT;
		break;
	case KEY_F2:
		mode = MAIN_SETUP;
		Reflow_SetMode(REFLOW_STANDBYFAN);
		break;
	case KEY_F3:
		mode = MAIN_BAKE;
		Reflow_Init();
		break;
	case KEY_F4:
		mode = MAIN_SELECT_PROFILE;
		break;
	case KEY_S:
		mode = MAIN_REFLOW;
		LCD_FB_Clear();
		printf("\nStarting reflow with profile: %s", Reflow_GetProfileName());
		Reflow_Init();
		Reflow_PlotProfile(-1);
		LCD_BMPDisplay(stopbmp, 127 - 17, 0);
		LCD_printf(13, 0, 0, Reflow_GetProfileName());
		Reflow_SetMode(REFLOW_REFLOW);
		break;
	}

	return mode;
}

static MainMode_t Setup_Mode(MainMode_t mode) {
	static uint8_t selected = 0;
	int y = 0;

	fkey_t key = Keypad_Get(2, 60);

	if (key.priorized_key == KEY_F1) {
		if (selected > 0) { // Prev row
			selected--;
		} else { // wrap
			selected = Setup_getNumItems() - 1;
		}
	}
	if (key.priorized_key == KEY_F2) {
		if (selected < (Setup_getNumItems() - 1)) { // Next row
			selected++;
		} else { // wrap
			selected = 0;
		}
	}

	if (key.priorized_key == KEY_F3) {
		Setup_decreaseValue(selected, key.acceleration / 2);
	}
	if (key.priorized_key == KEY_F4) {
		Setup_increaseValue(selected, key.acceleration / 2);
	}

	LCD_FB_Clear();
	LCD_printf(0, y, CENTERED, "Setup/calibration");
	y += 7;

	for (int i = 0; i < Setup_getNumItems() ; i++) {
		char buf[22];

		Setup_snprintFormattedValue(buf, sizeof(buf), i);
		LCD_printf(0, y, selected == i ? INVERT : 0, buf);
		y += 7;
	}

	// buttons
	y = 64 - 7;
	LCD_printf( 0, y, INVERT, " < ");
	LCD_printf(20, y, INVERT, " > ");
	LCD_printf(45, y, INVERT, " - ");
	LCD_printf(65, y, INVERT, " + ");
	LCD_printf(91, y, INVERT, " DONE ");

	// Leave setup
	if (key.priorized_key == KEY_S) {
		mode = MAIN_HOME;
		Reflow_SetMode(REFLOW_STANDBY);
	}

	return mode;
}

static MainMode_t About_Mode(MainMode_t mode) {
	fkey_t key = Keypad_Get(1, 1);

	LCD_FB_Clear();
	LCD_BMPDisplay(logobmp, 0, 0);

	LCD_printf(0, 0, CENTERED, "T-962 controller");
	LCD_printf(0, 58, CENTERED, Version_GetGitVersion());

	LCD_BMPDisplay(stopbmp, 127 - 17, 0);

	// Leave about with any key.
	if (key.keymask != 0) {
		mode = MAIN_HOME;
	}

	return mode;
}

static MainMode_t Reflow_Mode(MainMode_t mode) {
	uint32_t ticks = RTC_Read();
	fkey_t key = Keypad_Get(1, 1);

	LCD_printf(110,  7, 0, "SET"); LCD_printf(110, 13, 0, "%03u", Reflow_GetSetpoint());
	LCD_printf(110, 20, 0, "ACT"); LCD_printf(110, 26, 0, "%03u", Reflow_GetActualTemp());
	LCD_printf(110, 33, 0, "RUN"); LCD_printf(110, 39, 0, "%03u", (unsigned int) ticks);

	// Abort reflow
	if (Reflow_IsDone() || key.priorized_key == KEY_S) {
		printf("\nReflow %s\n", (Reflow_IsDone() ? "done" : "interrupted by keypress"));
		if (Reflow_IsDone()) {
			Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
		}
		mode = MAIN_HOME;
		Reflow_SetMode(REFLOW_STANDBY);
	}

	return mode;
}

// profile editing, this is shared between Select_Profile_Mode() and Edit_Profile_Mode()
static uint8_t current_edit_profile;

static MainMode_t Select_Profile_Mode(MainMode_t mode) {
	int curprofile = Reflow_GetProfileIdx();
	fkey_t key = Keypad_Get(1, 1);

	LCD_FB_Clear();

	// Prev profile
	if (key.priorized_key == KEY_F1) {
		curprofile--;
	}
	// Next profile
	if (key.priorized_key == KEY_F2) {
		curprofile++;
	}

	Reflow_SelectProfileIdx(curprofile);

	Reflow_PlotProfile(-1);
	LCD_BMPDisplay(selectbmp, 127 - 17, 0);
	int eeidx = Reflow_GetEEProfileIdx();
	if (eeidx) { // Display edit button
		LCD_BMPDisplay(f3editbmp, 127 - 17, 29);
	}
	LCD_printf(13, 0, 0, Reflow_GetProfileName());

	if (eeidx && key.priorized_key == KEY_F3) { // Edit ee profile
		mode = MAIN_EDIT_PROFILE;
		current_edit_profile = eeidx;
	}

	// Select current profile
	if (key.priorized_key == KEY_S) {
		mode = MAIN_HOME;
	}

	return mode;
}

static MainMode_t Bake_Mode(MainMode_t mode) {
	static uint16_t setpoint = 0;
	static int timer = 0;

	fkey_t key = Keypad_Get(2, 60);

	if (setpoint == 0) {
		Reflow_LoadSetpoint();
		setpoint = Reflow_GetSetpoint();
	}

	LCD_FB_Clear();
	LCD_printf(0, 0, CENTERED, "MANUAL/BAKE MODE");

	// Setpoint-
	if (key.priorized_key == KEY_F1) {
		setpoint -= key.acceleration / 2;
		if (setpoint < SETPOINT_MIN) setpoint = SETPOINT_MIN;
	}

	// Setpoint+
	if (key.priorized_key == KEY_F2) {
		setpoint += key.acceleration / 2;
		if (setpoint > SETPOINT_MAX) setpoint = SETPOINT_MAX;
	}

	// timer --
	if (key.priorized_key == KEY_F3) {
		if (timer - key.acceleration / 2 < 0) {
			// infinite bake
			timer = -1;
		} else {
			timer -= key.acceleration / 2;
		}
	}
	// timer ++
	if (key.priorized_key == KEY_F4) {
		timer += key.acceleration / 2;
	}

	int y = 10;
	// display F1 button only if setpoint can be decreased
	char f1function = ' ';
	if (setpoint > SETPOINT_MIN) {
		LCD_printf(0, y, INVERT, "F1");
		f1function = '-';
	}
	// display F2 button only if setpoint can be increased
	char f2function = ' ';
	if (setpoint < SETPOINT_MAX) {
		LCD_printf(0, y, RIGHT_ALIGNED | INVERT, "F2");
		f2function = '+';
	}
	LCD_printf(0, y, CENTERED, "%c SETPOINT %d` %c", f1function, (int)setpoint, f2function);


	y = 18;
	if (timer == 0) {
		LCD_printf(0, y, CENTERED, "inf TIMER stop +");
	} else if (timer < 0) {
		LCD_printf(0, y, CENTERED, "no timer    stop");
	} else {
		LCD_printf(0, y, CENTERED, "- TIMER %3d:%02d +", timer / 60, timer % 60);
	}

	if (timer >= 0) {
		LCD_printf(0, y, INVERT, "F3");
	}
	LCD_printf(0, y, RIGHT_ALIGNED | INVERT, "F4");

	y = 26;
	if (timer > 0) {
		int time_left = Reflow_GetTimeLeft();
		if (Reflow_IsPreheating()) {
			LCD_printf(0, y, RIGHT_ALIGNED, "PREHEAT");
		} else if (Reflow_IsDone() || time_left < 0) {
			LCD_printf(0, y, RIGHT_ALIGNED, "DONE");
		} else {
			LCD_printf(0, y, RIGHT_ALIGNED, "%d:%02d", time_left / 60, time_left % 60);
		}
	}

	LCD_printf(0, y, 0, "ACT %3.1f`", Sensor_GetTemp(TC_AVERAGE));

	y = 34;
	LCD_printf(0, y, 0, "  L %3.1f`", Sensor_GetTemp(TC_LEFT));
	LCD_printf(LCD_CENTER, y, 0, "  R %3.1f`", Sensor_GetTemp(TC_RIGHT));

	if (Sensor_IsValid(TC_EXTRA1) || Sensor_IsValid(TC_EXTRA2)) {
		y = 42;
		if (Sensor_IsValid(TC_EXTRA1)) {
			LCD_printf(0, y, 0, " X1 %3.1f`", Sensor_GetTemp(TC_EXTRA1));
		}
		if (Sensor_IsValid(TC_EXTRA2)) {
			LCD_printf(LCD_CENTER, y, 0, " X2 %3.1f`", Sensor_GetTemp(TC_EXTRA2));
		}
	}

	y = 50;
	LCD_printf(0, y, 0, "COLDJUNCTION");

	y += 8;
	if (Sensor_IsValid(TC_COLD_JUNCTION)) {
		LCD_printf(0, y, CENTERED, "%3.1f`", Sensor_GetTemp(TC_COLD_JUNCTION));
	} else {
		LCD_printf(0, y, CENTERED, "NOT PRESENT");
	}

	LCD_BMPDisplay(stopbmp, 127 - 17, 0);

	Reflow_SetSetpoint(setpoint);

	if (timer > 0 && Reflow_IsDone()) {
		Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
		Reflow_SetBakeTimer(0);
		Reflow_SetMode(REFLOW_STANDBY);
	}

	if (key.priorized_key == KEY_F3 || key.priorized_key == KEY_F4) {
		if (timer == 0) {
			Reflow_SetMode(REFLOW_STANDBY);
		} else {
			if (timer == -1) {
				Reflow_SetBakeTimer(0);
			} else if (timer > 0) {
				Reflow_SetBakeTimer(timer);
				printf("\nSetting bake timer to %d\n", timer);
			}
			Reflow_SetMode(REFLOW_BAKE);
		}
	}

	// Abort bake
	if (key.priorized_key == KEY_S) {
		printf("\nEnd bake mode by keypress\n");

		mode = MAIN_HOME;
		Reflow_SetBakeTimer(0);
		Reflow_SetMode(REFLOW_STANDBY);
	}

	return mode;
}

static MainMode_t Edit_Profile_Mode(MainMode_t mode) {
	static uint8_t profile_time_idx = 0;

	int16_t cursetpoint;

	fkey_t key = Keypad_Get(2, 60);

	LCD_FB_Clear();

	Reflow_SelectEEProfileIdx(current_edit_profile);
	if (key.priorized_key == KEY_F1 && profile_time_idx > 0) { // Prev time
		profile_time_idx--;
	}
	if (key.priorized_key == KEY_F2 && profile_time_idx < 47) { // Next time
		profile_time_idx++;
	}
	cursetpoint = Reflow_GetSetpointAtIdx(profile_time_idx);

	if (key.priorized_key == KEY_F3) { // Decrease setpoint
		cursetpoint -= key.acceleration / 2;
	}
	if (key.priorized_key == KEY_F4) { // Increase setpoint
		cursetpoint += key.acceleration / 2;
	}
	if (cursetpoint < 0) cursetpoint = 0;
	if (cursetpoint > SETPOINT_MAX) cursetpoint = SETPOINT_MAX;
	Reflow_SetSetpointAtIdx(profile_time_idx, cursetpoint);

	Reflow_PlotProfile(profile_time_idx);
	LCD_BMPDisplay(editbmp, 127 - 17, 0);

	LCD_printf(13, 0, 0, "%02u0s %03u`", profile_time_idx, cursetpoint);

	// Done editing
	if (key.priorized_key == KEY_S) {
		Reflow_SaveEEProfile();
		mode = MAIN_HOME;
	}

	return mode;
}

static int32_t Main_Work(void) {
	static MainMode_t mode = MAIN_HOME;
	int32_t retval = TICKS_MS(500);
	MainMode_t new_mode = MAIN_HOME;

	// main menu state machine
	switch(mode) {
	case MAIN_HOME:
		new_mode = Home_Mode(mode);
		break;
	case MAIN_ABOUT:
		new_mode = About_Mode(mode);
		break;
	case MAIN_SETUP:
		new_mode = Setup_Mode(mode);
		break;
	case MAIN_BAKE:
		new_mode = Bake_Mode(mode);
		break;
	case MAIN_SELECT_PROFILE:
		new_mode = Select_Profile_Mode(mode);
		break;
	case MAIN_EDIT_PROFILE:
		new_mode = Edit_Profile_Mode(mode);
		break;
	case MAIN_REFLOW:
		new_mode = Reflow_Mode(mode);
		break;
	}

	LCD_FB_Update();
	// whenever mode is switched return immediately
	if (new_mode != mode) {
		retval = 0;
		mode = new_mode;
	}

	return retval;
}
