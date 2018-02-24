/*
 * scliCore.h
 *
 *  Created on: 27.01.2016
 *      Author: Matthias Beckert <beckert.matthias@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCLI_CORE_H_
#define SCLI_CORE_H_

#include "SimpleCLI/cfg/scliConfig.h"
#include "SimpleCLI/inc/scliTypes.h"
#include "SimpleCLI/inc/scliText.h"
#include "SimpleCLI/inc/scliConf.h"

#if SCLI_USE_OWN_PRINTF == 1
#include <printf.h>
#else
#include <stdio.h>
#endif

extern uint8_t      scliCore_Init(SCLI_INTERFACE_T* Interface);
extern void         scliCore_ExecuteCommand(SCLI_CMD_INPUT_T *InputBuffer);
extern void         scliCore_ReleaseBuffer(SCLI_CMD_INPUT_T *InputBuffer);
extern int          scliCore_getchar(void);
extern SCLI_CMD_T * scliCore_GetCommandInTable(char * Cmd, SCLI_CMD_T * Table, SCLI_LINE_IDX CmdLen);
extern void         scliCore_PrintHelp(SCLI_CMD_T * Table, uint8_t SingleEntry);



#endif /* SCLI_CORE_H_ */
