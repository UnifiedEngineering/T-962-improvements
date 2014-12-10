#ifndef NVSTORAGE_H_
#define NVSTORAGE_H_

// Right now we only have 28 bytes in total for NV storage, and 3 of those bytes
// are used up for housekeeping so max 25 items will fit
typedef enum eNVItem { // Only append to the end of this list to avoid backwards incompatibilities
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

#endif /* NVSTORAGE_H_ */
