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

void ADC_Init( void ) {
	printf("\n%s called",__FUNCTION__);
	AD0CR = (1<<21) | (55<<8) | 0x06; // 1MHz adc clock, enabling ch1 and 2
	AD0CR |= (1<<16); // Burst
	AD0DR1;
	AD0DR2;
}

int32_t ADC_Read( uint32_t chnum ) {
	int32_t result=-1;
	uint32_t scratch=0;
	if(chnum==1) {
		scratch=AD0DR1;
	} else if(chnum==2) {
		scratch=AD0DR2;
	}
	if(scratch & (1<<31)) { // Done
		result=(scratch>>6)&0x3ff;
	}
	return result;
}

