/*
 * scliConf.c
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

#include "SimpleCLI/inc/scliCore.h"
#include "SimpleCLI/inc/scliStdC.h"

#if SCLI_USE_CFG_SYSTEM > 0

static SCLI_CFG_HANDLE_T    _scliConf_HandleTable[SCLI_CONF_MAX_HANDLE];
static SCLI_CFG_HANDLE_T *  _scliConf_HandleHead = (SCLI_CFG_HANDLE_T *)0;

/*
 * Forward declaration for internal commands
 */
static SCLI_CMD_RET _scliConf_ConfStore(uint8_t argc, char **argv);
static SCLI_CMD_RET _scliCore_ConfLoad(uint8_t argc, char **argv);
static SCLI_CMD_RET _scliConf_ConfShow(uint8_t argc, char **argv);

/*
 * Internal subcommand table
 */
static SCLI_CMD_T _scliConf_ConfSubcommands[] =
{
    {_scliConf_ConfStore, "store", "Save configuration",        0},
    {_scliCore_ConfLoad,  "load",  "Load configuration",        0},
    {_scliConf_ConfShow,  "show",  "Display system variables",  0},
    SCLI_CMD_LIST_END
};

static SCLI_CFG_HANDLE_T * _scliConf_GetFreeConfigSlot(void)
{
  unsigned int i = 0;
  for (i = 0; i < SCLI_CONF_MAX_HANDLE; i++)
  {
    if(    _scliConf_HandleTable[i].Next  == (SCLI_CFG_HANDLE_T *)0
        && _scliConf_HandleTable[i].Prev  == (SCLI_CFG_HANDLE_T *)0
        && _scliConf_HandleTable[i].Table == (SCLI_CFG_ENTRY_T *)0)
    {
      return &_scliConf_HandleTable[i];
    }
  }
  return (SCLI_CFG_HANDLE_T *)0;
}

static SCLI_CFG_ENTRY_T * _scliConf_LookupEntry(char *VarName)
{
  SCLI_CFG_HANDLE_T *CurrentHandle = _scliConf_HandleHead;
  SCLI_CFG_ENTRY_T *Entry = (SCLI_CFG_ENTRY_T *)0;
  while(CurrentHandle)
  {
    Entry = CurrentHandle->Table;

    while(Entry->Type != SCLI_CFG_VAR_NONE)
    {
      size_t EntryNameLen = strlen((char *)Entry->Name);
      size_t VarNameLen   = strlen(VarName);

      if(EntryNameLen == VarNameLen && strncmp(VarName,(char *)Entry->Name,EntryNameLen) == 0)
        return Entry;

      Entry++;
    }

    CurrentHandle = CurrentHandle->Next;
  }

  return (SCLI_CFG_ENTRY_T *)0;
}

static SCLI_CFG_HANDLE_T * _scliConf_LookupHandle(char *ConfName)
{
  SCLI_CFG_HANDLE_T *CurrentHandle = _scliConf_HandleHead;
  while(CurrentHandle)
  {
      size_t HandleNameLen = strlen((char *)CurrentHandle->Name);
      size_t ConfNameLen   = strlen(ConfName);

        if(HandleNameLen == ConfNameLen && strncmp(ConfName,(char *)CurrentHandle->Name,HandleNameLen) == 0)
          return CurrentHandle;

    CurrentHandle = CurrentHandle->Next;
  }

  return (SCLI_CFG_HANDLE_T *)0;
}

static SCLI_CMD_RET _scliConf_ParseInt(SCLI_CFG_ENTRY_T * Entry , char * String)
{

  char  *CheckPointer;
  uint32_t Unsigned = 0;
  int32_t  Signed = 0;

  switch(Entry->Type)
  {
  case SCLI_CFG_VAR_INT8S:
  case SCLI_CFG_VAR_INT16S:
  case SCLI_CFG_VAR_INT32S:
    Signed = strtol(String, &CheckPointer, 10);
    break;
  case SCLI_CFG_VAR_INT8U:
  case SCLI_CFG_VAR_INT16U:
  case SCLI_CFG_VAR_INT32U:
    Unsigned = strtoul(String, &CheckPointer, 10);
    break;
  default:
    return -1;
  }

  if(String == CheckPointer)
  {
    printf(" Invalid value\r\n");
    return -1;
  }

  switch(Entry->Type)
  {
  case SCLI_CFG_VAR_INT8S:  *((int8_t *)Entry->Target) = (int8_t)(Signed & 0x00FF); break;
  case SCLI_CFG_VAR_INT16S: *((int16_t *)Entry->Target) = (int16_t)(Signed & 0xFFFF); break;
  case SCLI_CFG_VAR_INT32S: *((int32_t *)Entry->Target) = Signed; break;

  case SCLI_CFG_VAR_INT8U:  *((uint8_t *)Entry->Target) = (uint8_t)(Unsigned & 0x00FF); break;
  case SCLI_CFG_VAR_INT16U: *((uint16_t *)Entry->Target) = (uint16_t)(Unsigned & 0xFFFF); break;
  case SCLI_CFG_VAR_INT32U: *((uint32_t *)Entry->Target) = Unsigned; break;

  default:
    return -1;
  }
  return 0;
}

static SCLI_CMD_RET _scliConf_ParseStrTbl(SCLI_CFG_ENTRY_T * Entry, char * String)
{
  SCLI_CFG_CMP_T *StrCmpTable = Entry->StrCmpTable;

  if(StrCmpTable == (SCLI_CFG_CMP_T *)0)
  {
    printf(" Missing string table!\r\n");
    return -1;
  }

  while(StrCmpTable->Str != 0)
  {
    size_t StrValueLen = strlen((char *)StrCmpTable->Str);
    size_t VarValueLen   = strlen(String);

      if(StrValueLen == VarValueLen && strncmp(String,(char *)StrCmpTable->Str,StrValueLen) == 0)
      {
        switch(Entry->Type)
        {
        case SCLI_CFG_VAR_INT8S:  *((int8_t *)Entry->Target) = (int8_t)(StrCmpTable->Value.Signed & 0x00FF); break;
        case SCLI_CFG_VAR_INT16S: *((int16_t *)Entry->Target) = (int16_t)(StrCmpTable->Value.Signed & 0xFFFF); break;
        case SCLI_CFG_VAR_INT32S: *((int32_t *)Entry->Target) = StrCmpTable->Value.Signed; break;

        case SCLI_CFG_VAR_INT8U:  *((uint8_t *)Entry->Target) = (uint8_t)(StrCmpTable->Value.Unsigned & 0x00FF); break;
        case SCLI_CFG_VAR_INT16U: *((uint16_t *)Entry->Target) = (uint16_t)(StrCmpTable->Value.Unsigned & 0xFFFF); break;
        case SCLI_CFG_VAR_INT32U: *((uint32_t *)Entry->Target) = StrCmpTable->Value.Unsigned; break;

        default:
          return -1;
        }
        return 0;
      }
    StrCmpTable++;
  }

  printf("\r\n Value %s for variable %s not found!\r\n"
         " Possible values are [", String, Entry->Name);

  StrCmpTable = Entry->StrCmpTable;
  while(StrCmpTable->Str != 0)
  {
    printf(" %s", StrCmpTable->Str);
    StrCmpTable++;
  }
  printf(" ]\r\n");

  return -1;
}

static SCLI_CMD_RET _scliConf_ParseStr(SCLI_CFG_ENTRY_T * Entry, char * String)
{
  strncpy((char *)Entry->Target, (const char *)String,Entry->Size - 1);
  ((char *)Entry->Target)[Entry->Size - 1] = '\0';
  return 0;
}

static SCLI_CMD_RET _scliConf_DisplayInt(SCLI_CFG_ENTRY_T * Entry, unsigned int Verbose)
{
  char Buffer[11];

  switch(Entry->Type)
  {
  case SCLI_CFG_VAR_INT8S:  sprintf(Buffer, "%d",(int32_t)*((int8_t *)(Entry->Target)));  break;
  case SCLI_CFG_VAR_INT16S: sprintf(Buffer, "%d",(int32_t)*((int16_t *)(Entry->Target))); break;
  case SCLI_CFG_VAR_INT32S: sprintf(Buffer, "%d",(int32_t)*((int32_t *)(Entry->Target))); break;

  case SCLI_CFG_VAR_INT8U:  sprintf(Buffer, "%u",(uint32_t)*((uint8_t *)(Entry->Target)));  break;
  case SCLI_CFG_VAR_INT16U: sprintf(Buffer, "%u",(uint32_t)*((uint16_t *)(Entry->Target))); break;
  case SCLI_CFG_VAR_INT32U: sprintf(Buffer, "%u",(uint32_t)*((uint32_t *)(Entry->Target))); break;

  default:
    return -1;
  }
  if(Verbose)
  {
    printf(" %s %s %s \r\n", (char *)Entry->Name, Buffer, (char *)Entry->Desc);
  }
  else
  {
    printf("\r\n get %s -> %s\r\n",(char *)Entry->Name, Buffer);
  }
  return 0;
}

static SCLI_CMD_RET _scliConf_DisplayStrTbl(SCLI_CFG_ENTRY_T * Entry, unsigned int Verbose)
{
  SCLI_CFG_CMP_T *StrCmpTable = Entry->StrCmpTable;
  SCLI_CFG_CMP_T CmpHelper;
  while(StrCmpTable->Str != 0)
  {
    switch(Entry->Type)
    {
    case SCLI_CFG_VAR_INT8S:  CmpHelper.Value.Signed = (int32_t) *((int8_t *)Entry->Target); break;
    case SCLI_CFG_VAR_INT16S: CmpHelper.Value.Signed = (int32_t) *((int16_t *)Entry->Target); break;
    case SCLI_CFG_VAR_INT32S: CmpHelper.Value.Signed = (int32_t) *((int32_t *)Entry->Target); break;

    case SCLI_CFG_VAR_INT8U:  CmpHelper.Value.Unsigned = (uint32_t) *((uint8_t *)Entry->Target); break;
    case SCLI_CFG_VAR_INT16U: CmpHelper.Value.Unsigned = (uint32_t) *((uint16_t *)Entry->Target); break;
    case SCLI_CFG_VAR_INT32U: CmpHelper.Value.Unsigned = (uint32_t) *((uint32_t *)Entry->Target); break;

    default:
      return -1;
    }
    if(CmpHelper.Value.Signed == StrCmpTable->Value.Signed
        || CmpHelper.Value.Unsigned == StrCmpTable->Value.Unsigned)
      break;
    StrCmpTable++;
  }
  if(StrCmpTable->Str == 0)
  {
    return -1;
  }
  if(Verbose)
  {
    printf(" %s %s %s \r\n", (char *)Entry->Name, (char *)StrCmpTable->Str, (char *)Entry->Desc);
  }
  else
  {
    printf("\r\n get %s -> %s\r\n",(char *)Entry->Name, (char *)StrCmpTable->Str);
  }
  return 0;
}

static SCLI_CMD_RET _scliConf_DisplayStr(SCLI_CFG_ENTRY_T * Entry, unsigned int Verbose)
{
  if(Verbose)
  {
    printf(" %s %s %s \r\n", (char *)Entry->Name, (char *)Entry->Target, (char *)Entry->Desc);
  }
  else
  {
    printf("\r\n get %s -> %s\r\n",(char *)Entry->Name, (char *)Entry->Target);
  }
  return 0;
}

static void _scliConf_PrintConfig(SCLI_CFG_HANDLE_T *Handle)
{
  printf("\r\n Configuration Entry %s: %s\r\n", Handle->Name, Handle->Desc);
  SCLI_CFG_ENTRY_T *Entry = Handle->Table;
  while(Entry->Type != SCLI_CFG_VAR_NONE)
  {
    switch(Entry->ParseMethod)
    {
    case SCLI_CFG_PARSE_INT:
      _scliConf_DisplayInt(Entry, 1);
      break;
    case SCLI_CFG_PARSE_STR_CMP:
      _scliConf_DisplayStrTbl(Entry, 1);
      break;
    case SCLI_CFG_PARSE_STR_PLAIN:
      _scliConf_DisplayStr(Entry, 1);
      break;
    default:
      printf("\r\n Unknown parsing method in config table\r\n");
      break;;
    }
    Entry++;
  }
}

static SCLI_CMD_RET _scliConf_ConfStore(uint8_t argc, char **argv)
{
  return 0;
}

static SCLI_CMD_RET _scliCore_ConfLoad(uint8_t argc, char **argv)
{
  return 0;
}

static SCLI_CMD_RET _scliConf_ConfShow(uint8_t argc, char **argv)
{
  SCLI_CFG_HANDLE_T * CurrentHandle = _scliConf_HandleHead;
  uint32_t i = 0;

  if(argc == 1)
  {
    while(CurrentHandle)
    {
      _scliConf_PrintConfig(CurrentHandle);

      CurrentHandle = CurrentHandle->Next;
    }
  }
  else
  {
    for(i=1; i < argc; i++)
    {
      CurrentHandle = _scliConf_LookupHandle(argv[i]);
      if(CurrentHandle)
      {
        _scliConf_PrintConfig(CurrentHandle);
      }
      else
      {
        printf("\r\n No entry for %s\r\n",argv[i]);
      }
    }
  }
  return 0;
}

SCLI_CFG_HANDLE_T * scliConf_RegisterConfig(const char *const Name, const char *const Desc, SCLI_CFG_ENTRY_T *Table)
{
  SCLI_CFG_HANDLE_T *NewHandle = _scliConf_GetFreeConfigSlot();

  if(NewHandle == (SCLI_CFG_HANDLE_T *)0)
    return NewHandle;

  NewHandle->Name  = (char *)Name;
  NewHandle->Desc  = (char *)Desc;
  NewHandle->Table = Table;
  NewHandle->Next  = ((SCLI_CFG_HANDLE_T *)0);

  if(_scliConf_HandleHead == (SCLI_CFG_HANDLE_T *)0)
  {
    _scliConf_HandleHead = NewHandle;
    NewHandle->Prev  = ((SCLI_CFG_HANDLE_T *)0);
  }
  else
  {
    SCLI_CFG_HANDLE_T *CurrentHandle = _scliConf_HandleHead;

    while(CurrentHandle->Next != (SCLI_CFG_HANDLE_T *)0)
      CurrentHandle = CurrentHandle->Next;

    CurrentHandle->Next = NewHandle;
    NewHandle->Prev = CurrentHandle;

  }
  return NewHandle;
}

SCLI_CMD_RET scliConf_SetCmd(uint8_t argc, char **argv)
{
  SCLI_CMD_RET Ret = 0;

  if(_scliConf_HandleHead == (SCLI_CFG_HANDLE_T *)0)
  {
    printf("%s",scliText_ConfNoTable);
    return -1;
  }

  if(argc != 3)
  {
    printf("\r\n Usage: set <ident> <value>\r\n");
    return -1;
  }

  SCLI_CFG_ENTRY_T *Entry = _scliConf_LookupEntry(argv[1]);

  if(Entry == (SCLI_CFG_ENTRY_T *)0)
  {
    printf("\r\n No match found for %s\r\n",argv[1]);
    return -1;
  }

  switch(Entry->ParseMethod)
  {
  case SCLI_CFG_PARSE_INT:
    Ret =  _scliConf_ParseInt(Entry ,argv[2]);
    break;
  case SCLI_CFG_PARSE_STR_CMP:
    Ret =  _scliConf_ParseStrTbl(Entry ,argv[2]);
    break;
  case SCLI_CFG_PARSE_STR_PLAIN:
    Ret =  _scliConf_ParseStr(Entry ,argv[2]);
    break;
  default:
    printf(" Unknown parsing method in config table\r\n");
    return -1;
  }

  if(Ret < 0)
  {
    printf(" Failed to set %s %s\r\n",(char *)Entry->Name, argv[2]);
  }
  else
  {
    printf("\r\n Done!\r\n");
  }
  return Ret;

}

SCLI_CMD_RET scliConf_GetCmd(uint8_t argc, char **argv)
{
  if(_scliConf_HandleHead == (SCLI_CFG_HANDLE_T *)0)
  {
    printf("%s",scliText_ConfNoTable);
    return -1;
  }

  if(argc != 2)
  {
    printf("\r\n Usage: get <ident>\r\n");
    return -1;
  }

  SCLI_CFG_ENTRY_T *Entry = _scliConf_LookupEntry(argv[1]);

  if(Entry == (SCLI_CFG_ENTRY_T *)0)
  {
    printf("\r\n No match found for %s\r\n",argv[1]);
    return -1;
  }

  switch(Entry->ParseMethod)
  {
  case SCLI_CFG_PARSE_INT:
    return _scliConf_DisplayInt(Entry,0);
  case SCLI_CFG_PARSE_STR_CMP:
    return _scliConf_DisplayStrTbl(Entry,0);
  case SCLI_CFG_PARSE_STR_PLAIN:
    return _scliConf_DisplayStr(Entry,0);
  default:
    printf("\r\n Unknown parsing method in config table\r\n");
    return -1;
  }

  return 0;

}

SCLI_CMD_RET scliConf_ConfCmd(uint8_t argc, char **argv)
{
  uint8_t PrintSubcommands = 0;
  SCLI_CMD_RET Ret = 0;

  if(_scliConf_HandleHead == (SCLI_CFG_HANDLE_T *)0)
  {
    printf("%s",scliText_ConfNoTable);
    return -1;
  }

  if(argc == 1)
  {
    PrintSubcommands = 1;
  }
  else
  {
    SCLI_CMD_T * Cmd = scliCore_GetCommandInTable(argv[1], _scliConf_ConfSubcommands,strlen(argv[1]));
    if(Cmd)
    {
      Ret = Cmd->CmdCallback(argc - 1, &argv[1] );
    }
    else
    {
      printf("\r\n Unknown subcommand!");
      PrintSubcommands = 1;
      Ret = -1;
    }
  }

  if(PrintSubcommands)
  {
    printf("\r\n Available subcommands:\r\n");
    scliCore_PrintHelp(_scliConf_ConfSubcommands, 0);
  }

  return Ret;
}

#endif
