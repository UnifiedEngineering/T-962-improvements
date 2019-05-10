/*
 * adc.c - AD converter interface for T-962 reflow controller
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
#include "log.h"
#include "adc.h"
#include "t962.h"
#include "sched.h"
#include "config.h"

// indices for channel 1 and 2 respectively
static uint8_t i=0;
static uint8_t j=0;
// hold intermediate sums of 16 ADC samples (with 10 bits -> 14bits needed)
static uint16_t sum1 = 0;
static uint16_t sum2 = 0;
// average values
static uint32_t avg1 = 0;
static uint32_t avg2 = 0;

// running average over 16 values (each a sum of 16 ADC samples)
#define N	16

// create a running average over 16 values of sums of 16 samples
// duplicate code, it's not worth the effort of indirection
static void average(uint32_t adc1, uint32_t adc2)
{
	// for ADC1
	if (adc1 & (1 << 31)) {
		adc1 = (adc1 >> 6) & 0x3ff;
		sum1 += adc1;
		if (i == 15) {
			avg1 = ((N - 1) * avg1 + sum1) / N;
			i = sum1 = 0;
		} else
			i++;
	}
	// for ADC2
	if (adc2 & (1 << 31)) {
		adc2 = (adc2 >> 6) & 0x3ff;
		sum2 += adc2;
		if (j == 15) {
			avg2 = ((N - 1) * avg2 + sum2) / N;
			j = sum2 = 0;
		} else
			j++;
	}
}

static int32_t ADC_Work(void) {
	average(AD0DR1, AD0DR2);
	// Run once per ms, at 256 fold oversampling leads to 1 value every 0.256s
	return TICKS_US(1000);
}

void ADC_Init( void ) {
	log(LOG_DEBUG, "%s called", __FUNCTION__);
	Sched_SetWorkfunc(ADC_WORK, ADC_Work);

	// 1MHz adc clock, enabling ch1 and 2
	AD0CR = (1 << 21) | (((uint8_t)(PCLKFREQ / 1000000)) << 8) | 0x06;
	AD0CR |= (1 << 16); // Burst
	AD0DR1;
	AD0DR2;
	Sched_SetState( ADC_WORK, 2, 0); // Start right away
}



/*
 * This is supposed to deliver values in 1/16 degree Celsius,
 *   which is about 1 LSB for a pre-amplifier gain of about 80.
 *   80 x 41.276uV/K = 3.3021mV/K --> 0.9772 LSB/K (for Vref=3.3V)
 * The board has a 3.3V, 1% regulator, so nothing better than 3 degC is
 * to be hoped for! Returning a resolution of 1/16 deg C is pretty
 * useless.
 * Using a precision OPamp is strongly recommended (using a gain
 *  of 222.2 --> 9.172mV/K -> 2.7141 LSB/K (for Vref=3.3V).
 */
int32_t ADC_Read(uint32_t chnum) {
#ifndef USE_PRECISION_OPAMP
	if (chnum == 1)
		return avg1;
	if (chnum == 2)
		return avg2;
#else
	// take care not to overflow an unsigned 32 bit value!
	// input is in 1/16th LSBs (up to 16*1024-1, or 14 bit)
	// factor is 0.36845 = 7369/20000
#define  TC_ADJUST(x) ((int32_t) ((x * 7369) / 20000))

	if (chnum == 1)
		return TC_ADJUST(avg1);
	if (chnum == 2)
		return TC_ADJUST(avg2);
#endif
	return -1;
}
