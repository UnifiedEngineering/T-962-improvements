/*
 * sc18is602b.c - I2C to SPI bridge handling for T-962 reflow controller
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

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
//#include "i2c.h"
#include "sc18is602b.h"

#define SCADDR (0x28<<1)
static uint8_t scaddr;
static I2C_HandleTypeDef* s_i2c;

int32_t SC18IS602B_Init( I2C_HandleTypeDef* i2c, SPIclk_t clk, SPImode_t mode, SPIorder_t order ) {
	uint8_t function[2];
	int32_t retval;
	printf("%s ",__FUNCTION__);
	s_i2c = i2c;
	for( uint8_t scan=0; scan<8; scan++ ) {
		function[0] = 0xf0;
		function[1] = clk | mode | order;
		//retval = I2C_Xfer(SCADDR | (scan<<1), function, sizeof(function), 1); // Attempt to initialize I2C to SPI bridge
		retval = HAL_I2C_Master_Transmit(s_i2c, SCADDR | (scan<<1), function, sizeof(function), HAL_MAX_DELAY); // Attempt to initialize I2C to SPI bridge
		if (retval==0) {
			scaddr = SCADDR | (scan<<1);
			break;
		}
	}
	if( retval == 0 ) {
		printf( "- Done (addr 0x%02x)\r\n", scaddr>>1);
	} else {
		printf( "- No chip found\r\n");
	}
	return retval;
}

int32_t SC18IS602B_SPI_Xfer( SPIxfer_t* item ) {
	int32_t retval;
	if( item->len > (sizeof(SPIxfer_t) - 2) ) {
		printf("%s: Invalid length!\r\n",__FUNCTION__);
		return -1;
	}
	//retval = I2C_Xfer(scaddr, (uint8_t*)item, item->len + 1, 1); // Initialize transfer, ssmask + data
	retval = HAL_I2C_Master_Transmit(s_i2c, scaddr, (uint8_t*)item, item->len + 1, HAL_MAX_DELAY); // Initialize transfer, ssmask + data
	if( retval == 0 ) {
		// TODO: There should be a timeout here
		do {
			//retval = I2C_Xfer(scaddr + 1, (uint8_t*)item->data, item->len, 1); // Initialize read transfer, data only
			retval = HAL_I2C_Master_Receive(s_i2c, scaddr, (uint8_t*)item->data, item->len, HAL_MAX_DELAY); // Initialize read transfer, data only
		} while( retval != 0 ); // Wait for chip to be done with transaction
	}
	return retval;
}
