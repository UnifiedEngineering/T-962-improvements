#ifndef REFLOW_PROFILES_H_
#define REFLOW_PROFILES_H_

// Number of temperature settings in a reflow profile
#define NUMPROFILETEMPS (48)

#define YAXIS (57)
#define XAXIS (12)

typedef struct {
	const char* name;
	const uint16_t temperatures[NUMPROFILETEMPS];
} profile;

typedef struct {
	const char* name;
	uint16_t temperatures[NUMPROFILETEMPS];
} ramprofile;

void Reflow_LoadCustomProfiles(void);
void Reflow_ValidateNV(void);
void Reflow_PlotProfile(int highlight);

int Reflow_GetProfileIdx(void);
int Reflow_SelectProfileIdx(int idx);
int Reflow_SelectEEProfileIdx(int idx);

int Reflow_GetEEProfileIdx(void);
int Reflow_SaveEEProfile(void);
void Reflow_ListProfiles(void);
const char* Reflow_GetProfileName(void);
uint16_t Reflow_GetSetpointAtIdx(uint8_t idx);
void Reflow_SetSetpointAtIdx(uint8_t idx, uint16_t value);
void Reflow_DumpProfile(int profile);

#endif /* REFLOW_PROFILES_H */
