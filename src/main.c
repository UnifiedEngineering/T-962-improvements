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
#include "timer.h"
#include "onewire.h"
#include "adc.h"
#include "i2c.h"
#include "rtc.h"
#include "eeprom.h"
#include "keypad.h"
#include "reflow.h"
#include "timer.h"

extern uint8_t logobmp[];
extern uint8_t stopbmp[];
extern uint8_t selectbmp[];
extern uint8_t editbmp[];

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
		{"LPC2131(/01)", 0x0002ff01},
		{"LPC2132(/01)", 0x0002ff11},
		{"LPC2134(/01)", 0x0002ff12},
		{"LPC2136(/01)", 0x0002ff23},
		{"LPC2138(/01)", 0x0002ff25},

		{"LPC2141", 0x0402ff01},
		{"LPC2142", 0x0402ff11},
		{"LPC2144", 0x0402ff12},
		{"LPC2146", 0x0402ff23},
		{"LPC2148", 0x0402ff25},
};
#define NUM_PARTS (sizeof(partmap)/sizeof(partmap[0]))

uint32_t partid,partrev;
uint32_t command[1];
uint32_t result[3];

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
	
	Timer_Init();
	IO_Init();
	Set_Heater(0);
	Set_Fan(0);
	Serial_Init();
	printf("\nInitializing improved reflow oven...");
	LCD_Init();
	LCD_BMPDisplay(logobmp,0,0);

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

	LCD_FB_Update();
	ADC_Init();
	I2C_Init();
	EEPROM_Init();
	RTC_Init();
	OneWire_Init();
	Reflow_Init();

	uint32_t mode=0;
	uint8_t heat=0,fan=0;
	int32_t temp[3];

	float temperature[2];
	temperature[0]=0.0f;
	temperature[1]=0.0f;
	uint32_t setpoint=30;

	BusyWait8( 2000000 << 3 ); // Delay 2 seconds

	while(1) {
		float coldjunction;
		uint32_t keyspressed=Keypad_Poll();
		if(keyspressed) printf("\nKeypad %02x ",keyspressed);
		//len = snprintf(buf,sizeof(buf),"timer:<%010d>",Timer_Get());
		//LCD_disp_str((uint8_t*)buf, len, 0, 63-5, FONT6X6);
		//LCD_FB_Update();

		float avgtemp; // The feedback temperature

		// Initiate temperature conversion on all connected 1-wire sensors
		OneWire_PerformTemperatureConversion();

		printf("\n");

		// These are the temperature readings we get from the thermocouple interfaces
		// Right now it is assumed that if they are indeed present the first two channels will be used as feedback
		float tctemp[4];
		uint8_t tcpresent[4];
		for( int i=0; i<4; i++ ) { // Get 4 TC channels
			tcpresent[i] = 0;
			tctemp[i] = OneWire_GetTCReading( i );
			if( tctemp[i] < 999.0f && tctemp[i] != 0.0f ) tcpresent[i] = 1;
			if( tcpresent[i] ) {
				printf("(TC%x %.1fC)",i,tctemp[i]);
			} else {
				//printf("(TC%x ---C)",i);
			}
		}
		if(tcpresent[0] && tcpresent[1]) {
			avgtemp = (tctemp[0] + tctemp[1]) / 2.0f;
			coldjunction = 0.0f; // Todo, actually read out CJ temp from TC IF
		} else {
			// If the external TC interface is not present we fall back to the built-in ADC, with or without compensation
			coldjunction=OneWire_GetTempSensorReading();
			temp[0]=ADC_Read(1);
			if(temp[0]>=0) temp[1]=temp[0];
			temp[0]=ADC_Read(2);
			if(temp[0]>=0) temp[2]=temp[0];
			printf("(ADC readout 0x%03x 0x%03x) ",temp[1],temp[2]);
			temperature[0] = (float)temp[1];
			temperature[1] = (float)temp[2];
			// Change gain here
			temperature[0] += coldjunction -6.0f; // Offset adjust, this will definitely have to be calibrated per device
			temperature[1] += coldjunction -2.0f;

			avgtemp=(temperature[0]+temperature[1]) / 2.0f;
			printf("L=%03.1fC R=%03.1fC CJ=%03.1fC",
				temperature[0],temperature[1],coldjunction);
		}
		printf(" AVG=%03.1fC SP=%03dC Heat=0x%02x Fan=0x%02x",
			avgtemp,setpoint,heat,fan);

		// Sort out this "state machine"
		if(mode==5) { // Run reflow
			uint32_t ticks=RTC_Read();
			//len = snprintf(buf,sizeof(buf),"seconds:%d",ticks);
			//LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
			len = snprintf(buf,sizeof(buf),"%03u",Reflow_GetSetpoint());
			LCD_disp_str((uint8_t*)"SET", 3, 110, 7, FONT6X6);
			LCD_disp_str((uint8_t*)buf, len, 110, 13, FONT6X6);
			len = snprintf(buf,sizeof(buf),"%03u",(uint16_t)avgtemp);
			LCD_disp_str((uint8_t*)"ACT", 3, 110, 20, FONT6X6);
			LCD_disp_str((uint8_t*)buf, len, 110, 26, FONT6X6);
			len = snprintf(buf,sizeof(buf),"%03u",ticks);
			LCD_disp_str((uint8_t*)"RUN", 3, 110, 33, FONT6X6);
			LCD_disp_str((uint8_t*)buf, len, 110, 39, FONT6X6);
			if(Reflow_Run(ticks, avgtemp, &heat, &fan, 0) || keyspressed & KEY_S) { // Abort reflow
				mode=0;
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
			Reflow_PlotProfile();
			LCD_BMPDisplay(selectbmp,127-17,0);
			len = snprintf(buf,sizeof(buf),"%s",Reflow_GetProfileName());
			LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);

			if(keyspressed & KEY_S) { // Select current profile
				mode=0;
			}
		} else if(mode==3) { // Bake
			LCD_FB_Clear();
			LCD_disp_str((uint8_t*)"MANUAL/BAKE MODE", 16, 0, 0, FONT6X6);

			if(keyspressed & KEY_F1) { // Setpoint-
				setpoint--;
				if(setpoint<30) setpoint = 30;
			}
			if(keyspressed & KEY_F2) { // Setpoint+
				setpoint++;
				if(setpoint>300) setpoint = 300;
			}

			len = snprintf(buf,sizeof(buf),"L=%03.1fC R=%03.0fC",temperature[0],temperature[1]);
			LCD_disp_str((uint8_t*)buf, len, 0, 6, FONT6X6);

			len = snprintf(buf,sizeof(buf),"AVG=%03.1fC",avgtemp);
			LCD_disp_str((uint8_t*)buf, len, 0, 12, FONT6X6);

			len = snprintf(buf,sizeof(buf),"SP=%03dC F1- F2+",setpoint);
			LCD_disp_str((uint8_t*)buf, len, 0, 18, FONT6X6);

			len = snprintf(buf,sizeof(buf),"CJ=%03.1fC",coldjunction);
			LCD_disp_str((uint8_t*)buf, len, 0, 24, FONT6X6);

			LCD_BMPDisplay(stopbmp,127-17,0);

			len = snprintf(buf,sizeof(buf),"heat=0x%02x fan=0x%02x",heat,fan);
			LCD_disp_str((uint8_t*)buf, len, 0, 63-5, FONT6X6);

			// Add timer for bake at some point

			Reflow_Run(0, avgtemp, &heat, &fan, setpoint);

			if(keyspressed & KEY_S) { // Abort bake
				mode=0;
			}
		} else if(mode==2 || mode==1) { // Edit ee1 or 2
			// TODO
			mode = 0;
		} else { // Main menu
			LCD_FB_Clear();
			heat=fan=0;

			len = snprintf(buf,sizeof(buf),"MAIN MENU");
			LCD_disp_str((uint8_t*)buf, len, 0, 6*0, FONT6X6);
			//LCD_disp_str((uint8_t*)"F1:EDIT CUSTOM 1", 16, 0, 6*2, FONT6X6);
			//LCD_disp_str((uint8_t*)"F2:EDIT CUSTOM 2", 16, 0, 6*3, FONT6X6);
			LCD_disp_str((uint8_t*)"F3:BAKE/MANUAL MODE", 19, 0, 6*4, FONT6X6);
			LCD_disp_str((uint8_t*)"F4:SELECT PROFILE", 17, 0, 6*5, FONT6X6);
			LCD_disp_str((uint8_t*)"S :RUN REFLOW PROFILE", 21, 0, 6*7, FONT6X6);
			len = snprintf(buf,sizeof(buf),"[%s]",Reflow_GetProfileName());
			LCD_disp_str((uint8_t*)buf, len, 0, 6*8, FONT6X6);
			len = snprintf(buf,sizeof(buf),"OVEN TEMP. %03.1fC",avgtemp);
			LCD_disp_str((uint8_t*)buf, len, 0, 64-6, FONT6X6);

			if(keyspressed & KEY_F1) { // Edit ee1
				mode=1;
			}
			if(keyspressed & KEY_F2) { // Edit ee2
				mode=2;
			}
			if(keyspressed & KEY_F3) { // Bake mode
				mode=3;
				Reflow_Init();
			}
			if(keyspressed & KEY_F4) { // Select profile
				mode=4;
			}
			if(keyspressed & KEY_S) { // Start reflow
				mode=5;
				LCD_FB_Clear();
				Reflow_Init();
				Reflow_PlotProfile();
				LCD_BMPDisplay(stopbmp,127-17,0);
				len = snprintf(buf,sizeof(buf),"%s",Reflow_GetProfileName());
				LCD_disp_str((uint8_t*)buf, len, 13, 0, FONT6X6);
			}
		}

		LCD_FB_Update();

		if(mode != 5 && mode != 3 && avgtemp > 40) fan=0x80; // Force cooldown if still warm

		Set_Heater(heat);
		Set_Fan(fan);
	}
	return 0;
}
