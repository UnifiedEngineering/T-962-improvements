#ifndef NVSTORAGE_H_
#define NVSTORAGE_H_

// some configuration items, never more than 20, see nvstorage.c
typedef enum eNVItem {
	REFLOW_BEEP_DONE_LEN=0,
	REFLOW_PROFILE=1,
	TC_LEFT_GAIN,
	TC_LEFT_OFFSET,
	TC_RIGHT_GAIN,
	TC_RIGHT_OFFSET,
	REFLOW_MIN_FAN_SPEED,
	NVITEM_NUM_ITEMS // Last value
} NVItem_t;

void NV_Init(void);
uint8_t NV_GetConfig(NVItem_t item);
void NV_SetConfig(NVItem_t item, uint8_t value);
int32_t NV_Work(void);
int NV_NoOfProfiles(void);
// interface for EEProm profiles
uint16_t NV_GetSetpoint(uint8_t, uint8_t);
void NV_SetSetpoint(uint8_t, uint8_t, uint16_t);
char *NV_GetProfileName(uint8_t);
void NV_SetProfileName(uint8_t, const char *);
int NV_StoreProfile(uint8_t);

#endif /* NVSTORAGE_H_ */
