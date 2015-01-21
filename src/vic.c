/*
 * vic.c - Vectored Interrupt Controller interface for T-962 reflow controller
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
#include "t962.h"
#include "vic.h"

// Added functionality missing in the standard LPC header
#define VICVectAddrX(x) (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x100 + (x << 2)))
#define VICVectCntlX(x) (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x200 + (x << 2)))

static void __attribute__ ((interrupt ("IRQ"))) VIC_Default_Handler( void ) {
	static uint32_t spuriousirq = 0;
	spuriousirq++;
	VICVectAddr = 0; // ACK irq
}

void VIC_Init( void ) {
	VICIntEnable = 0; // Make sure all interrupts are disabled
	VICIntSelect = 0; // All interrupts are routed to IRQ (not FIQ)
	VICDefVectAddr = (uint32_t)VIC_Default_Handler;
}

uint32_t VIC_IsIRQDisabled( void ) {
	uint32_t state;
	asm("MRS %0,cpsr" : "=r" (state));
	return !!(state&0x80);
}

uint32_t VIC_DisableIRQ( void ) {
	uint32_t retval;
	asm("MRS %0,cpsr" : "=r" (retval));
	asm("MSR cpsr_c,#(0x1F | 0x80 | 0x40)");
	//FIO0CLR = (1<<11); // Visualize interrupts being disabled by turning backlight off
	return retval;
}

void VIC_RestoreIRQ( uint32_t mask ) {
	//if(!(mask & 0x80)) FIO0SET = (1<<11); // Visualize when interrupts are enabled again by turning backlight on
	asm("MSR cpsr_c,%0" : : "r" (mask));
}

int32_t VIC_RegisterHandler( VICInt_t num, void* ptr ) {
	int32_t i, retval;
	for( i = 0; i < 16 ; i++ ) { // Find empty vector slot
		if( !( VICVectCntlX( i ) & (1<<5) ) ) break;
	}
	if( i < 16 ) { // Found empty slot, insert handler
		VICVectAddrX( i ) = (uint32_t)ptr;
		VICVectCntlX( i ) = num | (1<<5); // Enable SLOT, not necessarily the interrupt itself
		retval = 0;
	} else {
		retval = -1; // No space
	}
	return retval;
}

int32_t VIC_EnableHandler( VICInt_t num ) {
	VICIntEnable |= (1<<num);
	return 0;
}

int32_t VIC_DisableHandler( VICInt_t num ) {
	VICIntEnClr = (1<<num);
	return 0;
}
