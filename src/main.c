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
#include <stdlib.h>
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
#include "ui_extras.h"

extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t selectbmp[];
extern uint8_t editbmp[];
extern uint8_t f3editbmp[];
extern uint8_t graph2bmp[];

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
	int len=0;

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

	len = IO_Partinfo(buf, sizeof(buf), "%s rev %c");
	//LCD_disp_str((uint8_t*)buf, len, 0, 64 - 6, FONT6X6);
	printf("\nRunning on an %s", buf);

	len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
	//LCD_disp_str((uint8_t*)buf, len, 128 - (len * 6), 0, FONT6X6);

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
	Sched_SetState(MAIN_WORK, 1, TICKS_SECS(3)); // Enable in 3 seconds

	Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(50));

	while (1) {
#ifdef ENABLE_SLEEP
//		int32_t sleeptime;
//		sleeptime = Sched_Do(0); // No fast-forward support
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
	MAIN_REFLOW,
	MAIN_SCREENSAVER,
	MAIN_INIT
} MainMode_t;

static char buf[25];
static int len;
static uint16_t animCnt=0;
static int16_t animIX=0,animIY=0;
static uint8_t blinkCnt=0,blinkOn=0;


static int32_t Main_Work(void) {
	static MainMode_t mode = MAIN_HOME;
	static MainMode_t prevMode = MAIN_INIT;
	static uint16_t setpoint = 0;
	static uint8_t modeChange=0;

	if (setpoint == 0) {
		Reflow_LoadSetpoint();
		setpoint = Reflow_GetSetpoint();
	}
	static int timer = 0;

	// profile editing
	static uint8_t profile_time_idx = 0;
	static uint8_t current_edit_profile;

	int32_t retval = TICKS_MS(500);

	uint32_t keyspressed = Keypad_Get();

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
				printf("\nCannot understand command, ? for help\n");
			}
		}
	}

	// Set flag if mode was changed by user action (to minimise redraws)
	if(mode!=prevMode){
		prevMode=mode;
		modeChange=1;
		animCnt=0;
		animIX=0;
		animIY=0;
	}else{
		modeChange=0;
		++animCnt;
	}

	if(++blinkCnt>=5){
		blinkOn=(blinkOn==0?1:0);
		blinkCnt=0;
	}




	// main menu state machine
	// setup/calibration
	if (mode == MAIN_SETUP) {
		static int8_t selected = 0;
		static int8_t scrollPos = 0;
		int y = 0;

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1) keyrepeataccel = 1;
		if (keyrepeataccel > 30) keyrepeataccel = 30;

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

		LCD_FB_Clear();
		retval = TICKS_MS(100);

		showHeader("Setup/calibration");
		for(uint8_t n=0;n<128;n++){
			LCD_SetPixel(n,7);
			LCD_SetPixel(n,64-9);
		}

		y += 11;

		int8_t maxItems=6;
		int8_t numItems=Setup_getNumItems();
		int8_t endItem=scrollPos+maxItems;

		if(scrollPos>selected){
			scrollPos=selected;
			endItem=scrollPos+maxItems;
		}else if(selected>=endItem){
			endItem=selected+1;
			scrollPos=endItem-maxItems;
		}

		if(scrollPos<0){
			scrollPos=0;
			endItem=maxItems;
		}

		if(endItem>numItems){
			endItem=numItems;
		}

		if(endItem-scrollPos>maxItems){
			endItem=scrollPos+maxItems;
		}

		for (int i = scrollPos; i < endItem ; i++) {
			len = Setup_snprintFormattedValue(buf, sizeof(buf), i);
			LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6 | (selected == i) ? INVERT : 0);
			y += 7;
		}

		// buttons
		y = 64 - 7;
		LCD_disp_str((uint8_t*)" < ", 3, 0, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" > ", 3, 20, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" - ", 3, 45, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" + ", 3, 65, y, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)" DONE ", 6, 91, y, FONT6X6 | INVERT);

		// Leave setup
		if (keyspressed & KEY_S) {
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}



	// About
	} else if (mode == MAIN_ABOUT) {
		if(modeChange){
			LCD_FB_Clear();
			LCD_BMPDisplay(logobmp, 0, 0);
			uint8_t n,i;

			for(n=0;n<8;n++){
				for(i=0;i<5;i++){
					if( (i>1) || (n>0 && i>0) || (n>1 && i==0) )
						LCD_disp_str((uint8_t*)" ", 1, (128-8*6)+(n*6), 26+(i*7), FONT6X6|INVERT);
				}
			}

			for(n=0;n<22;n++){
				LCD_disp_str((uint8_t*)" ", 1, n*6, 2, FONT6X6);
				LCD_disp_str((uint8_t*)" ", 1, n*6, 64-10, FONT6X6);
				LCD_disp_str((uint8_t*)" ", 1, n*6, 64-7, FONT6X6);
			}

			for(n=0;n<128;n++){
				LCD_SetPixel(n,7);
				LCD_SetPixel(n,64-9);
			}


			len = snprintf(buf, sizeof(buf), "T-962 OVEN");
			LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_RIGHT(len), 20, FONT6X6);
			len = snprintf(buf, sizeof(buf), "SMASHCAT UI");
			LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_RIGHT(len), 28, FONT6X6);
			len = snprintf(buf, sizeof(buf), "%s", Version_GetGitVersion());
			LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_RIGHT(len), 36, FONT6X6);
			len = snprintf(buf, sizeof(buf), "UNIFIED ENGINEERING");
			LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_RIGHT(len), 44, FONT6X6);

			LCD_disp_str((uint8_t*)"  ", 2, 128-12, 64-7, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"S", 1, 128-9, 64-7, FONT6X6 | INVERT);

		}
		retval = TICKS_MS(100);
		showHeader("ABOUT");

		// Leave about with any key.
		if (keyspressed & KEY_ANY) {
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}


	// Reflow active!
	} else if (mode == MAIN_REFLOW) {
		displayReflowScreen(keyspressed,modeChange);
		retval = TICKS_MS(100);

		// Abort reflow
		if (Reflow_IsDone() || keyspressed & KEY_S) {
			printf("\nReflow %s\n", (Reflow_IsDone() ? "done" : "interrupted by keypress"));
			if (Reflow_IsDone()) {
				Buzzer_Beep(BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN));
			}
			mode = MAIN_HOME;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}




	// Select reflow profile
	} else if (mode == MAIN_SELECT_PROFILE) {
		int curprofile = Reflow_GetProfileIdx();
		LCD_FB_Clear();

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
		LCD_BMPDisplay(selectbmp, 127 - 17, 0);
		int eeidx = Reflow_GetEEProfileIdx();
		if (eeidx) { // Display edit button
			LCD_BMPDisplay(f3editbmp, 127 - 17, 29);
		}
		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);

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




	// Manual bake mode
	} else if (mode == MAIN_BAKE) {
		LCD_FB_Clear();
		retval = TICKS_MS(100);
		showHeader("MANUAL/BAKE MODE");
		for(uint8_t n=0;n<128;n++){
			LCD_SetPixel(n,7);
			LCD_SetPixel(n,64-9);
		}

		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1)
			keyrepeataccel = 1;
		if (keyrepeataccel > 30)
			keyrepeataccel = 30;

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

		int y = 10;
		// display F1 button only if setpoint can be decreased
		char f1function = ' ';
		if (setpoint > SETPOINT_MIN) {
			LCD_disp_str((uint8_t*)"F1", 2, 0, y, FONT6X6 | INVERT);
			f1function = '-';
		}
		// display F2 button only if setpoint can be increased
		char f2function = ' ';
		if (setpoint < SETPOINT_MAX) {
			LCD_disp_str((uint8_t*)"F2", 2, LCD_ALIGN_RIGHT(2), y, FONT6X6 | INVERT);
			f2function = '+';
		}
		len = snprintf(buf, sizeof(buf), "%c SETPOINT %d` %c", f1function, (int)setpoint, f2function);
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);


		y = 18;
		if (timer == 0) {
			len = snprintf(buf, sizeof(buf), "inf TIMER stop +");
		} else if (timer < 0) {
			len = snprintf(buf, sizeof(buf), "no timer    stop");
		} else {
			len = snprintf(buf, sizeof(buf), "- TIMER %3d:%02d +", timer / 60, timer % 60);
		}
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);

		if (timer >= 0) {
			LCD_disp_str((uint8_t*)"F3", 2, 0, y, FONT6X6 | INVERT);
		}
		LCD_disp_str((uint8_t*)"F4", 2, LCD_ALIGN_RIGHT(2), y, FONT6X6 | INVERT);

		y = 27;
		if (timer > 0) {
			int time_left = Reflow_GetTimeLeft();
			if (Reflow_IsPreheating()) {
				len = snprintf(buf, sizeof(buf), "PREHEAT");
				LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, (blinkOn==1?FONT6X6|INVERT:FONT6X6));
			} else if (Reflow_IsDone() || time_left < 0) {
				len = snprintf(buf, sizeof(buf), "DONE");
				LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, (blinkOn==1?FONT6X6|INVERT:FONT6X6));
			} else {
				len = snprintf(buf, sizeof(buf), "%d:%02d", time_left / 60, time_left % 60);
				LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);
			}
		}

		y = 36;
		len = snprintf(buf, sizeof(buf), "OVEN TEMP %3.1f`", Sensor_GetTemp(TC_AVERAGE));
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), y, FONT6X6);

		y = 44;
		len = snprintf(buf, sizeof(buf), "  L %3.1f`", Sensor_GetTemp(TC_LEFT));
		LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);
		len = snprintf(buf, sizeof(buf), "  R %3.1f`", Sensor_GetTemp(TC_RIGHT));
		LCD_disp_str((uint8_t*)buf, len, LCD_CENTER, y, FONT6X6);

		if (Sensor_IsValid(TC_EXTRA1) || Sensor_IsValid(TC_EXTRA2)) {
			y = 42;
			if (Sensor_IsValid(TC_EXTRA1)) {
				len = snprintf(buf, sizeof(buf), " X1 %3.1f`", Sensor_GetTemp(TC_EXTRA1));
				LCD_disp_str((uint8_t*)buf, len, 0, y, FONT6X6);
			}
			if (Sensor_IsValid(TC_EXTRA2)) {
				len = snprintf(buf, sizeof(buf), " X2 %3.1f`", Sensor_GetTemp(TC_EXTRA2));
				LCD_disp_str((uint8_t*)buf, len, LCD_CENTER, y, FONT6X6);
			}
		}

		y = 50;
		len = snprintf(buf, sizeof(buf), "COLD JUNCTION");
		LCD_disp_str((uint8_t*)buf, len, 0, 64-7, FONT6X6);

		y += 8;
		if (Sensor_IsValid(TC_COLD_JUNCTION)) {
			len = snprintf(buf, sizeof(buf), "%3.1f`", Sensor_GetTemp(TC_COLD_JUNCTION));
		} else {
			len = snprintf(buf, sizeof(buf), "ERR");
		}
		LCD_disp_str((uint8_t*)buf, len, 123-(7 * 6), 64-7, FONT6X6);

		//LCD_BMPDisplay(stopbmp, 127 - 17, 0);
		LCD_disp_str((uint8_t*)"  ", 2, 128-12, 64-7, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"S", 1, 128-9, 64-7, FONT6X6 | INVERT);

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
					printf("\nSetting bake timer to %d\n", timer);
				}
				Reflow_SetMode(REFLOW_BAKE);
			}
		}

		// Abort bake
		if (keyspressed & KEY_S) {
			printf("\nEnd bake mode by keypress\n");

			mode = MAIN_HOME;
			Reflow_SetBakeTimer(0);
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}




	// Edit profile
	} else if (mode == MAIN_EDIT_PROFILE) { // Edit ee1 or 2
		LCD_FB_Clear();
		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if (keyrepeataccel < 1)
			keyrepeataccel = 1;
		if (keyrepeataccel > 30)
			keyrepeataccel = 30;

		int16_t cursetpoint;
		Reflow_SelectEEProfileIdx(current_edit_profile);
		if (keyspressed & KEY_F1 && profile_time_idx > 0) { // Prev time
			profile_time_idx--;
		}
		if (keyspressed & KEY_F2 && profile_time_idx < 47) { // Next time
			profile_time_idx++;
		}
		cursetpoint = Reflow_GetSetpointAtIdx(profile_time_idx);

		if (keyspressed & KEY_F3) { // Decrease setpoint
			cursetpoint -= keyrepeataccel;
		}
		if (keyspressed & KEY_F4) { // Increase setpoint
			cursetpoint += keyrepeataccel;
		}
		if (cursetpoint < 0) cursetpoint = 0;
		if (cursetpoint > SETPOINT_MAX) cursetpoint = SETPOINT_MAX;
		Reflow_SetSetpointAtIdx(profile_time_idx, cursetpoint);

		Reflow_PlotProfile(profile_time_idx);
		LCD_BMPDisplay(editbmp, 127 - 17, 0);

		len = snprintf(buf, sizeof(buf), "%01u:%02u %03u`", (profile_time_idx*10)/60,(profile_time_idx*10)%60 , cursetpoint);
		LCD_disp_str((uint8_t*)buf, len, 33, 0, FONT6X6);

		// Done editing
		if (keyspressed & KEY_S) {
			Reflow_SaveEEProfile();
			mode = MAIN_HOME;
			retval = 0; // Force immediate refresh
		}



	// Edit profile
	} else if (mode == MAIN_SCREENSAVER) {

		if(animIY==0){
			if(modeChange){
				initSprites();
			}
			if(++animIX==32){
				animIY=1;
			}else{
				LCD_ScrollDisplay();
				LCD_ScrollDisplay();
			}
		}else{
			drawSprites();

			if (keyspressed & KEY_S) {
				retval=0;
				mode=MAIN_HOME;
			}
		}
		retval = TICKS_MS(50);	// approx 20fps - LCD panel on oven can't really update any quicker without looking terrible...

	// Main menu
	} else {
		if(modeChange){
			LCD_FB_Clear();
			initScreensaverTimeout();
			LCD_disp_str((uint8_t*)"F1", 2, 0,     (8 * 1)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"ABOUT", 5, 14, (8 * 1)+1, FONT6X6);
			LCD_disp_str((uint8_t*)"F2", 2, 0,     (8 * 2)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"SETUP", 5, 14, (8 * 2)+1, FONT6X6);
			LCD_disp_str((uint8_t*)"F3", 2, 0,     (8 * 3)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"BAKE/MANUAL MODE", 16, 14, (8 * 3)+1, FONT6X6);
			LCD_disp_str((uint8_t*)"F4", 2, 0, (8 * 4)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"SELECT PROFILE", 14, 14, (8 * 4)+1, FONT6X6);
			LCD_disp_str((uint8_t*)"  ", 2, 0, (8 * 5)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"S", 1, 3, (8 * 5)+1, FONT6X6 | INVERT);
			LCD_disp_str((uint8_t*)"RUN REFLOW PROFILE", 18, 14, (8 * 5)+1, FONT6X6);
			for(uint8_t n=0;n<128;n++){
				LCD_SetPixel(n,7);
				LCD_SetPixel(n,63-6);
			}
		}

		showHeader("MAIN MENU");

		len = snprintf(buf, sizeof(buf), "%s", Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), (8 * 6)+1, FONT6X6 | INVERT);

		len = snprintf(buf,sizeof(buf), "OVEN TEMPERATURE %d`", Reflow_GetActualTemp());
		LCD_disp_str((uint8_t*)buf, len, LCD_ALIGN_CENTER(len), 64 - 6, FONT6X6);

		// Make sure reflow complete beep is silenced when pressing any key
		if (keyspressed) {
			Buzzer_Beep(BUZZ_NONE, 0, 0);
		}

		retval = TICKS_MS(100);

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
			LCD_FB_Clear();
			printf("\nStarting reflow with profile: %s", Reflow_GetProfileName());
			Reflow_Init();
			Reflow_SetMode(REFLOW_REFLOW);
			retval = 0; // Force immediate refresh
		}

		if(timeForScreensaver()){
			mode=MAIN_SCREENSAVER;
		}
	}

	LCD_FB_Update();

	return retval;
}
