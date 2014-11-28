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

#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include "t962.h"
#include "rtc.h"

#define RTCINTDIV ((PCLKFREQ / 32768)-1)
#define RTCFRACDIV (PCLKFREQ-((RTCINTDIV+1)*32768))

void RTC_Init(void) {
	PREINT = RTCINTDIV;
	PREFRAC = RTCFRACDIV;
	CCR = (1<<0); // Enable RTC
	RTC_Zero();
}

uint32_t RTC_Read(void) {
	uint32_t retval,tmp;
	tmp = CTIME0;
	retval = tmp & 0x3f; // Seconds
	tmp >>= 8;
	retval += ((tmp&0x3f)*60); // Minutes
	tmp >>= 8;
	retval += ((tmp&0x1f)*3600); // Hours
	return retval;
}

void RTC_Zero(void) {
	CCR |= (1<<1);
	SEC = MIN = HOUR = 0; // Maybe need day for bake as well?
	CCR &= ~(1<<1);
}
