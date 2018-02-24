/*
 * scliTypes.h
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

#ifndef SCLI_TYPES_H_
#define SCLI_TYPES_H_

#include "SimpleCLI/cfg/scliConfig.h"
#include <stdint.h>
#include <stddef.h>

#define SCLI_CMD_LIST_END {0,0,0,0}
#define SCLI_FIFO_EMPTY (-1)

#if SCLI_CMD_HIST < 256
typedef uint8_t SCLI_BUF_IDX;
#else
typedef uint16_t SCLI_BUF_IDX;
#endif

#if SCLI_CMD_MAX_LEN < 256
typedef uint8_t SCLI_LINE_IDX;
#else
typedef uint16_t SCLI_LINE_IDX;
#endif

#if SCLI_GETCH_BUF_SIZE < 256
typedef uint8_t SCLI_FIFO_IDX;
#else
typedef uint16_t SCLI_FIFO_IDX;
#endif

typedef int32_t SCLI_CMD_RET;
typedef SCLI_CMD_RET (*SCLI_CMD_CB)(uint8_t argc, char **argv);

typedef enum
{
  SCLI_BUF_INIT = 0,
  SCLI_BUF_READY,
  SCLI_BUF_ESC_CLI,
  SCLI_BUF_ESC_CMD,
  SCLI_BUF_PARSING,
  SCLI_BUF_EXEC
}SCLI_BUF_STATE;

typedef struct
{
  SCLI_CMD_CB       CmdCallback;
  const char *const CmdName;
  const char *const HelpShort;
  const char       *HelpLong;
}SCLI_CMD_T;

typedef struct
{
  SCLI_BUF_STATE  State;
  char            Line[SCLI_CMD_MAX_LEN];
  SCLI_BUF_IDX    HistRefs;
  SCLI_LINE_IDX   LineIdx;
}SCLI_CMD_INPUT_T;

typedef struct
{
  void        (*SubsystemInit)(void (*)(uint8_t ));
  void        (*PutCh)(char ch);
  void        (*UserLevelHandOver)(SCLI_CMD_INPUT_T *Input);
  uint32_t    (*CriticalEnter)(void);
  void        (*CriticalExit)(uint32_t reg);
  SCLI_CMD_T  *UserLevelCommands;

}SCLI_INTERFACE_T;

typedef struct
{
  char            Buffer[SCLI_GETCH_BUF_SIZE];
  SCLI_FIFO_IDX   Write;
  SCLI_FIFO_IDX   Read;
  SCLI_FIFO_IDX   Size;
}SCLI_FIFO_T;

#if SCLI_USE_CFG_SYSTEM > 0

#define SCLI_CFG_VAR_SIZE_MASK  0x0F
#define SCLI_CFG_CMP_LIST_END   {.Str = 0, .Value.Unsigned = 0}
#define SCLI_CFG_ENTRY_LIST_END {0, SCLI_CFG_VAR_NONE, SCLI_CFG_PARSE_INT, 0, 0, 0, 0}

typedef enum
{
  SCLI_CFG_VAR_NONE   = 0x00,
  SCLI_CFG_VAR_INT8S  = 0x11,
  SCLI_CFG_VAR_INT8U  = 0x21,
  SCLI_CFG_VAR_INT16S = 0x32,
  SCLI_CFG_VAR_INT16U = 0x42,
  SCLI_CFG_VAR_INT32S = 0x54,
  SCLI_CFG_VAR_INT32U = 0x64,
  SCLI_CFG_VAR_STRING = 0xFF,
}SCLI_CFG_VAR_TYPE;

typedef enum
{
  SCLI_CFG_PARSE_INT = 0,
  SCLI_CFG_PARSE_STR_CMP,
  SCLI_CFG_PARSE_STR_PLAIN,
}SCLI_CFG_P_METHOD;

typedef struct
{
  const char *const Str;
  union
  {
    int32_t   Signed;
    uint32_t  Unsigned;
  }Value;
}SCLI_CFG_CMP_T;

typedef struct
{
  const char *const Name;
  SCLI_CFG_VAR_TYPE Type;
  SCLI_CFG_P_METHOD ParseMethod;
  SCLI_CFG_CMP_T   *StrCmpTable;
  const char *const Desc;
  size_t            Size;
  void             *Target;
}SCLI_CFG_ENTRY_T;

typedef struct _SCLI_CFG_HANDLE
{
  char *                    Name;
  char *                    Desc;
  SCLI_CFG_ENTRY_T         *Table;
  struct _SCLI_CFG_HANDLE  *Prev;
  struct _SCLI_CFG_HANDLE  *Next;
}SCLI_CFG_HANDLE_T;

typedef struct _SCLI_CFG_FS_ENTRY
{
  SCLI_CFG_VAR_TYPE           Type;
  struct _SCLI_CFG_FS_ENTRY  *Next;
  union
  {
    struct
    {
      uint32_t  Value;
      char      Name;
    }UnsignedNum;
    struct
    {
      int32_t   Value;
      char      Name;
    }SignedNum;
    char String;
  }Payload;
}SCLI_CFG_FS_ENTRY_T;

typedef struct _SCLI_CFG_RAW_FS
{
  uint32_t                  Hash;
  SCLI_CFG_FS_ENTRY_T      *Start;
  struct _SCLI_CFG_RAW_FS  *Prev;
  struct _SCLI_CFG_RAW_FS  *Next;
}SCLI_CFG_RAW_FS_T;
#endif

#endif /* SCLI_TYPES_H_ */
