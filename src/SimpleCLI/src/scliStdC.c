/*
 * scliStdC.c
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

#include "SimpleCLI/inc/scliStdC.h"

#if SCLI_USE_OWN_STDC_FUNC > 0

uint32_t scliStdC_strlen(char *str)
{
  uint32_t i = 0;

  while(str[i])
    i++;

  return i;
}

int scliStdC_strncmp(char * str1,char *str2,size_t Len)
{
  size_t i = 0;

  for(i=0; i < Len; i++)
  {
    if(str1[i] != str2[i])
      return -1;
  }

  return 0;
}
void scliStdC_memset(void* buf, int8_t Val,size_t Len)
{
  int8_t *target = (int8_t *)buf;
  size_t i = 0;

  for(i=0; i < Len; i++)
    target[i] = Val;

}

char *scliStdC_strncpy(char * dst, const char * src, size_t n)
{
  size_t i = 0;
  for(i = 0; i < n; i++)
  {
    dst[i] = src[i];
    if(dst[i] == '\0')
      break;
  }
  return(dst);
}

uint32_t scliStdC_strtoul(char *str, char **endPtr, int base)
{
  uint32_t res = 0;

  if( base != 2  &&
      base != 8  &&
      base != 10 &&
      base != 16)
    return 0;

  /*
   * Skip any leading blanks.
   */
  while(*str == ' ')
    str++;

  /*
   * Skip +
   */
  if(*str == '+')
    str++;

  while(*str)
  {
    uint32_t value = 0;
    switch(*str)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      value = *str - '0';
      break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
      value = *str - 'A' + 10;
      break;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      value = *str - 'a' + 10;
      break;
    default:
      value = 255;
      break;
    }

    if(value > (base-1))
      break;

    res = value + res*base;
    str++;
  }

  if(endPtr)
    *endPtr = str;

  return res;

}

int32_t scliStdC_strtol(char *str, char **endPtr, int base)
{
  int32_t sign = 1;
  int32_t result = 0;
  char * start = str;

  /*
   * Skip any leading blanks.
   */
  while(*str == ' ')
    str++;

  /*
   * Check for a sign.
   */
  switch(*str)
  {
  case '-':
    sign = -1;
    str++;
    break;
  case '+':
    sign = 1;
    str++;
    break;
  default:
    break;
  }

  result = sign*scliStdC_strtoul(str, endPtr, base);

  if ((result == 0) && (endPtr != 0) && (*endPtr == str)) {
    *endPtr = start;
  }
  return result;
}

#endif
