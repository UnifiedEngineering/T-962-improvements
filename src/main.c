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

int main(void) {
	char buf[20];

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
		int len;
		float coldjunction;
		uint32_t keyspressed=Keypad_Poll();
		if(keyspressed) printf("\nKeypad %02x ",keyspressed);
		//len = snprintf(buf,sizeof(buf),"timer:<%010d>",Timer_Get());
		//LCD_disp_str((uint8_t*)buf, len, 0, 63-5, FONT6X6);
		//LCD_FB_Update();

		// Sort this out, must support alternate sensors
		coldjunction=OneWire_GetTemperature();
		temp[0]=ADC_Read(1);
		if(temp[0]>=0) temp[1]=temp[0];
		temp[0]=ADC_Read(2);
		if(temp[0]>=0) temp[2]=temp[0];
		printf("\nADC readout 0x%03x 0x%03x ",temp[1],temp[2]);
		temperature[0] = (float)temp[1];
		temperature[1] = (float)temp[2];
		// Change gain here
		temperature[0] += coldjunction -6.0f; // Offset adjust
		temperature[1] += coldjunction -2.0f;

		float avgtemp=(temperature[0]+temperature[1])/2;
		printf("L=%03.1fC R=%03.1fC AVG=%03.1fC SP=%03dC CJ=%03.1fC Heat=0x%02x Fan=0x%02x",
				temperature[0],temperature[1],avgtemp,setpoint,coldjunction,heat,fan);

		// Sort out this "state machine"
		if(mode==1) {
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
		} else {
			LCD_FB_Clear();
			heat=fan=0;

			if(avgtemp>(coldjunction+20)) fan=0x80; // Force cooldown if still warm

			len = snprintf(buf,sizeof(buf),"L=%03.1fC R=%03.1fC",temperature[0],temperature[1]);
			LCD_disp_str((uint8_t*)buf, len, 0, 0, FONT6X6);

			len = snprintf(buf,sizeof(buf),"AVG=%03.1fC",avgtemp);
			LCD_disp_str((uint8_t*)buf, len, 0, 6, FONT6X6);

			len = snprintf(buf,sizeof(buf),"SP=%03dC",setpoint);
			LCD_disp_str((uint8_t*)buf, len, 0, 12, FONT6X6);

			len = snprintf(buf,sizeof(buf),"CJ=%03.1fC",coldjunction);
			LCD_disp_str((uint8_t*)buf, len, 70, 52, FONT6X6);

			len = snprintf(buf,sizeof(buf),"heat=0x%02x fan=0x%02x",heat,fan);
			LCD_disp_str((uint8_t*)buf, len, 0, 63-5, FONT6X6);
			if(keyspressed & KEY_F1) { // Start reflow
				mode=1;
				LCD_FB_Clear();
				Reflow_Init();
				Reflow_PlotProfile();
			}
		}

		LCD_FB_Update();

		Set_Heater(heat);
		Set_Fan(fan);
	}
	return 0;
}
