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

#ifdef __NEWLIB__
#define __sys_write _write
#endif

#define CIRCBUFSIZE 128

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
	add_to_circ_buf(&txbuf, thebyte, 0);
}

//Blindly read character, assuming we knew one was available
char uart_readc(void) {
	return get_from_circ_buf(&rxbuf);
}

int uart_isrxready(void){
	circ_buf_has_char(&rxbuf);
}

// Override __sys_write so we actually can do printf
int __sys_write(int hnd, char *buf, int len) {
	int foo = len;
	while(foo--) uart_putc(*buf++);
	return (len);
}

/* Settings for 2Mb/s */
#ifdef SERIAL_2MBs
#define BAUD_M  11
#define BAUD_D  8
#define BAUD_DL 1

#else

/* Settings for 115kbps */
#define BAUD_M  1
#define BAUD_D  0
#define BAUD_DL 30

#endif

/* Settings for 1.2288Mb/s (supported by PL2303TA) */


//Baud = 55.296 / ( 16 x (BAUD_DL) x (1 + (BAUD_D / BAUD_M) ) )

void Serial_Init(void) {
	// Pin select config already done in IO init

	//Setup buffers
	init_circ_buf(&rxbuf);
	init_circ_buf(&txbuf);

	// This setup will result in 2Mbps with a 55.296MHz system clock
	U0FCR = 7; // Enable and reset FIFOs
	U0FDR = ((BAUD_M)<<4) | ((BAUD_D)<<0); // M=11, D=8
	U0LCR = 0x83; // 8N1 + enable divisor loading
	U0DLL = BAUD_DL; // Minimum allowed when running fractional brg is 3 according to UM10120 but this works just fine!
	U0DLM = 0;
	U0LCR &= ~0x80; // Divisor load done
#ifdef __NEWLIB__
	setbuf(stdout, NULL); // Needed to get rid of default line-buffering in newlib not present in redlib
#endif
}

/* This could be done via interrupts instead, but done via polling to avoid adding interrupts to the code which
 * previous didn't use them. */
void Uart_Work(void){

	//Ohhh, a character for us
	if (U0LSR & 0x01){
		add_to_circ_buf(&rxbuf, U0RBR, 0);
	}

	//Oohh, a character to transmit
	if (circ_buf_has_char(&txbuf)){
		//Hot damn, we've got space
		if (U0LSR & (1<<5)) {
			U0THR = get_from_circ_buf(&txbuf);
		}
	}
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

        //Add processing here?
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
