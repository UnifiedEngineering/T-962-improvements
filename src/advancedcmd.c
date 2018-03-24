/*
* advancedcmd.c - Advanced Command Interface for T-962 reflow controller
*
* Author: Ryan Densberger - radensb
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
#include <stdio.h>
#include "circbuffer.h"
#include "serial.h"
#include "advancedcmd.h"

//chkAdvCmd - See if an Advanced Command had been sent. If a valid command is buffered,
//read the data into the struct and return 1.
uint8_t chkAdvCmd(tcirc_buf* buf, advancedSerialCMD* cmd) {
	//check for a full header worth of bytes
	if (circ_buf_count(buf) < ADVCMD_OVERHEAD) return 0;
	//See if it is an advanced command
	if (circ_buf_peek(buf, ADVCMD_SYNC1_LOC) == 0xFF && circ_buf_peek(buf, ADVCMD_SYNC2_LOC) == 0x55) {
		//Check the header CRC
		if (calcCkSum(buf, ADVCMD_OVERHEAD - 1) != circ_buf_peek(buf, ADVCMD_OVERHEAD - 1)) {
			//header CRC failed, corrupt size byte -> flush and abort
			circ_buf_flush(buf);
			return 0;
		}
		//verify we have enough bytes buffered to verify a CRC -> if not, abort and try again next time
		if (circ_buf_count(buf) < ((unsigned)circ_buf_peek(buf, ADVCMD_SIZE_LOC) + ADVCMD_OVERHEAD)) return 0;

		//there is a full, verifyable command in the ring - lets go!
		uart_readc(); uart_readc(); // flush the 0xFF55
		cmd->cmdSize = uart_readc();
		uart_readc(); // flush the valid header CRC
		//check the CRC for the data section
		if (calcCkSum(buf, cmd->cmdSize) != circ_buf_peek(buf, cmd->cmdSize)) {
			//CMD in ring failed CRC -> Flush and abort
			/*DEBUG*/
			//printf("Calculated CRC: %02X\n", calcCkSum(buf, cmd->cmdSize));
			//printf("Received CRC  : %02X\n", circ_buf_peek(buf, cmd->cmdSize));
			//for (int i = 0; i < cmd->cmdSize + 1; ++i) {
			//	if (i % 16 == 0) printf("\n");
			//	printf("%02X, ", circ_buf_peek(buf, i));
			//}
			//printf("Corrupt. Flushing...\n");
			circ_buf_flush(buf); //corrupt			
			return 0;
		} else {
			//CMD in ring passed CRC -> ACK, transfer data to cmd struct, flush, and report success
			printf("%c", ADVCMD_ACK);
			//get CMD
			cmd->cmd = uart_readc();
			//Get Data
			for (uint8_t i = 0; i < cmd->cmdSize; ++i) {
				cmd->data[i] = uart_readc();
			}
			circ_buf_flush(buf); //scrub clean
			return 1;
		}
	}
	return 0;
}

//Fast and easy checksum
uint8_t calcCkSum(tcirc_buf* buf, int length) {
	unsigned datSum = 0;
	for (uint8_t i = 0; i < length; ++i) {
		datSum += circ_buf_peek(buf, i);
	}
	return 0x100 - (datSum & 0xFF);
}