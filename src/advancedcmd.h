#ifndef ADVANCEDCMD_H_
#define ADVANCEDCMD_H_
#include "circbuffer.h"

#define ADVCMD_SYNC1_LOC 0
#define ADVCMD_SYNC2_LOC 1
#define ADVCMD_SIZE_LOC  2
#define ADVCMD_OVERHEAD  4
#define ADVCMD_ACK 0x85

typedef enum eAdvancedCMDs {
	SetEEProfileCmd = 0xEE
	//add more advcmds here...
} advcmd_t;

typedef struct {
	uint8_t cmdSize;
	uint8_t cmd;
	uint8_t data[256];
} advancedSerialCMD;

typedef struct {
	uint8_t profileNum;
	uint16_t tempData[48];
} EEProfileCMD;

uint8_t chkAdvCmd(tcirc_buf* buf, advancedSerialCMD* cmd);
uint8_t calcCkSum(tcirc_buf* buf, int length);

#endif /*ADVANCEDCMD_H_*/