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
#include <stdio.h>
#include "adc.h"
#include "t962.h"
#include "sched.h"

#define NUM_ADCH (2)
#define OVERSAMPLING_BITS (4) // This is n (how many additional bits to supply)
#define NUM_ACCUM (256) // This needs to be 4 ^ n
#define ACCUM_MASK (NUM_ACCUM-1)

static uint32_t ADres[NUM_ADCH];
static uint16_t ADidx[NUM_ADCH];
static uint16_t ADaccum[NUM_ADCH][NUM_ACCUM];

static void ADC_Accum(uint32_t chnum, uint32_t value) {
	uint16_t temp;
	chnum--;
	uint8_t idx = ADidx[chnum];

	// Done
	if (value & (1 << 31)) {
		temp = (value >> 6) & 0x3ff;
		ADres[chnum] -= ADaccum[chnum][idx];
		ADaccum[chnum][idx] = temp;
		ADres[chnum] += temp;
		idx++;
		idx &= ACCUM_MASK;
		ADidx[chnum] = idx;
	}
}

static int32_t ADC_Work(void) {
	ADC_Accum( 1, AD0DR1);
	ADC_Accum( 2, AD0DR2);
	return TICKS_US( 100 ); // Run 10000 times per second
}

void ADC_Init( void ) {
	printf("\n%s called", __FUNCTION__);
	Sched_SetWorkfunc(ADC_WORK, ADC_Work);

	// 1MHz adc clock, enabling ch1 and 2
	AD0CR = (1 << 21) | (((uint8_t)(PCLKFREQ / 1000000)) << 8) | 0x06;
	AD0CR |= (1 << 16); // Burst
	AD0DR1;
	AD0DR2;
	for (int i = 0; i < NUM_ADCH; i++) {
		ADres[i] = ADidx[i] = 0;
		for (int j = 0; j < NUM_ACCUM; j++) {
			ADaccum[i][j] = 0;
		}
	}
	Sched_SetState( ADC_WORK, 2, 0); // Start right away
}

int32_t ADC_Read(uint32_t chnum) {
	int32_t result=-1;
	if (chnum >= 1 && chnum <= 2) {
		result = ADres[chnum - 1] >> OVERSAMPLING_BITS;
	}
	return result;
}
