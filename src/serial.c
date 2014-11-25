/*
 * serial.c - UART handling for T-962 reflow controller
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

#include "lpc214x.h"
#include <stdint.h>
#include "serial.h"

static void uart_putc(char thebyte) {
	if (thebyte == '\n')
		uart_putc('\r');
	while(!(U0LSR & (1<<5)));
	U0THR = thebyte;
}

// Override __sys_write so we actually can do printf
int __sys_write(int hnd, char *buf, int len) {
	int foo = len;
	while(foo--) uart_putc(*buf++);
	return (len);
}

void Serial_Init(void) {
	// Pin select config already done in IO init

	// This setup will result in 2Mbps with a 55.296MHz system clock
	U0FCR = 7; // Enable and reset FIFOs
	U0FDR = (11<<4) | (8<<0); // M=11, D=8
	U0LCR = 0x83; // 8N1 + enable divisor loading
	U0DLL = 1; // Minimum allowed when running fractional brg is 3 according to UM10120 but this works just fine!
	U0DLM = 0;
	U0LCR &= ~0x80; // Divisor load done
}
