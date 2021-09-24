#ifndef IO_H_
#define IO_H_

#define IAP_READ_PART (54)
#define IAP_REINVOKE_ISP (57)
#define PART_REV_ADDR (0x0007D070)

typedef struct {
    const char* name;
    const uint32_t id;
} partmapStruct;

void Set_Heater(uint8_t enable);
void Set_Fan(uint8_t enable);
void IO_InitWatchdog(void);
void IO_PrintResetReason(void);
int IO_Partinfo(char* buf, int n, char* format);
void IO_JumpBootloader(void);
void IO_Init(void);
#endif /* IO_H_ */
