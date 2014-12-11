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
#include "buzzer.h"
#include "nvstorage.h"
#include "version.h"

extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t selectbmp[];
extern uint8_t editbmp[];

// No version.c file generated for LPCXpresso builds, fall back to this
__attribute__((weak)) const char* Version_GetGitVersion(void) {
    return "no version info";
}

// Support for boot ROM functions (get part number etc)
typedef void (*IAP)(unsigned int [],unsigned int[]);
IAP iap_entry = (void*)0x7ffffff1;
#define IAP_READ_PART (54)
#define PART_REV_ADDR (0x0007D070)
typedef struct {
	const char* name;
	const uint32_t id;
} partmapStruct;

partmapStruct partmap[] = {
		{"LPC2131(/01)", 0x0002ff01}, // Probably pointless but present for completeness (32kB flash is too small for factory image)
		{"LPC2132(/01)", 0x0002ff11},
		{"LPC2134(/01)", 0x0002ff12},
		{"LPC2136(/01)", 0x0002ff23},
		{"LPC2138(/01)", 0x0002ff25},

		{"LPC2141", 0x0402ff01}, // Probably pointless but present for completeness (32kB flash is too small for factory image)
		{"LPC2142", 0x0402ff11},
		{"LPC2144", 0x0402ff12},
		{"LPC2146", 0x0402ff23},
		{"LPC2148", 0x0402ff25},
};
#define NUM_PARTS (sizeof(partmap)/sizeof(partmap[0]))

uint32_t partid,partrev;
uint32_t command[1];
uint32_t result[3];

static int32_t Main_Work( void );

int main(void) {
	char buf[22];
	int len;

	PLLCFG = (1<<5) | (4<<0); //PLL MSEL=0x4 (+1), PSEL=0x1 (/2) so 11.0592*5 = 55.296MHz, Fcco = (2x55.296)*2 = 221MHz which is within 156 to 320MHz
	PLLCON = 0x01;
	PLLFEED = 0xaa;
	PLLFEED = 0x55; // Feed complete
	while(!(PLLSTAT & (1<<10))); // Wait for PLL to lock
	PLLCON = 0x03;
	PLLFEED = 0xaa;
	PLLFEED = 0x55; // Feed complete
	VPBDIV = 0x01; // APB runs at the same frequency as the CPU (55.296MHz)
	MAMTIM = 0x03; // 3 cycles flash access recommended >40MHz
	MAMCR = 0x02; // Fully enable memory accelerator
	
	Sched_Init();
	IO_Init();
	Set_Heater(0);
	Set_Fan(0);
	Serial_Init();
	printf(	"\nT-962-controller open source firmware (%s)" \
			"\n" \
			"\nSee https://github.com/UnifiedEngineering/T-962-improvement for more details." \
			"\n" \
			"\nInitializing improved reflow oven...", Version_GetGitVersion());
	I2C_Init();
	EEPROM_Init();
	NV_Init();

	if( NV_GetConfig(REFLOW_BEEP_DONE_LEN) == 255 ) {
		NV_SetConfig(REFLOW_BEEP_DONE_LEN, 10); // Default 1 second beep length
	}

	if( NV_GetConfig(REFLOW_MIN_FAN_SPEED) == 255 ) {
		NV_SetConfig(REFLOW_MIN_FAN_SPEED, 8); // Default fan speed is now 8
	}

	LCD_Init();
	LCD_BMPDisplay(logobmp,0,0);

	// Setup watchdog
	WDTC = PCLKFREQ / 3; // Some margin (PCLKFREQ/4 would be exactly the period the WD is fed by sleep_work)
	WDMOD = 0x03; // Enable
	WDFEED = 0xaa;
	WDFEED = 0x55;

	uint8_t resetreason = RSIR;
	RSIR = 0x0f; // Clear it out
	printf("\nReset reason(s): %s%s%s%s", (resetreason&(1<<0))?"[POR]":"", (resetreason&(1<<1))?"[EXTR]":"",
			(resetreason&(1<<2))?"[WDTR]":"", (resetreason&(1<<3))?"[BODR]":"");

	// Request part number
	command[0] = IAP_READ_PART;
	iap_entry(command, result);
	const char* partstrptr = NULL;
	for(int i=0; i<NUM_PARTS; i++) {
		if(result[1] == partmap[i].id) {
			partstrptr = partmap[i].name;
			break;
		}
	}
	// Read part revision
	partrev=*(uint8_t*)PART_REV_ADDR;
	if(partrev==0 || partrev > 0x1a) {
		partrev = '-';
	} else {
		partrev += 'A' - 1;
	}
	len = snprintf(buf,sizeof(buf),"%s rev %c",partstrptr,partrev);
	LCD_disp_str((uint8_t*)buf, len, 0, 64-6, FONT6X6);
	printf("\nRunning on an %s", buf);

	len = snprintf(buf,sizeof(buf),"%s",Version_GetGitVersion());
	LCD_disp_str((uint8_t*)buf, len, 128-(len*6), 0, FONT6X6);

	LCD_FB_Update();
	Keypad_Init();
	Buzzer_Init();
	ADC_Init();
	RTC_Init();
	OneWire_Init();
	Reflow_Init();

	Sched_SetWorkfunc( MAIN_WORK, Main_Work );
	Sched_SetState( MAIN_WORK, 1, TICKS_SECS( 2 ) ); // Enable in 2 seconds

	Buzzer_Beep( BUZZ_1KHZ, 255, TICKS_MS(100) );

	while(1) {
		int32_t sleeptime;
		sleeptime=Sched_Do( 0 ); // No fast-forward support
		//printf("\n%d ticks 'til next activity"),sleeptime);
	}
	return 0;
}

static int32_t Main_Work( void ) {
	static uint32_t mode=0;
	static uint32_t setpoint=30;
	static uint8_t curidx=0;
	int32_t retval=TICKS_MS(500);

	char buf[22];
	int len;

	uint32_t keyspressed=Keypad_Get();

	// Sort out this "state machine"
	if(mode==5) { // Run reflow
		uint32_t ticks=RTC_Read();
		//len = snprintf(buf,sizeof(buf),"seconds:%d",ticks);
		//LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
		len = snprintf(buf,sizeof(buf),"%03u",Reflow_GetSetpoint());
		LCD_disp_str((uint8_t*)"SET", 3, 110, 7, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 13, FONT6X6);
		len = snprintf(buf,sizeof(buf),"%03u",Reflow_GetActualTemp());
		LCD_disp_str((uint8_t*)"ACT", 3, 110, 20, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 26, FONT6X6);
		len = snprintf(buf,sizeof(buf),"%03u",ticks);
		LCD_disp_str((uint8_t*)"RUN", 3, 110, 33, FONT6X6);
		LCD_disp_str((uint8_t*)buf, len, 110, 39, FONT6X6);
		if(Reflow_IsDone() || keyspressed & KEY_S) { // Abort reflow
			if( Reflow_IsDone() ) Buzzer_Beep( BUZZ_1KHZ, 255, TICKS_MS(100) * NV_GetConfig(REFLOW_BEEP_DONE_LEN) );
			mode=0;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}
	} else if(mode==4) { // Select profile
		int curprofile = Reflow_GetProfileIdx();

		LCD_FB_Clear();

		if(keyspressed & KEY_F1) { // Prev profile
			curprofile--;
		}
		if(keyspressed & KEY_F2) { // Next profile
			curprofile++;
		}
		Reflow_SelectProfileIdx(curprofile);
		Reflow_PlotProfile(-1);
		LCD_BMPDisplay(selectbmp,127-17,0);
		len = snprintf(buf,sizeof(buf),"%s",Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);

		if(keyspressed & KEY_S) { // Select current profile
			mode=0;
			retval = 0; // Force immediate refresh
		}
	} else if(mode==3) { // Bake
		LCD_FB_Clear();
		LCD_disp_str((uint8_t*)"MANUAL/BAKE MODE", 16, 0, 0, FONT6X6);
		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if( keyrepeataccel < 1) keyrepeataccel = 1;
		if( keyrepeataccel > 30) keyrepeataccel = 30;

		if(keyspressed & KEY_F1) { // Setpoint-
			setpoint -= keyrepeataccel;
			if(setpoint<30) setpoint = 30;
		}
		if(keyspressed & KEY_F2) { // Setpoint+
			setpoint += keyrepeataccel;
			if(setpoint>300) setpoint = 300;
		}

		len = snprintf(buf,sizeof(buf),"- SETPOINT %uC +",setpoint);
		LCD_disp_str((uint8_t*)buf, len, 64-(len*3), 10, FONT6X6);

		LCD_disp_str((uint8_t*)"F1", 2, 0, 10, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"F2", 2, 127-12, 10, FONT6X6 | INVERT);

		len = snprintf(buf,sizeof(buf),"ACTUAL %.1fC",Reflow_GetTempSensor(TC_AVERAGE));
		LCD_disp_str((uint8_t*)buf, len, 64-(len*3), 18, FONT6X6);

		len = snprintf(buf,sizeof(buf),"L %.1fC",Reflow_GetTempSensor(TC_LEFT));
		LCD_disp_str((uint8_t*)buf, len, 32-(len*3), 26, FONT6X6);

		len = snprintf(buf,sizeof(buf),"R %.1fC",Reflow_GetTempSensor(TC_RIGHT));
		LCD_disp_str((uint8_t*)buf, len, 96-(len*3), 26, FONT6X6);

		if( Reflow_IsTempSensorValid(TC_EXTRA1) ) {
			len = snprintf(buf,sizeof(buf),"X1 %.1fC",Reflow_GetTempSensor(TC_EXTRA1));
			LCD_disp_str((uint8_t*)buf, len, 32-(len*3), 34, FONT6X6);
		}

		if( Reflow_IsTempSensorValid(TC_EXTRA2) ) {
			len = snprintf(buf,sizeof(buf),"X2 %.1fC",Reflow_GetTempSensor(TC_EXTRA2));
			LCD_disp_str((uint8_t*)buf, len, 96-(len*3), 34, FONT6X6);
		}

		if( Reflow_IsTempSensorValid(TC_COLD_JUNCTION) ) {
			len = snprintf(buf,sizeof(buf),"COLD-JUNCTION %.1fC",Reflow_GetTempSensor(TC_COLD_JUNCTION));
		} else {
			len = snprintf(buf,sizeof(buf),"NO COLD-JUNCTION TS!");
		}
		LCD_disp_str((uint8_t*)buf, len, 64-(len*3), 42, FONT6X6);

		LCD_BMPDisplay(stopbmp,127-17,0);

//		len = snprintf(buf,sizeof(buf),"heat=0x%02x fan=0x%02x",heat,fan);
//		LCD_disp_str((uint8_t*)buf, len, 0, 63-5, FONT6X6);

		// Add timer for bake at some point

		Reflow_SetSetpoint(setpoint);

		if(keyspressed & KEY_S) { // Abort bake
			mode=0;
			Reflow_SetMode(REFLOW_STANDBY);
			retval = 0; // Force immediate refresh
		}
	} else if(mode==2 || mode==1) { // Edit ee1 or 2
		LCD_FB_Clear();
		int keyrepeataccel = keyspressed >> 17; // Divide the value by 2
		if( keyrepeataccel < 1) keyrepeataccel = 1;
		if( keyrepeataccel > 30) keyrepeataccel = 30;

		int16_t cursetpoint;
		Reflow_SelectEEProfileIdx(mode);
		if(keyspressed & KEY_F1 && curidx > 0) { // Prev time
			curidx--;
		}
		if(keyspressed & KEY_F2 && curidx < 47) { // Next time
			curidx++;
		}
		cursetpoint = Reflow_GetSetpointAtIdx( curidx );

		if(keyspressed & KEY_F3) { // Decrease setpoint
			cursetpoint -= keyrepeataccel;
		}
		if(keyspressed & KEY_F4) { // Increase setpoint
			cursetpoint += keyrepeataccel;
		}
		if(cursetpoint<0) cursetpoint = 0;
		if(cursetpoint>300) cursetpoint = 300;
		Reflow_SetSetpointAtIdx( curidx, cursetpoint);

		Reflow_PlotProfile(curidx);
		LCD_BMPDisplay(editbmp,127-17,0);
		len = snprintf(buf,sizeof(buf),"%02u0s %03uC",curidx, cursetpoint);
		LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
		if(keyspressed & KEY_S) { // Done editing
			Reflow_SaveEEProfile();
			mode=0;
			retval = 0; // Force immediate refresh
		}
	} else { // Main menu
		LCD_FB_Clear();

		len = snprintf(buf,sizeof(buf),"MAIN MENU");
		LCD_disp_str((uint8_t*)buf, len, 0, 6*0, FONT6X6);
		LCD_disp_str((uint8_t*)"F1", 2, 0, 8*1, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"EDIT CUSTOM 1", 13, 14, 8*1, FONT6X6);
		LCD_disp_str((uint8_t*)"F2", 2, 0, 8*2, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"EDIT CUSTOM 2", 13, 14, 8*2, FONT6X6);
		LCD_disp_str((uint8_t*)"F3", 2, 0, 8*3, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"BAKE/MANUAL MODE", 16, 14, 8*3, FONT6X6);
		LCD_disp_str((uint8_t*)"F4", 2, 0, 8*4, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"SELECT PROFILE", 14, 14, 8*4, FONT6X6);
		LCD_disp_str((uint8_t*)"S", 1, 3, 8*5, FONT6X6 | INVERT);
		LCD_disp_str((uint8_t*)"RUN REFLOW PROFILE", 18, 14, 8*5, FONT6X6);
		len = snprintf(buf,sizeof(buf),"%s",Reflow_GetProfileName());
		LCD_disp_str((uint8_t*)buf, len, 64-(len*3), 8*6, FONT6X6 | INVERT);
		len = snprintf(buf,sizeof(buf),"OVEN TEMPERATURE %dC", Reflow_GetActualTemp());
		LCD_disp_str((uint8_t*)buf, len, 64-(len*3), 64-6, FONT6X6);

		if(keyspressed & KEY_F1) { // Edit ee1
			curidx=0;
			mode=1;
			retval = 0; // Force immediate refresh
		}
		if(keyspressed & KEY_F2) { // Edit ee2
			curidx=0;
			mode=2;
			retval = 0; // Force immediate refresh
		}
		if(keyspressed & KEY_F3) { // Bake mode
			mode=3;
			Reflow_Init();
			Reflow_SetMode(REFLOW_BAKE);
			retval = 0; // Force immediate refresh
		}
		if(keyspressed & KEY_F4) { // Select profile
			mode=4;
			retval = 0; // Force immediate refresh
		}
		if(keyspressed & KEY_S) { // Start reflow
			mode=5;
			LCD_FB_Clear();
			Reflow_Init();
			Reflow_PlotProfile(-1);
			LCD_BMPDisplay(stopbmp,127-17,0);
			len = snprintf(buf,sizeof(buf),"%s",Reflow_GetProfileName());
			LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
			Reflow_SetMode(REFLOW_REFLOW);
			retval = 0; // Force immediate refresh
		}
	}

	LCD_FB_Update();

	return retval;
}
