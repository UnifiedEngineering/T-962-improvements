/*
 * scliCore.c
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

#include "SimpleCLI/inc/scliCore.h"
#include "SimpleCLI/inc/scliStdC.h"
#include "SimpleCLI/inc/scliConf.h"

/*
 * Static variables
 */
static SCLI_CMD_INPUT_T   _scliCore_InputBuffer[SCLI_CMD_HIST];
static SCLI_BUF_IDX       _scliCore_CurBufferIdx = 0;
static SCLI_BUF_IDX       _scliCore_CmdHist[SCLI_CMD_HIST];
static SCLI_BUF_IDX       _scliCore_CmdHistHead = 0;
static SCLI_BUF_IDX       _scliCore_CmdHistCurIdx = 0;
static SCLI_INTERFACE_T*  _scliCore_Interface = 0;
static SCLI_FIFO_T        _scliCore_GetCharBuffer;
static SCLI_BUF_IDX       _scliCore_CmdHistWrapArround = 0;

/*
 * Forward declaration for internal commands
 */
static SCLI_CMD_RET _scliCore_HelpCmd(uint8_t argc, char **argv);
static SCLI_CMD_RET _scliCore_VersionCmd(uint8_t argc, char **argv);
static SCLI_CMD_RET _scliCore_HistoryCmd(uint8_t argc, char **argv);

/*
 * Internal command table
 */
static SCLI_CMD_T _scliCore_BasicCommands[] =
{
    {_scliCore_HelpCmd,     "help",     "Displays help text",     scliText_Help},
    {_scliCore_VersionCmd,  "version",  "Displays version text",  scliText_Version},
    {_scliCore_HistoryCmd,  "history",  "Displays history",       scliText_Version},
#if SCLI_USE_CFG_SYSTEM > 0
    {scliConf_SetCmd,       "set",      "Set system variable",    scliText_Set},
    {scliConf_GetCmd,       "get",      "Get system variable",    scliText_Get},
    {scliConf_ConfCmd,      "conf",     "Configuration overview", scliText_Conf},
#endif
    SCLI_CMD_LIST_END
};

/*
 * Print prompt
 */
static inline void _scliCore_PrintPrompt(void)
{
  printf("\r\n%s%s%s",scliText_ColDefBold, scliText_Prompt, scliText_ColDef);
}

/*
 * Print welcome message
 */
static inline void _scliCore_PrintWelcome(void)
{
  printf("\r\n%s",scliText_WelcomeMsg);
}

/*
 * Return next free input buffer
 */
static SCLI_BUF_IDX _scliCore_GetNextCmdBuffer(SCLI_BUF_IDX CurIdx)
{
  SCLI_BUF_IDX i = 0;
  SCLI_BUF_IDX LookupIdx = 0;
  for(i = 1; i < (SCLI_CMD_HIST - 1); i++)
  {
    LookupIdx = (i + CurIdx) % (SCLI_CMD_HIST - 1);
    if(_scliCore_InputBuffer[LookupIdx].HistRefs == 0)
        return LookupIdx;
  }
  return LookupIdx;
}

/*
 * help command
 * print helptext of a command
 */
static SCLI_CMD_RET _scliCore_HelpCmd(uint8_t argc, char **argv)
{
  SCLI_CMD_T * Cmd;
  /*
   * No additional parameter
   * -> print short help text for all
   *    available commands
   */
  if(argc == 1)
  {
    SCLI_CMD_T * Table = _scliCore_Interface->UserLevelCommands;

    if(Table != _scliCore_BasicCommands)
    {
      printf("\r\n User defined commands:\r\n");
      scliCore_PrintHelp(Table,0);
    }
    Table = _scliCore_BasicCommands;
    printf("\r\n Internal commands:\r\n");

    scliCore_PrintHelp(Table,0);

    return 0;
  }
  /*
   * print command usage, if more than
   * one parameter was passed
   */
  else if(argc != 2)
  {
    printf("\r\n%s",scliText_Help);
    return -1;
  }
  /*
   * Get length of passed parameter
   */
  uint32_t Length = strlen(argv[1]);

  /*
   * Perform lookup in user level command table
   * -> print long help text
   */
  if((Cmd = scliCore_GetCommandInTable(argv[1], _scliCore_Interface->UserLevelCommands,Length)))
  {
    scliCore_PrintHelp(Cmd,1);

    return 0;
  }

  /*
   * Nothing found, perform
   * lookup with internal command table
   * -> print long help text
   */
  if((Cmd = scliCore_GetCommandInTable(argv[1], _scliCore_BasicCommands,Length)))
  {
    printf("\r\n%s\r\n",Cmd->HelpLong);
    return 0;
  }

  /*
   * No command found
   */
  printf("\r\n Can't find cmd \"%s\" in command list\r\n", argv[1]);

  return -1;
}

/*
 * version command
 * print current version
 */
static SCLI_CMD_RET _scliCore_VersionCmd(uint8_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
  printf("\r\n%s\r\n",scliText_Version);
  return 0;
}

/*
 * history command
 * print command history
 */
static SCLI_CMD_RET _scliCore_HistoryCmd(uint8_t argc __attribute__((unused)), char **argv __attribute__((unused)))
{
  SCLI_BUF_IDX i = 0;
  SCLI_BUF_IDX LookupIdx = 0;

  printf("\r\n Command history"
         "\r\n  Current commands in history: %4d"
         "\r\n  Maximum history buffer:      %4d"
         "\r\n"
         "\r\n   Id Buffer Command\r\n", _scliCore_CmdHistWrapArround, SCLI_CMD_HIST);

  for(i = 1; i < _scliCore_CmdHistWrapArround; i++)
  {
    LookupIdx = (_scliCore_CmdHistHead > i) ? _scliCore_CmdHistHead - i :
        SCLI_CMD_HIST + _scliCore_CmdHistHead - i;

    printf(" %4d   %4d %s\r\n",i ,_scliCore_CmdHist[LookupIdx], _scliCore_InputBuffer[_scliCore_CmdHist[LookupIdx]].Line);
  }

  printf("\r\n");
  return 0;
}

/*
 * Tabcompletion
 */
static void _scliCore_TabCompletion(SCLI_CMD_INPUT_T *InputBuffer)
{
  SCLI_CMD_T * Table = _scliCore_Interface->UserLevelCommands;
  SCLI_CMD_T * LastCmd;
  uint32_t CommandsFound = 0;
  /*
   * Search inside the user defined command table
   */
  while(Table->CmdCallback != (SCLI_CMD_CB)0)
  {
    if(strncmp(InputBuffer->Line,(char *)Table->CmdName,InputBuffer->LineIdx) == 0)
    {
      /*
       * Match found, print newline (for the first command)
       * print possible command
       */
      if(!CommandsFound)
        printf("\r\n");

      printf("%s ",Table->CmdName);
      LastCmd = Table;
      CommandsFound++;
    }
    Table++;
  }
  /*
   * Do the same for the internal command table
   */
  if(Table != _scliCore_BasicCommands)
  {
    Table = _scliCore_BasicCommands;
    while(Table->CmdCallback != (SCLI_CMD_CB)0)
    {
      if(strncmp(InputBuffer->Line,(char *)Table->CmdName,InputBuffer->LineIdx) == 0)
      {
        /*
         * Match found, print newline (for the first command)
         * print possible command
         */
        if(!CommandsFound)
          printf("\r\n");

        printf("%s ",Table->CmdName);
        LastCmd = Table;
        CommandsFound++;
      }
      Table++;
    }
  }
  /*
   * Print current prompt in a new line
   */
  if(CommandsFound)
  {
    if(CommandsFound == 1)
    {
      /*
       * Complete command automatically
       * if only one command was found
       */
      InputBuffer->LineIdx = sprintf(InputBuffer->Line,"%s",LastCmd->CmdName);
    }
    _scliCore_PrintPrompt();
    printf("%s",InputBuffer->Line);
  }
}

static SCLI_CMD_T *  _scliCore_CommandLookup(SCLI_CMD_INPUT_T *InputBuffer)
{
  SCLI_LINE_IDX i = 0;
  SCLI_CMD_T * Cmd;
  /*
   * Get name of cmd
   */
  for(i = 0; i < InputBuffer->LineIdx; i++)
  {
    if(InputBuffer->Line[i] == ' ')
      break;
  }

  /*
   * Lookup in user defined commands
   */
  if((Cmd = scliCore_GetCommandInTable(InputBuffer->Line, _scliCore_Interface->UserLevelCommands,i)))
    return Cmd;

  /*
   * Lookup in user defined commands
   */
  if((Cmd = scliCore_GetCommandInTable(InputBuffer->Line, _scliCore_BasicCommands,i)))
    return Cmd;

  return (SCLI_CMD_T *)0;
}

/*
 * Add a char to the getch buffer,
 * overwrite oldest entry if the buffer is full
 */
static void _scliCore_PushGetCh(char ch)
{
  uint32_t psr = _scliCore_Interface->CriticalEnter();

  if(_scliCore_GetCharBuffer.Size < SCLI_GETCH_BUF_SIZE)
  {
    _scliCore_GetCharBuffer.Size++;
  }
  else
  {
    _scliCore_GetCharBuffer.Read = (_scliCore_GetCharBuffer.Read + 1) % SCLI_GETCH_BUF_SIZE;
  }
  _scliCore_GetCharBuffer.Buffer[_scliCore_GetCharBuffer.Write] = ch;
  _scliCore_GetCharBuffer.Write = (_scliCore_GetCharBuffer.Write + 1) % SCLI_GETCH_BUF_SIZE;

  _scliCore_Interface->CriticalExit(psr);
}

/*
 * Pop char from getch buffer,
 * return SCLI_FIFO_EMPTY (EOF) if the FIFO is empty
 */
static int _scliCore_PopGetCh(void)
{
  char ch = 0;

  if(_scliCore_GetCharBuffer.Size == 0)
    return SCLI_FIFO_EMPTY;

  uint32_t psr = _scliCore_Interface->CriticalEnter();
  ch = _scliCore_GetCharBuffer.Buffer[_scliCore_GetCharBuffer.Read];
  _scliCore_GetCharBuffer.Read = (_scliCore_GetCharBuffer.Read + 1) % SCLI_GETCH_BUF_SIZE;
  _scliCore_GetCharBuffer.Size--;
  _scliCore_Interface->CriticalExit(psr);

  return (int)ch;
}

/*
 * Flush getch buffer
 */
static void _scliCore_FlushGetCh(void)
{
  _scliCore_GetCharBuffer.Size = 0;
  _scliCore_GetCharBuffer.Read = _scliCore_GetCharBuffer.Write;
}

/*
 * Input FSM for character parsing
 */
static void _scliCore_Input(uint8_t ch)
{
  /*
   * Do character check inside a critical
   * region, with deactivated IRQs
   */
  uint32_t psr = _scliCore_Interface->CriticalEnter();
  SCLI_CMD_INPUT_T *CurInputBuffer = &_scliCore_InputBuffer[_scliCore_CurBufferIdx];
  switch(CurInputBuffer->State)
  {
  /*
   * Default character parsing
   */
  case SCLI_BUF_READY:
    switch(ch)
    {
    case 0x03:
      /*
       * STRG-C
       */
      scliCore_ReleaseBuffer(CurInputBuffer);
      _scliCore_PrintPrompt();
      break;
    case '\e':
      /*
       * ESC -> start esc seq. parsing for the next ch
       */
      CurInputBuffer->State = SCLI_BUF_ESC_CLI;
      break;
    case '\n':
      /*
       * Skip
       */
      break;
    case '\t':
      /*
       * Call tabcompletion
       */
      _scliCore_TabCompletion(CurInputBuffer);
      break;
    case '\b':
      /*
       * Backspace
       */
      if(CurInputBuffer->LineIdx)
      {
        CurInputBuffer->LineIdx--;
        CurInputBuffer->Line[CurInputBuffer->LineIdx] = '\0';
        printf("\r%s%s%s%s \b",scliText_ColDefBold, scliText_Prompt, scliText_ColDef, CurInputBuffer->Line);
      }
      break;
    case '\r':
      /*
       * Received ENTER,
       * -> call user level
       * -> set buffer state to SCLI_BUF_PARSING
       */
      CurInputBuffer->State = SCLI_BUF_PARSING;
      _scliCore_Interface->UserLevelHandOver(CurInputBuffer);
      _scliCore_CmdHistWrapArround = (_scliCore_CmdHistWrapArround < SCLI_CMD_HIST) ?
          _scliCore_CmdHistWrapArround + 1 : SCLI_CMD_HIST;
      break;
    default:
      /*
       * Add any other ch to the input buffer
       */
      CurInputBuffer->Line[CurInputBuffer->LineIdx] = ch;
      CurInputBuffer->LineIdx++;
      _scliCore_Interface->PutCh(ch);
      break;
    }
    break;
    /*
     * Wait for '['
     */
    case SCLI_BUF_ESC_CLI:
      if(ch != '[')
      {
        CurInputBuffer->State = SCLI_BUF_READY;
      }
      else
      {
        CurInputBuffer->State = SCLI_BUF_ESC_CMD;
      }
      break;
    /*
     * Parse ESC sequence character
     */
    case SCLI_BUF_ESC_CMD:
    {
      switch(ch)
      {
      /*
       * Arrow up
       */
      case 'A':
        _scliCore_CmdHistCurIdx = (_scliCore_CmdHistCurIdx > 0) ?
            _scliCore_CmdHistCurIdx - 1 : _scliCore_CmdHistWrapArround - 1;
        break;
      /*
       * Arrow down
       */
      case 'B':
        _scliCore_CmdHistCurIdx = (_scliCore_CmdHistCurIdx < (_scliCore_CmdHistWrapArround - 1)) ?
            _scliCore_CmdHistCurIdx + 1 : 0;
        break;
      default:
        return;
      }
      /*
       * Use buffer from history as new input buffer
       */
      _scliCore_CurBufferIdx = _scliCore_CmdHist[_scliCore_CmdHistCurIdx];
      _scliCore_CmdHist[_scliCore_CmdHistHead] = _scliCore_CurBufferIdx;
      CurInputBuffer->State = SCLI_BUF_READY;
      CurInputBuffer = &_scliCore_InputBuffer[_scliCore_CurBufferIdx];
      CurInputBuffer->State = SCLI_BUF_READY;
      CurInputBuffer->Line[CurInputBuffer->LineIdx] = '\0';
      printf("\r%s%s%s%s%s",scliText_ColDefBold, scliText_Prompt, scliText_ColDef, CurInputBuffer->Line,scliText_DeleteRight);
      break;
    }
    case SCLI_BUF_EXEC:
      /*
       * During command execution,
       * pass ch to getch buffer
       */
      _scliCore_PushGetCh(ch);
      break;
    case SCLI_BUF_PARSING:
      /*
       * Skip ch during command parsing
       */
    default:
      break;
  }
  _scliCore_Interface->CriticalExit(psr);
}

void scliCore_ReleaseBuffer(SCLI_CMD_INPUT_T *InputBuffer)
{
  /*
   * Add last buffer to command history,
   * increment history reference counter
   * and history head/idx
   */
  _scliCore_CmdHist[_scliCore_CmdHistHead] = _scliCore_CurBufferIdx;
  InputBuffer->HistRefs++;
  _scliCore_CmdHistHead = (_scliCore_CmdHistHead + 1) % SCLI_CMD_HIST;
  _scliCore_CmdHistCurIdx = _scliCore_CmdHistHead;

  /*
   * Set current buffer to the next input buffer
   * inside our history
   */
  _scliCore_CurBufferIdx = _scliCore_CmdHist[_scliCore_CmdHistHead];
  InputBuffer = &_scliCore_InputBuffer[_scliCore_CurBufferIdx];

  /*
   * Decrement history reference counter and check if zero
   * if HistRefs != 0 we can't use this buffer, as another
   * reference to this buffer exists inside our history
   * ->call _scliCore_GetNextCmdBuffer
   */
  InputBuffer->HistRefs = (InputBuffer->HistRefs) ? InputBuffer->HistRefs - 1 : 0;
  if(InputBuffer->HistRefs)
  {
    _scliCore_CurBufferIdx = _scliCore_GetNextCmdBuffer(_scliCore_CurBufferIdx);
    InputBuffer = &_scliCore_InputBuffer[_scliCore_CurBufferIdx];
  }

  /*
   * Reset our new buffer to the default state
   */
  InputBuffer->State = SCLI_BUF_READY;
  InputBuffer->LineIdx = 0;
}

/*
 * Parse input buffer and execute command
 */
void scliCore_ExecuteCommand(SCLI_CMD_INPUT_T *InputBuffer)
{
  SCLI_LINE_IDX i = 0;
  SCLI_CMD_RET ret = 0;
  uint8_t argc = 0;
  char *argv[SCLI_CMD_MAX_ARGS];
  /*
   * Init argc & argv with
   * command entry
   */
  argv[0] = &(InputBuffer->Line[0]);
  argc = 1;

  /*
   * Empty command, return new prompt
   */
  if(InputBuffer->LineIdx == 0)
  {
    _scliCore_PrintWelcome();
    _scliCore_PrintPrompt();
    InputBuffer->State = SCLI_BUF_READY;
    return;
  }

  /*
   * Command lookup insideall command
   * tables
   */
  SCLI_CMD_T* Cmd = _scliCore_CommandLookup(InputBuffer);

  /*
   * Command not found
   */
  if(Cmd == (SCLI_CMD_T*)0)
  {
    printf("\r\n %sCommand not found%s\r\n",scliText_ColRedBold,scliText_ColDef);
    _scliCore_PrintPrompt();
    scliCore_ReleaseBuffer(InputBuffer);
    return;
  }


  /*
   * Insert \0 for each ' '
   * for str zero termination in argv
   */
  for(i=0; i < InputBuffer->LineIdx; i++)
  {
    if(InputBuffer->Line[i] == ' ')
    {
      InputBuffer->Line[i] = '\0';
      if((i+1) < InputBuffer->LineIdx &&
          InputBuffer->Line[i+1] >= 33 &&
          InputBuffer->Line[i+1] <= 126)
      {
        argv[argc] = &(InputBuffer->Line[i+1]);
        argc++;
      }
    }
  }
  InputBuffer->Line[i] = '\0';

  /*
   * Call function
   */
  InputBuffer->State = SCLI_BUF_EXEC;
  ret=Cmd->CmdCallback(argc,argv);

  /*
   * Revert buffer for
   * call history
   */
  for(i=0; i < InputBuffer->LineIdx; i++)
  {
    if(InputBuffer->Line[i] == '\0')
    {
      InputBuffer->Line[i] = ' ';
    }
  }

  /*
   * Flush get char buffer
   */
  _scliCore_FlushGetCh();

  /*
   * Report error
   */
  if(ret < 0)
  {
    printf("\r\n %s%s returned %d%s\r\n",scliText_ColRedBold, Cmd->CmdName, ret, scliText_ColDef);
  }

  /*
   * Release input buffer
   */
  scliCore_ReleaseBuffer(InputBuffer);

  /*
   * Print new prompt
   */
  _scliCore_PrintPrompt();

  return;
}

/*
 * Print help message
 * either the long version of a single
 * or the short version of multiple entries
 */
void scliCore_PrintHelp(SCLI_CMD_T * Table, uint8_t SingleEntry)
{
  if(SingleEntry)
  {
    if(Table->HelpLong)
      printf("\r\n%s\r\n",Table->HelpLong);

    return;
  }
  else
  {
    while(Table->CmdCallback != (SCLI_CMD_CB)0)
    {
      size_t StrLen = strlen((char *)Table->CmdName);
      size_t i = 0;

      for(i = StrLen; i < SCLI_CMD_NAME_MAX_LEN; i++)
        _scliCore_Interface->PutCh(' ');

      printf("    %s:", Table->CmdName);


      printf(" %s\r\n", Table->HelpShort);
      Table++;
    }
  }
}

/*
 * Perform command lookup,
 * return pointer to command if successful
 */
SCLI_CMD_T * scliCore_GetCommandInTable(char * Cmd, SCLI_CMD_T * Table, SCLI_LINE_IDX CmdLen)
{
  size_t CmdNameLen = 0;
  while(Table->CmdCallback != (SCLI_CMD_CB)0)
  {
    CmdNameLen = strlen((char *)Table->CmdName);
    if(CmdNameLen == CmdLen && strncmp(Cmd,(char *)Table->CmdName,CmdNameLen) == 0)
      return Table;
    Table++;
  }

  return (SCLI_CMD_T *)0;
}

/*
 * Return a char from the input buffer
 * during command execution
 */
int scliCore_getchar(void)
{
  return _scliCore_PopGetCh();
}

/*
 * Init command line subsystem
 */
uint8_t scliCore_Init( SCLI_INTERFACE_T* Interface)
{

  if( Interface->CriticalEnter == 0 || Interface->CriticalExit == 0 ||
      Interface->PutCh == 0 || Interface->SubsystemInit == 0 ||
      Interface->UserLevelHandOver == 0)
    return 0;

  /*
   * Set internal interface
   * pointer
   */
  _scliCore_Interface = Interface;

  /*
   * Check for external
   * cmd table
   */
  if(_scliCore_Interface->UserLevelCommands == 0)
    _scliCore_Interface->UserLevelCommands = _scliCore_BasicCommands;

  /*
   * Call init function for
   * hardware and printf init
   */
  _scliCore_Interface->SubsystemInit(_scliCore_Input);

  /*
   * Init internal input & get char buffer
   */
  memset(_scliCore_InputBuffer,0,sizeof(_scliCore_InputBuffer));
  memset(&_scliCore_GetCharBuffer,0,sizeof(_scliCore_GetCharBuffer));
  _scliCore_FlushGetCh();

  /*
   * Print welcome message & prompt
   */
  _scliCore_PrintWelcome();
  _scliCore_PrintPrompt();

  /*
   * Release buffers
   */
  SCLI_BUF_IDX i = 0;
  uint32_t psr = _scliCore_Interface->CriticalEnter();

  for(i = 0; i < SCLI_CMD_HIST; i++)
  {
    _scliCore_InputBuffer[i].State = SCLI_BUF_READY;
    _scliCore_InputBuffer[i].LineIdx = 0;
    _scliCore_InputBuffer[i].HistRefs = 0;
    _scliCore_CmdHist[i] = i;
  }
  _scliCore_CmdHistHead = 0;
  _scliCore_CmdHistCurIdx = _scliCore_CmdHistHead;

  _scliCore_Interface->CriticalExit(psr);

  return 1;
}
