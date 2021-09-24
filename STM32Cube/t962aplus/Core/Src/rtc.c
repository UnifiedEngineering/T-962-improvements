/*
 * rtc.c - RTC interface for T-962 reflow controller
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


#include "main.h"
//#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
//#include "t962.h"
#include "rtc.h"

//#define RTCINTDIV ((PCLKFREQ / 32768)-1)
//#define RTCFRACDIV (PCLKFREQ-((RTCINTDIV+1)*32768))
static RTC_HandleTypeDef* s_rtc;

void RTC_Init(RTC_HandleTypeDef* rtc) {
	s_rtc = rtc;
#if 0
	PREINT = RTCINTDIV;
	PREFRAC = RTCFRACDIV;
	CCR = (1<<0); // Enable RTC
#endif
	RTC_Zero();
}

uint32_t RTC_Read(void) {
	uint32_t retval,tmp;
//	tmp = CTIME0;
	uint32_t high1 = READ_REG(s_rtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
	uint32_t low1  = READ_REG(s_rtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
	uint32_t high2 = READ_REG(s_rtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
	uint32_t low2  = READ_REG(s_rtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
	if (high1 != high2) {
		tmp = (high2 << 16) | low2;
	} else {
		tmp = (high1 << 16) | low1;
	}
	retval = tmp;
	return retval;
}

void RTC_Zero(void) {
	RTC_TimeTypeDef tmp;
	tmp.Hours = tmp.Minutes = tmp.Seconds = 0;
	HAL_RTC_SetTime(s_rtc, &tmp, RTC_FORMAT_BIN);
//	CCR |= (1<<1);
//	SEC = MIN = HOUR = 0; // Maybe need day for bake as well?
//	CCR &= ~(1<<1);
}
