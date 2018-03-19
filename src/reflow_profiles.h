#ifndef REFLOW_PROFILES_H_
#define REFLOW_PROFILES_H_

#include <stdbool.h>

// Number of temperature settings in a reflow profile
#define NUMPROFILETEMPS 48

typedef struct {
	const char* name;
	const uint16_t temperatures[NUMPROFILETEMPS];
} profile;

typedef struct {
	const char* name;
	uint16_t temperatures[NUMPROFILETEMPS];
} ramprofile;

void Reflow_InitNV(void);

int Reflow_NoOfProfiles(void);
int Reflow_GetProfileIdx(void);
int Reflow_SelectProfileIdx(int idx);
bool Reflow_IdxIsInEEPROM(int idx);
int Reflow_SaveEEProfile(void);
const char* Reflow_GetProfileName(int idx);
void Reflow_SetProfileName(int idx, const char*);
uint16_t Reflow_GetSetpointAtIdx(uint8_t idx);
uint16_t Reflow_GetSetpointAtTime(uint32_t time);
void Reflow_SetSetpointAtIdx(uint8_t idx, uint16_t value);

#endif /* REFLOW_PROFILES_H */
