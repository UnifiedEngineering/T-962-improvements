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
#include "serial.h"
#include "vic.h"

#ifdef __NEWLIB__
#define __sys_write _write
#endif

/* Size of buffer. The putchar function used here will block when the buffer is full (buffer is drained in interrupt),
 * so if you want to continously write lots of data increase the buffer size. Change putchar() to non-blocking and read
 * the 'dropped' parameter in the buffer structure to determine how your buffer size is doing. */
#define CIRCBUFSIZE 256

/* The following baud rates assume a 55.296MHz system clock */
#ifdef SERIAL_2MBs
/* Settings for 2Mb/s */
#define BAUD_M  11
#define BAUD_D  8
#define BAUD_DL 1 /* Minimum allowed when running fractional brg is 3 according to UM10120 but this works just fine! */
#else
/* Settings for 115kbps */
#define BAUD_M  1
#define BAUD_D  0
#define BAUD_DL 30
#endif

typedef struct {
    volatile unsigned int head;
    volatile unsigned int tail;
    volatile unsigned int dropped;
    char buf[CIRCBUFSIZE];
} tcirc_buf;

/* Buffer functions used internally */
static void init_circ_buf(tcirc_buf * cbuf);
static void add_to_circ_buf(tcirc_buf *cbuf, char ch, int block);
static int circ_buf_has_char(tcirc_buf *cbuf);
static char get_from_circ_buf(tcirc_buf *cbuf);
static unsigned int circ_buf_count(tcirc_buf *cbuf);

/* UART Buffers */
tcirc_buf txbuf;
tcirc_buf rxbuf;


static void uart_putc(char thebyte) {
	if (thebyte == '\n')
		uart_putc('\r');

	//The following is done blocking. This means when you call printf() with lots of data,
	//it relies on the ability of the interrupt to drain the txbuf, otherwise the system
	//will lock up.
	if (!VIC_IsIRQDisabled()){
		add_to_circ_buf(&txbuf, thebyte, 1);
	} else {
		add_to_circ_buf(&txbuf, thebyte, 0);
	}

	//If interrupt is disabled, we need to start the process and enable the interrupt
	if((U0IER & (1<<1)) == 0){
		U0THR = get_from_circ_buf(&txbuf);
		U0IER |= 1<<1;
	}
}

//Blindly read character, assuming we knew one was available
char uart_readc(void) {
	return get_from_circ_buf(&rxbuf);
}

int uart_isrxready(void){
	return circ_buf_has_char(&rxbuf);
}

// Override __sys_write so we actually can do printf
int __sys_write(int hnd, char *buf, int len) {
	int foo = len;
	while(foo--) uart_putc(*buf++);
	return (len);
}

static void __attribute__ ((interrupt ("IRQ"))) Serial_IRQHandler( void ) {
	// Figure out which interrupt that fired within the peripheral, and ACK it
	uint32_t intsrc = (U0IIR & 0b0001110);

	// THRE Interrupt
	if (intsrc == 0b00000010) {
		//Check if data to TX still
		if (circ_buf_has_char(&txbuf)){
			//Still data to transmit - write to THR
			U0THR = get_from_circ_buf(&txbuf);
		} else {
			//No more data - disable future THRE interrupts
			U0IER &= ~(1<<1);
		}
	}

	// RDA Interrupt
	if (intsrc == 0b00000100) {
		//Don't block, as we are inside an interrupt!
		add_to_circ_buf(&rxbuf, U0RBR, 0);
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

	// Enable RX interrupt
	U0IER |= 1<<0;
}


/**** Circular Buffer used by UART ****/

static void init_circ_buf(tcirc_buf *cbuf)
{
    cbuf->head = cbuf->tail = 0;
    cbuf->dropped = 0;
}

static void add_to_circ_buf(tcirc_buf *cbuf, char ch, int block)
{
    // Add char to buffer
    unsigned int newhead = cbuf->head;
    newhead++;
    if (newhead >= CIRCBUFSIZE)
        newhead = 0;
    while (newhead == cbuf->tail)
    {
        if (!block)
        {
            cbuf->dropped++;
            return;
        }

        //If blocking, this just keeps looping. Due to interrupt-driven
        //system the buffer might eventually have space in it, however
        //if this is called when interrupts are disabled it will stall
        //the system, so the caller is cautioned not to fsck it up.
    }

    cbuf->buf[cbuf->head] = ch;
    cbuf->head = newhead;
}


static char get_from_circ_buf(tcirc_buf *cbuf)
{
    // Get char from buffer
    // Be sure to check first that there is a char in buffer
    unsigned int newtail = cbuf->tail;
    uint8_t retval = cbuf->buf[newtail];

    if (newtail == cbuf->head)
        return 0xFF;

    newtail++;
    if (newtail >= CIRCBUFSIZE)
        // Rollover
        newtail = 0;
    cbuf->tail = newtail;

    return retval;
}


static int circ_buf_has_char(tcirc_buf *cbuf)
{
    // Return true if buffer empty
    unsigned int head = cbuf->head;
    return (head != cbuf->tail);
}

static unsigned int circ_buf_count(tcirc_buf *cbuf)
{
    int count;

    count = cbuf->head;
    count -= cbuf->tail;
    if (count < 0)
        count += CIRCBUFSIZE;
    return (unsigned int)count;
}

