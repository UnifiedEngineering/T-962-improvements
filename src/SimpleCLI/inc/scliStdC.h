/*
 * scliStdC.h
 *
 *  Created on: 31.01.2016
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

#ifndef SCLISTDC_H_
#define SCLISTDC_H_

#include "SimpleCLI/cfg/scliConfig.h"
#include "SimpleCLI/inc/scliTypes.h"

#if SCLI_USE_OWN_STDC_FUNC > 0
extern uint32_t scliStdC_strlen(char *);
extern int      scliStdC_strncmp(char *, char *, size_t);
extern void     scliStdC_memset(void*, int8_t, size_t);
extern char *   scliStdC_strncpy(char *, const char *, size_t );
extern uint32_t scliStdC_strtoul(char *, char **, int );
extern int32_t  scliStdC_strtol(char *, char **, int );

#define strlen  scliStdC_strlen
#define strncmp scliStdC_strncmp
#define memset  scliStdC_memset
#define strncpy scliStdC_strncpy
#define strtoul scliStdC_strtoul
#define strtol  scliStdC_strtol

#else
#include <string.h>
#include <stdlib.h>
#endif

#endif /* SCLISTDC_H_ */
