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

#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>

#include "t962.h"
#include "vic.h"
#include "circbuffer.h"
#include "serial.h"
#include "config.h"

#ifdef __NEWLIB__
#define __sys_write _write
#endif

/* The following baud rates assume a 55.296MHz system clock */
#if UART0_BAUDRATE == 2000842
/* Settings for 2Mb/s */
#define BAUD_M  11
#define BAUD_D  8
/* Minimum allowed when running fractional brg is 3 according to UM10120 but
 * this works just fine! */
#define BAUD_DL 1
#else
/* calculate from 11.0592MHz * 5 / 16 / 30 / baudrate_divisor * 1 */
#define BAUD_M  1
#define BAUD_D  0
/* in multiples of 115200 */
#define BAUD_DL (30 * (115200 / UART0_BAUDRATE))
#endif

/* UART Buffers */
static tcirc_buf txbuf;
static tcirc_buf rxbuf;

static void uart_putc(char thebyte) {
	if (thebyte == '\n')
		uart_putc('\r');

	/* The following is done blocking. This means when you call printf() with lots of data,
	 * it relies on the ability of the interrupt to drain the txbuf, otherwise the system
	 * will lock up.
	 */
	add_to_circ_buf(&txbuf, thebyte, !VIC_IsIRQDisabled());

	// If interrupt is disabled, we need to start the process and enable the interrupt
	if ((U0IER & (1<<1)) == 0) {
		U0THR = get_from_circ_buf(&txbuf);
		uint32_t vs = VIC_DisableIRQ();
			U0IER |= 1<<1;
		VIC_RestoreIRQ(vs);
	}
}

// Blindly read character, assuming we knew one was available
char uart_readc(void) {
	char c = get_from_circ_buf(&rxbuf);
	// enable interrupts, there should be some space left now!
	if ((U0IER & (1<<0)) == 0) {
		uint32_t vs = VIC_DisableIRQ();
			U0IER |= 1<<0;
		VIC_RestoreIRQ(vs);
	}
	return c;
}

int uart_isrxready(void) {
	return circ_buf_has_char(&rxbuf);
}

// Override __sys_write so we actually can do printf
int __sys_write(int hnd, char *buf, int len) {
	int foo = len;
	UNUSED(hnd);
	while(foo--) uart_putc(*buf++);

	return len;
}

/*
 * TODO: this implementation is pretty incomplete, no error handling
 *  at all! So simply copy to and from the ring buffers. This will
 *  loose data in the hardware FIFOs if overrun.
 *  Note: Don't use U0IIR here as priorities may prevent transmitter action!
 */
static void __attribute__ ((interrupt ("IRQ"))) Serial_IRQHandler( void ) {

	// RDA Interrupt, copy to ring buffer, but only what fits
	while (U0LSR & (1 << 0)) {
		if (circ_buf_count(&rxbuf) == CIRCBUFSIZE) {
			// ring buffer full, stop interrupts
			U0IER &= ~(1<<0);
			break;
		} else {
			// Don't block, as we are inside an interrupt!
			add_to_circ_buf(&rxbuf, U0RBR, 0);
		}
	}

	// THRE interrupt, store characters to tx FIFO until no more space left
	while (U0LSR & (1 << 5)) {
		// still data to transmit - write to THR
		if (circ_buf_has_char(&txbuf)) {
			U0THR = get_from_circ_buf(&txbuf);
		} else {
			//No more data - disable future THRE interrupts
			U0IER &= ~(1<<1);
			break;
		}
	}

	// ACK IRQ with VIC as the last thing
	VICVectAddr = 0;
}

void Serial_Init(void) {
	// Pin select config already done in IO init

	// Setup buffers
	init_circ_buf(&txbuf);
	init_circ_buf(&rxbuf);

	// Baud rate defined above
	U0FCR = 7; // Enable and reset FIFOs
	U0FDR = ((BAUD_M)<<4) | ((BAUD_D)<<0);
	U0LCR = 0x83; // 8N1 + enable divisor loading
	U0DLL = BAUD_DL & 0xff;
	U0DLM = BAUD_DL >> 8;
	U0LCR &= ~0x80; // Divisor load done
#ifdef __NEWLIB__
	setbuf(stdout, NULL); // Needed to get rid of default line-buffering in newlib not present in redlib
#endif

	VIC_RegisterHandler( VIC_UART0, Serial_IRQHandler );
	VIC_EnableHandler( VIC_UART0 );

	// Only enable RX interrupts
	U0IER = 1<<0;
}
