/*
 * scliConf.h
 *
 *  Created on: 12.02.2016
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

#ifndef SCLICONF_H_
#define SCLICONF_H_

#include "SimpleCLI/inc/scliTypes.h"

#if SCLI_USE_CFG_SYSTEM > 0

extern SCLI_CMD_RET         scliConf_SetCmd(uint8_t argc, char **argv);
extern SCLI_CMD_RET         scliConf_GetCmd(uint8_t argc, char **argv);
extern SCLI_CMD_RET         scliConf_ConfCmd(uint8_t argc, char **argv);
extern SCLI_CFG_HANDLE_T *  scliConf_RegisterConfig(const char *const Name, const char *const Desc, SCLI_CFG_ENTRY_T *Table);
#endif

#endif /* SCLICONF_H_ */
