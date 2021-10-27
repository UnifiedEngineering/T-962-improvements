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
#include "display.h"
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


#define T962C

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
	int len;

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

//	Display_Init();
	Display_Init();
//	Display_FB_Clear();
	Display_BMPDisplay(logobmp, FB_WIDTH/2-64, FB_HEIGHT/2-32);

//	IO_InitWatchdog();
//	IO_PrintResetReason();

	len = IO_Partinfo(buf, sizeof(buf), "%s rev %c");
	Display_disp_str((uint8_t*)buf, len, 0, FB_HEIGHT - 16, DEFAULTFONT);
	printf("\nRunning on an %s", buf);

	len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
	Display_disp_str((uint8_t*)buf, len, FB_WIDTH - (len * 12), 0, DEFAULTFONT);

//	Display_FB_Update();
	Display_FB_Update();
	Keypad_Init();
	Buzzer_Init();
	ADC_Init();
	RTC_Init();
	RTC_Zero();

/*
	FIO1SET	=	(1<<13);
	FIO1DIR |= (1<<27)|(1<<24)|(1<<16);
	FIO1PIN = (0<<16)|(0<<24)|(0<<27);
	while (1) {
		FIO1DIR |= (1<<27)|(1<<24)|(1<<16);
		FIO1PIN = (0<<16)|(0<<24)|(0<<27);
		BusyWait(TICKS_US(10));
		FIO1PIN = (1<<16)|(1<<24)|(1<<27);
		BusyWait(TICKS_US(10));
	}
*/

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

static int32_t Main_Work(void) {
	static MainMode_t mode = MAIN_HOME;
	static uint16_t setpoint = 0;
	if (setpoint == 0) {
		Reflow_LoadSetpoint();
		setpoint = Reflow_GetSetpoint();
	}
	static int timer = 0;

	// profile editing
	static uint8_t profile_time_idx = 0;
	static uint8_t current_edit_profile;

	int32_t retval = TICKS_MS(500);

	char buf[160];
	int len;

	uint32_t keyspressed = Keypad_Get();

	char serial_cmd[255] = "";
	for (int i=0;i<255;i++) serial_cmd[i]=0;
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
				printf("\nCannot understand command, ? for help\n");
			}
		}
	}

	// main menu state machine
	if (mode == MAIN_SETUP) {
		static uint8_t selected = 0;
		int y = 0;

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

#ifdef T962C
		if (keyspressed & KEY_F3) {
			if (selected > 0) { // Prev row
				selected--;
			} else { // wrap
				selected = Setup_getNumItems() - 1;
			}
		}
		if (keyspressed & KEY_F4) {
			if (selected < (Setup_getNumItems() - 1)) { // Next row
				selected++;
			} else { // wrap
				selected = 0;
			}
		}

		if (keyspressed & KEY_F1) {
			Setup_decreaseValue(selected, keyrepeataccel);
		}
		if (keyspressed & KEY_F2) {
			Setup_increaseValue(selected, keyrepeataccel);
		}
#else
		if (keyspressed & KEY_F1) {
			if (selected > 0) { // Prev row
				selected--;
			} else { // wrap
				selected = Setup_getNumItems() - 1;
			}
		}
		if (keyspressed & KEY_F2) {
			if (selected < (Setup_getNumItems() - 1)) { // Next row
				selected++;
			} else { // wrap
				selected = 0;
			}
		}

		if (keyspressed & KEY_F3) {
			Setup_decreaseValue(selected, keyrepeataccel);
		}
		if (keyspressed & KEY_F4) {
			Setup_increaseValue(selected, keyrepeataccel);
		}
#endif

		Display_FB_Clear();
		len = snprintf(buf, sizeof(buf), "Setup/calibration");
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), y, DEFAULTFONT);
		y += 20;

		for (int i = 0; i < Setup_getNumItems() ; i++) {
			len = Setup_snprintFormattedValue(buf, sizeof(buf), i, DEFAULTFONT);
			printf("formatted string with filler: %d\n",len);
			Display_disp_str((uint8_t*)buf, len, 0, y, DEFAULTFONT | ((selected == i) ? INVERT : 0x00));
			y += 20;
		}

		// buttons
		y = FB_HEIGHT - 16;
#ifdef T962C
		Display_disp_str((uint8_t*)" - ", 3, 0, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" + ", 3, 20, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" < ", 3, 45, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" > ", 3, 65, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" DONE ", 6, 91, y, DEFAULTFONT | INVERT);
#else
		Display_disp_str((uint8_t*)" < ", 3, 0, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" > ", 3, 20, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" - ", 3, 45, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" + ", 3, 65, y, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)" DONE ", 6, 91, y, DEFAULTFONT | INVERT);

#endif
		// Leave setup
		if (keyspressed & KEY_S) {
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}
	} else if (mode == MAIN_ABOUT) {
//		Display_FB_Clear();
		Display_FB_Clear();
		Display_BMPDisplay(logobmp, FB_WIDTH/2-64, FB_HEIGHT/2-32);

		len = snprintf(buf, sizeof(buf), "T-962 controller");
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), 0, DEFAULTFONT);
		printf("project name disaplayed\n");

		len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), FB_HEIGHT-16, DEFAULTFONT);
		printf("getgitversion name disaplayed\n");

		Display_BMPDisplay(stopbmp, FB_WIDTH - 20, FB_HEIGHT-64);

		// Leave about with any key.
		if (keyspressed & KEY_ANY) {
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}
	} else if (mode == MAIN_REFLOW) {
		uint32_t ticks = RTC_Read();

		len = snprintf(buf, sizeof(buf), "%03u", Reflow_GetSetpoint());
		Display_disp_str((uint8_t*)"SET", 3, DISPLAY_ALIGN_RIGHT(3)-3, 16, DEFAULTFONT);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_RIGHT(3)-3, 32, DEFAULTFONT);

		len = snprintf(buf, sizeof(buf), "%03u", Reflow_GetActualTemp());
		Display_disp_str((uint8_t*)"ACT", 3, DISPLAY_ALIGN_RIGHT(3)-3, 64, DEFAULTFONT);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_RIGHT(3)-3, 80, DEFAULTFONT);

		len = snprintf(buf, sizeof(buf), "%03u", (unsigned int)ticks);
		Display_disp_str((uint8_t*)"RUN", 3, DISPLAY_ALIGN_RIGHT(3)-3, 112, DEFAULTFONT);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_RIGHT(3)-3, 128, DEFAULTFONT);

		// Abort reflow
		if (Reflow_IsDone() || keyspressed & KEY_S) {
//			printf("\nReflow %s\n", (Reflow_IsDone() ? "done" : "interrupted by keypress"));
			if (Reflow_IsDone()) {
				Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			}
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_SELECT_PROFILE) {
		int curprofile = Reflow_GetProfileIdx();
//		Display_FB_Clear();
		Display_FB_Clear();

		// Prev profile
		if (keyspressed & KEY_F1) {
			curprofile--;
		}
		// Next profile
		if (keyspressed & KEY_F2) {
			curprofile++;
		}

		Reflow_SelectProfileIdx(curprofile);

		Reflow_PlotProfile(-1);
		Display_BMPDisplay(selectbmp, FB_WIDTH - 20, 20);
		int eeidx = Reflow_GetEEProfileIdx();
		if (eeidx) { // Display edit button
			Display_BMPDisplay(f3editbmp, FB_WIDTH - 20, 20+29);
		}
		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		Display_disp_str((uint8_t*)buf, len, 13, 0, DEFAULTFONT);

		if (eeidx && keyspressed & KEY_F3) { // Edit ee profile
			mode = MAIN_EDIT_PROFILE;
			current_edit_profile = eeidx;
			retval = 0; // Force immediate refresh
		}

		// Select current profile
		if (keyspressed & KEY_S) {
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_BAKE) {
//		Display_FB_Clear();
		Display_FB_Clear();
		Display_disp_str((uint8_t*)"MANUAL/BAKE MODE", 16, 0, 0, DEFAULTFONT);

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

		// Setpoint-
		if (keyspressed & KEY_F1) {
			setpoint -= keyrepeataccel;
			if (setpoint < SETPOINT_MIN) setpoint = SETPOINT_MIN;
		}

		// Setpoint+
		if (keyspressed & KEY_F2) {
			setpoint += keyrepeataccel;
			if (setpoint > SETPOINT_MAX) setpoint = SETPOINT_MAX;
		}

#ifdef T962C
		// timer --
		if (keyspressed & KEY_F4) {
			if (timer - keyrepeataccel < 0) {
				// infinite bake
				timer = -1;
			} else {
				timer -= keyrepeataccel;
			}
		}
		// timer ++
		if (keyspressed & KEY_F3) {
			timer += keyrepeataccel;
		}
#else
		// timer --
		if (keyspressed & KEY_F3) {
			if (timer - keyrepeataccel < 0) {
				// infinite bake
				timer = -1;
			} else {
				timer -= keyrepeataccel;
			}
		}
		// timer ++
		if (keyspressed & KEY_F4) {
			timer += keyrepeataccel;
		}
#endif
		int y = 32;
		// display F1 button only if setpoint can be decreased
		char f1function = ' ';
		if (setpoint > SETPOINT_MIN) {
			Display_disp_str((uint8_t*)"F1", 2, 0, y, DEFAULTFONT | INVERT);
			f1function = '-';
		}
		// display F2 button only if setpoint can be increased
		char f2function = ' ';
		if (setpoint < SETPOINT_MAX) {
			Display_disp_str((uint8_t*)"F2", 2, DISPLAY_ALIGN_RIGHT(2), y, DEFAULTFONT | INVERT);
			f2function = '+';
		}
		len = snprintf(buf, sizeof(buf), "%c SETPOINT %d%c %c", f1function, (int)setpoint, 0x7F, f2function);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), y, DEFAULTFONT);




		y = 64;
		if (timer == 0) {
			len = snprintf(buf, sizeof(buf), "inf TIMER stop +");
		} else if (timer < 0) {
			len = snprintf(buf, sizeof(buf), "no timer    stop");
		} else {
			len = snprintf(buf, sizeof(buf), "- TIMER %3d:%02d +", timer / 60, timer % 60);
		}
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), y, DEFAULTFONT);

#ifdef T962C
		if (timer >= 0) {
			Display_disp_str((uint8_t*)"F4", 2, 0, y, DEFAULTFONT | INVERT);
		}
		Display_disp_str((uint8_t*)"F3", 2, DISPLAY_ALIGN_RIGHT(2), y, DEFAULTFONT | INVERT);
#else
		if (timer >= 0) {
			Display_disp_str((uint8_t*)"F3", 2, 0, y, DEFAULTFONT | INVERT);
		}
		Display_disp_str((uint8_t*)"F4", 2, DISPLAY_ALIGN_RIGHT(2), y, DEFAULTFONT | INVERT);
#endif
		y = 96;
		if (timer > 0) {
			int time_left = Reflow_GetTimeLeft();
			if (Reflow_IsPreheating()) {
				len = snprintf(buf, sizeof(buf), "PREHEAT");
			} else if (Reflow_IsDone() || time_left < 0) {
				len = snprintf(buf, sizeof(buf), "DONE");
			} else {
				len = snprintf(buf, sizeof(buf), "%d:%02d", time_left / 60, time_left % 60);
			}
			Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_RIGHT(len), y, DEFAULTFONT);
		}

		len = snprintf(buf, sizeof(buf), "ACT %3.1f%c", Sensor_GetTemp(TC_AVERAGE),0x7F);
		Display_disp_str((uint8_t*)buf, len, 0, y, DEFAULTFONT);

		y = 128;
		len = snprintf(buf, sizeof(buf), "  L %3.1f%c", Sensor_GetTemp(TC_LEFT),0x7F);
		Display_disp_str((uint8_t*)buf, len, 0, y, DEFAULTFONT);
		len = snprintf(buf, sizeof(buf), "  R %3.1f%c", Sensor_GetTemp(TC_RIGHT),0x7F);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_CENTER, y, DEFAULTFONT);

		if (Sensor_IsValid(TC_EXTRA1) || Sensor_IsValid(TC_EXTRA2)) {
			y = 144;
			if (Sensor_IsValid(TC_EXTRA1)) {
				len = snprintf(buf, sizeof(buf), " X1 %3.1f%c", Sensor_GetTemp(TC_EXTRA1),0x7F);
				Display_disp_str((uint8_t*)buf, len, 0, y, DEFAULTFONT);
			}
			if (Sensor_IsValid(TC_EXTRA2)) {
				len = snprintf(buf, sizeof(buf), " X2 %3.1f%c", Sensor_GetTemp(TC_EXTRA2),0x7F);
				Display_disp_str((uint8_t*)buf, len, DISPLAY_CENTER, y, DEFAULTFONT);
			}
		}

		y = 160;
		len = snprintf(buf, sizeof(buf), "COLDJUNCTION");
		Display_disp_str((uint8_t*)buf, len, 0, y, DEFAULTFONT);

		y += 16;
		if (Sensor_IsValid(TC_COLD_JUNCTION)) {
			len = snprintf(buf, sizeof(buf), "%3.1f%c", Sensor_GetTemp(TC_COLD_JUNCTION),0x7F);
		} else {
			len = snprintf(buf, sizeof(buf), "NOT PRESENT");
		}
		Display_disp_str((uint8_t*)buf, len, (12 * 12) - (len * 12), y, DEFAULTFONT);

		Display_BMPDisplay(stopbmp, FB_WIDTH - 20, FB_HEIGHT-64);



		Reflow_SetSetpoint(setpoint);

		if (timer > 0 && Reflow_IsDone()) {
			Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			Reflow_SetBakeTimer(0);
			Reflow_SetMode(REFLOW_STANDBY);
		}

		if (keyspressed & KEY_F3 || keyspressed & KEY_F4) {
			if (timer == 0) {
				Reflow_SetMode(REFLOW_STANDBY);
			} else {
				if (timer == -1) {
					Reflow_SetBakeTimer(0);
				} else if (timer > 0) {
					Reflow_SetBakeTimer(timer);
//					printf("\nSetting bake timer to %d\n", timer);
				}
				Reflow_SetMode(REFLOW_BAKE);
			}
		}

		// Abort bake
		if (keyspressed & KEY_S) {
//			printf("\nEnd bake mode by keypress\n");

			mode = MAIN_HOME;
			Reflow_SetBakeTimer(0);
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}

	} else if (mode == MAIN_EDIT_PROFILE) { // Edit ee1 or 2
//		Display_FB_Clear();
		Display_FB_Clear();
		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

		int16_t cursetpoint;
		Reflow_SelectEEProfileIdx(current_edit_profile);
		if (keyspressed & KEY_F1 && profile_time_idx > 0) { // Prev time
			profile_time_idx--;
		}
		if (keyspressed & KEY_F2 && profile_time_idx < 47) { // Next time
			profile_time_idx++;
		}
		cursetpoint = Reflow_GetSetpointAtIdx(profile_time_idx);
#ifdef T962C
		if (keyspressed & KEY_F4) { // Decrease setpoint
			cursetpoint -= keyrepeataccel;
		}
		if (keyspressed & KEY_F3) { // Increase setpoint
			cursetpoint += keyrepeataccel;
		}
#else
		if (keyspressed & KEY_F3) { // Decrease setpoint
			cursetpoint -= keyrepeataccel;
		}
		if (keyspressed & KEY_F4) { // Increase setpoint
			cursetpoint += keyrepeataccel;
		}
#endif
		if (cursetpoint < 0) cursetpoint = 0;
		if (cursetpoint > SETPOINT_MAX) cursetpoint = SETPOINT_MAX;
		Reflow_SetSetpointAtIdx(profile_time_idx, cursetpoint);

		Reflow_PlotProfile(profile_time_idx);
		Display_BMPDisplay(editbmp, FB_WIDTH - 20, FB_HEIGHT-64);

		len = snprintf(buf, sizeof(buf), "%02u0s %03u`", profile_time_idx, cursetpoint);
		Display_disp_str((uint8_t*)buf, len, 13, 0, DEFAULTFONT);

		// Done editing
		if (keyspressed & KEY_S) {
			Reflow_SaveEEProfile();
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}

	} else { // Main menu
//		Display_FB_Clear();
		Display_FB_Clear();

		len = snprintf(buf, sizeof(buf),"MAIN MENU");
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), 16 * 0, DEFAULTFONT);

		Display_disp_str((uint8_t*)"F1", 2, 0, 20 * 2, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)"ABOUT", 5, 30, 20 * 2, DEFAULTFONT);

		Display_disp_str((uint8_t*)"F2", 2, 0, 20 * 3, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)"SETUP", 5, 30, 20 * 3, DEFAULTFONT);

		Display_disp_str((uint8_t*)"F3", 2, 0, 20 * 4, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)"BAKE/MANUAL MODE", 16, 30, 20 * 4, DEFAULTFONT);

		Display_disp_str((uint8_t*)"F4", 2, 0, 20 * 5, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)"SELECT PROFILE", 14, 30, 20 * 5, DEFAULTFONT);

		Display_disp_str((uint8_t*)" S", 2, 0, 20 * 6, DEFAULTFONT | INVERT);
		Display_disp_str((uint8_t*)"RUN REFLOW PROFILE", 18, 30, 20 * 6, DEFAULTFONT);

		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_RIGHT(len), 20 * 7, DEFAULTFONT | INVERT);

		len = snprintf(buf,sizeof(buf), "OVEN TEMPERATURE %d%c", Reflow_GetActualTemp(),0x7F);
		Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), FB_HEIGHT-16, DEFAULTFONT);


		// Make sure reflow complete beep is silenced when pressing any key
		if (keyspressed) {
			Buzzer_Beep(BUZZ_NONE, 0, 0);
		}

		// About
		if (keyspressed & KEY_F1) {
			mode = MAIN_ABOUT;
			retval = 0; // Force immediate refresh
		}
		if (keyspressed & KEY_F2) { // Setup/cal
			mode = MAIN_SETUP;
			Reflow_SetMode(REFLOW_STANDBYFAN);
			retval = 0; // Force immediate refresh
		}

		// Bake mode
		if (keyspressed & KEY_F3) {
			mode = MAIN_BAKE;
			Reflow_Init();
			retval = 0; // Force immediate refresh
		}

		// Select profile
		if (keyspressed & KEY_F4) {
			mode = MAIN_SELECT_PROFILE;
			retval = 0; // Force immediate refresh
		}

		// Start reflow
		if (keyspressed & KEY_S) {
			mode = MAIN_REFLOW;
//			Display_FB_Clear();
			Display_FB_Clear();
//			printf("\nStarting reflow with profile: %s", Reflow_GetProfileName());
			Reflow_Init();
			Reflow_PlotProfile(-1);
			len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
			Display_disp_str((uint8_t*)buf, len, DISPLAY_ALIGN_CENTER(len), 0, DEFAULTFONT);
			Reflow_SetMode(REFLOW_REFLOW);
			Display_BMPDisplay(stopbmp, FB_WIDTH - 20, FB_HEIGHT-64);
			retval = 0; // Force immediate refresh
		}
	}

	//Display_FB_Update();
	Display_FB_Update();


	return retval;
}
